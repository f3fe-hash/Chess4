#!/usr/bin/env python3
"""
Lichess <-> TCP UCI bridge for a custom chess engine.

Architecture:

    Lichess Bot API
          |
          v
    lichess_bridge.py
       |          |
       v          v
   Game A       Game B       ... one worker per game
       |          |
       v          v
   UCI TCP      UCI TCP
       |          |
       +----------+
             |
      C++ UCI server

Requirements:
    python3 -m pip install requests

Environment:
    export LICHESS_TOKEN="..."

The C++ UCI server must accept multiple simultaneous TCP clients.
Each Lichess game gets its own TCP connection.

The bridge uses the Lichess move list as the authoritative position.
This means restarting the bridge in the middle of a game is safe:
the current game state is reconstructed from Lichess instead of from
local state.
"""

from __future__ import annotations

import json
import os
import socket
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass, field
from typing import Optional

import requests
import chess

try:
    from .chat import GroqChat
except ImportError:
    from chat import GroqChat


# ============================================================
# Configuration
# ============================================================

LICHESS_API = "https://lichess.org"

LICHESS_TOKEN = os.environ.get("LICHESS_TOKEN")
GROQ_API_KEY = os.environ.get("GROQ_API_KEY")

UCI_HOST = os.environ.get("UCI_HOST", "127.0.0.1")
UCI_PORT = int(os.environ.get("UCI_PORT", "8080"))

# Maximum number of games this process will play simultaneously.
MAX_GAMES = 32

# Challenge policy.
AUTO_ACCEPT_CHALLENGES = True

# Set these according to what you want your bot to accept.
ALLOW_RATED = True
ALLOW_CASUAL = True

# Keep a little time in reserve to avoid losing on time.
MOVE_OVERHEAD_MS = 250

# Never ask the engine to use more than this much time for one move.
# Set to None for no artificial cap.
MAX_MOVE_TIME_MS: Optional[int] = None

# Retry delay for transient errors.
RECONNECT_DELAY_SECONDS = 5

# Games that have not had a move for this long are considered stale.
# 24 hours (1 day)
STALE_GAME_SECONDS = 24 * 60 * 60


# ============================================================
# Logging
# ============================================================

print_lock = threading.Lock()


def log(message: str) -> None:
    with print_lock:
        print(message, flush=True)


def log_error(message: str) -> None:
    with print_lock:
        print(message, file=sys.stderr, flush=True)


# ============================================================
# Lichess API
# ============================================================

class LichessAPI:
    def __init__(self, token: str):
        self.session = requests.Session()
        self.session.headers.update({
            "Authorization": f"Bearer {token}",
            "User-Agent": "CustomUCIChessBot/1.0",
        })

    def get_account(self) -> dict:
        response = self.session.get(
            f"{LICHESS_API}/api/account",
            timeout=30,
        )
        response.raise_for_status()
        return response.json()

    def stream_events(self):
        """
        Global account event stream.

        Lichess sends newline-delimited JSON.
        """
        response = self.session.get(
            f"{LICHESS_API}/api/stream/event",
            stream=True,
            timeout=(30, None),
        )
        response.raise_for_status()

        try:
            for line in response.iter_lines():
                if not line:
                    continue

                yield json.loads(line)
        finally:
            response.close()

    def stream_game(self, game_id: str):
        """
        Stream one game's events.

        The first useful event is normally gameFull, followed by
        gameState events.
        """
        response = self.session.get(
            f"{LICHESS_API}/api/bot/game/stream/{game_id}",
            stream=True,
            timeout=(30, None),
        )
        response.raise_for_status()

        try:
            for line in response.iter_lines():
                if not line:
                    continue

                yield json.loads(line)
        finally:
            response.close()

    def accept_challenge(self, challenge_id: str) -> None:
        response = self.session.post(
            f"{LICHESS_API}/api/challenge/{challenge_id}/accept",
            timeout=30,
        )
        response.raise_for_status()

    def decline_challenge(self, challenge_id: str) -> None:
        response = self.session.post(
            f"{LICHESS_API}/api/challenge/{challenge_id}/decline",
            timeout=30,
        )
        response.raise_for_status()

    def make_move(self, game_id: str, move: str) -> None:
        response = self.session.post(
            f"{LICHESS_API}/api/bot/game/{game_id}/move/{move}",
            timeout=30,
        )
        response.raise_for_status()

    def abort_game(self, game_id: str) -> None:
        response = self.session.post(
            f"{LICHESS_API}/api/bot/game/{game_id}/abort",
            timeout=30,
        )
        response.raise_for_status()

    def send_chat(
        self,
        game_id: str,
        text: str,
        room: str = "player",
    ) -> None:
        response = self.session.post(
            f"{LICHESS_API}/api/bot/game/{game_id}/chat",
            data={"room": room, "text": text},
            timeout=30,
        )
        response.raise_for_status()


# ============================================================
# TCP UCI client
# ============================================================

class UCIClient:
    """
    One TCP connection to one engine instance/session.

    A separate UCIClient is created for every active Lichess game.
    """

    def __init__(self, host: str, port: int, game_id: str):
        self.host = host
        self.port = port
        self.game_id = game_id
        self.sock: Optional[socket.socket] = None
        self.file = None
        self.lock = threading.Lock()

    def connect(self) -> None:
        self.close()

        log(
            f"[{self.game_id}] Connecting to UCI "
            f"{self.host}:{self.port}"
        )

        sock = socket.create_connection(
            (self.host, self.port),
            timeout=10,
        )

        # Do not leave a timeout on the socket while the engine is
        # searching. A long search must be allowed to complete.
        sock.settimeout(None)

        self.sock = sock
        self.file = sock.makefile("r", encoding="utf-8", newline="\n")

        self.send("uci")
        self.wait_for("uciok")

        self.send("isready")
        self.wait_for("readyok")

        log(f"[{self.game_id}] UCI connection ready")

    def close(self) -> None:
        if self.file is not None:
            try:
                self.file.close()
            except Exception:
                pass

        self.file = None

        if self.sock is not None:
            try:
                self.sock.close()
            except Exception:
                pass

        self.sock = None

    def send(self, command: str) -> None:
        if self.sock is None:
            raise ConnectionError("UCI socket is not connected")

        log(f"[{self.game_id}] > {command}")

        self.sock.sendall(
            (command + "\n").encode("utf-8")
        )

    def read_line(self) -> str:
        if self.file is None:
            raise ConnectionError("UCI socket is not connected")

        line = self.file.readline()

        if line == "":
            raise ConnectionError("UCI server disconnected")

        line = line.rstrip("\r\n")

        log(f"[{self.game_id}] < {line}")

        return line

    def wait_for(self, prefix: str) -> str:
        while True:
            line = self.read_line()

            if line.startswith(prefix):
                return line

    def set_position(self, moves: list[str]) -> None:
        if moves:
            self.send(
                "position startpos moves " +
                " ".join(moves)
            )
        else:
            self.send("position startpos")

    def search(
        self,
        *,
        wtime_ms: Optional[int],
        btime_ms: Optional[int],
        winc_ms: Optional[int],
        binc_ms: Optional[int],
        movetime_ms: Optional[int],
    ) -> tuple[str, Optional[str]]:
        """
        Returns:
            (bestmove, evaluation)
        """

        if (
            wtime_ms is not None
            and btime_ms is not None
            and winc_ms is not None
            and binc_ms is not None
        ):
            command = (
                f"go wtime {wtime_ms} "
                f"btime {btime_ms} "
                f"winc {winc_ms} "
                f"binc {binc_ms}"
            )

            self.send(command)

        else:
            if movetime_ms is None:
                movetime_ms = 1000

            self.send(f"go movetime {movetime_ms}")

        evaluation = None

        while True:
            line = self.read_line()

            # Example:
            # info depth 12 score cp 35 ...
            if line.startswith("info "):
                parts = line.split()

                try:
                    score_index = parts.index("score")

                    if score_index + 2 < len(parts):
                        score_type = parts[score_index + 1]
                        score_value = parts[score_index + 2]

                        # Centipawns
                        if score_type == "cp":
                            cp = int(score_value)
                            evaluation = f"{cp / 100:+.2f}"

                        # Mate score
                        elif score_type == "mate":
                            mate = int(score_value)

                            if mate > 0:
                                evaluation = f"mate +{mate}"
                            else:
                                evaluation = f"mate {mate}"

                except (ValueError, IndexError):
                    pass

                continue

            if not line.startswith("bestmove"):
                continue

            parts = line.split()

            if len(parts) < 2:
                raise RuntimeError(
                    f"Malformed bestmove response: {line}"
                )

            move = parts[1]

            if move == "(none)":
                raise RuntimeError(
                    "Engine returned bestmove (none)"
                )

            return move, evaluation


# ============================================================
# Game state
# ============================================================

@dataclass
class GameWorker:
    api: LichessAPI
    game_id: str
    bot_id: str
    chat: GroqChat

    color: Optional[str] = None

    last_move: Optional[str] = None
    evaluation: Optional[str] = None

    # Current authoritative chess position.
    board: chess.Board = field(
        default_factory=chess.Board,
    )

    # Complete move history received from Lichess.
    move_history: list[str] = field(
        default_factory=list,
    )

    last_processed_ply: Optional[int] = None

    state_lock: threading.Lock = field(
        default_factory=threading.Lock,
        repr=False,
    )

    engine: Optional[UCIClient] = None

    stop_event: threading.Event = field(
        default_factory=threading.Event,
        repr=False,
    )

    def determine_color(self, game_full: dict) -> None:
        white = game_full.get("white", {})
        black = game_full.get("black", {})

        white_id = white.get("id", "").lower()
        black_id = black.get("id", "").lower()
        bot_id = self.bot_id.lower()

        if white_id == bot_id:
            self.color = "white"
        elif black_id == bot_id:
            self.color = "black"
        else:
            raise RuntimeError(
                f"Bot {self.bot_id} is not a player in game "
                f"{self.game_id}"
            )

    def is_our_turn(self, moves: list[str]) -> bool:
        if self.color == "white":
            return len(moves) % 2 == 0

        if self.color == "black":
            return len(moves) % 2 == 1

        return False

    def ensure_engine(self) -> None:
        if self.engine is None:
            self.engine = UCIClient(
                UCI_HOST,
                UCI_PORT,
                self.game_id,
            )

            self.engine.connect()

    def close_engine(self) -> None:
        if self.engine is not None:
            self.engine.close()
            self.engine = None

    def process_chat(self, event: dict) -> None:
        username = event.get("username", "")
        text = event.get("text", "").strip()
        room = event.get("room", "player")

        if not text or username.lower() == self.bot_id.lower():
            return

        context = self.get_chat_context()

        reply = self.chat.reply(
            username,
            text,
            **context,
        )

        self.api.send_chat(
            self.game_id,
            reply,
            room=room,
        )

        log(
            f"[{self.game_id}] "
            f"Chat reply to {username}: {reply}"
        )

    def process_position(
        self,
        moves: list[str],
        state: dict,
    ) -> None:
        """
        Process exactly one new game state.

        The move list from Lichess is authoritative.
        """

        ply = len(moves)

        self.rebuild_board(moves)

        with self.state_lock:
            if (
                self.last_processed_ply is not None
                and ply == self.last_processed_ply
            ):
                return

            if (
                self.last_processed_ply is not None
                and ply < self.last_processed_ply
            ):
                return

        if not self.is_our_turn(moves):
            with self.state_lock:
                self.last_processed_ply = ply
            return

        self.ensure_engine()

        assert self.engine is not None

        self.engine.set_position(moves)

        #wtime_ms = deciseconds_to_ms(state.get("wtime"))
        #btime_ms = deciseconds_to_ms(state.get("btime"))
        #winc_ms = deciseconds_to_ms(state.get("winc"))
        #binc_ms = deciseconds_to_ms(state.get("binc"))
        wtime_ms = state.get("wtime")
        btime_ms = state.get("btime")
        winc_ms = state.get("winc")
        binc_ms = state.get("binc")

        if (
            wtime_ms is not None
            and btime_ms is not None
        ):
            wtime_ms = max(0, wtime_ms - MOVE_OVERHEAD_MS)
            btime_ms = max(0, btime_ms - MOVE_OVERHEAD_MS)

        move, evaluation = self.engine.search(
            wtime_ms=wtime_ms,
            btime_ms=btime_ms,
            winc_ms=winc_ms,
            binc_ms=binc_ms,
            movetime_ms=1000,
        )

        self.evaluation = evaluation

        log(
            f"[{self.game_id}] Engine selected {move} "
            f"(eval: {evaluation})"
        )

        self.api.make_move(self.game_id, move)

        with self.state_lock:
            self.last_processed_ply = ply

        # Update our local board immediately after successfully
        # submitting our move. This means chat occurring between
        # this move and Lichess's next gameState already sees the
        # correct position.
        try:
            chess_move = chess.Move.from_uci(move)

            if chess_move not in self.board.legal_moves:
                log_error(
                    f"[{self.game_id}] Engine produced move "
                    f"{move} that is illegal on local board"
                )
            else:
                self.board.push(chess_move)
                self.move_history.append(move)
                self.last_move = move

        except ValueError as exc:
            log_error(
                f"[{self.game_id}] Invalid engine move {move}: {exc}"
            )

    def run(self) -> None:
        log(f"[{self.game_id}] Game worker started")

        try:
            for event in self.api.stream_game(self.game_id):
                if self.stop_event.is_set():
                    break

                event_type = event.get("type")

                if event_type == "gameFull":
                    state = event.get("state", {})

                    if state.get("status") != "started":
                        log(
                            f"[{self.game_id}] "
                            f"Game already ended: {state.get('status')}"
                        )
                        break

                    if not self.is_recent_game(event):
                        log(
                            f"[{self.game_id}] "
                            "Game is older than 24 hours; ignoring"
                        )
                        break

                    self.determine_color(event)

                    moves = parse_moves(
                        state.get("moves", "")
                    )

                    log(
                        f"[{self.game_id}] "
                        f"Game full: {self.color}, "
                        f"{len(moves)} plies"
                    )

                    self.process_position(
                        moves,
                        state,
                    )

                elif event_type == "gameState":
                    status = event.get("status")

                    if status != "started":
                        log(
                            f"[{self.game_id}] "
                            f"Game ended: {status}"
                        )
                        break

                    moves = parse_moves(
                        event.get("moves", "")
                    )

                    self.process_position(
                        moves,
                        event,
                    )

                elif event_type == "chatLine":
                    try:
                        self.process_chat(event)
                    except Exception as exc:
                        log_error(
                            f"[{self.game_id}] Chat error: {exc}"
                        )

        except requests.HTTPError as exc:
            log_error(
                f"[{self.game_id}] Lichess HTTP error: {exc}"
            )

        except (ConnectionError, OSError) as exc:
            log_error(
                f"[{self.game_id}] Connection error: {exc}"
            )

        except Exception as exc:
            log_error(
                f"[{self.game_id}] "
                f"Unhandled game error: {exc}"
            )

        finally:
            self.stop_event.set()
            self.close_engine()
            log(f"[{self.game_id}] Game worker stopped")
    
    def rebuild_board(self, moves: list[str]) -> None:
        """
        Rebuild the board from the authoritative Lichess move list.

        Lichess gives us UCI moves, so python-chess can reconstruct
        the complete position including castling, en passant, etc.
        """

        board = chess.Board()

        for move_string in moves:
            move = chess.Move.from_uci(move_string)

            if move not in board.legal_moves:
                raise RuntimeError(
                    f"Illegal move while rebuilding game: "
                    f"{move_string}"
                )

            board.push(move)

        self.board = board
        self.move_history = list(moves)

        self.last_move = moves[-1] if moves else None

    def get_material_summary(self) -> str:
        """Return a compact material summary."""

        piece_names = {
            chess.PAWN: "P",
            chess.KNIGHT: "N",
            chess.BISHOP: "B",
            chess.ROOK: "R",
            chess.QUEEN: "Q",
            chess.KING: "K",
        }

        def color_material(color: chess.Color) -> str:
            pieces: list[str] = []

            for piece_type in (
                chess.QUEEN,
                chess.ROOK,
                chess.BISHOP,
                chess.KNIGHT,
                chess.PAWN,
            ):
                count = len(
                    self.board.pieces(piece_type, color)
                )

                if count:
                    pieces.append(
                        piece_names[piece_type] * count
                    )

            return " ".join(pieces) if pieces else "none"

        return (
            f"White: {color_material(chess.WHITE)}; "
            f"Black: {color_material(chess.BLACK)}"
        )

    def get_chat_context(self) -> dict:
        """Build the complete chess context for the chat model."""

        side_to_move = (
            "white"
            if self.board.turn == chess.WHITE
            else "black"
        )

        status = "playing"

        if self.board.is_checkmate():
            status = "checkmate"
        elif self.board.is_stalemate():
            status = "stalemate"
        elif self.board.is_insufficient_material():
            status = "draw by insufficient material"
        elif self.board.is_fifty_moves():
            status = "draw by fifty-move rule"
        elif self.board.is_repetition():
            status = "draw by repetition"

        recent_moves = self.move_history[-12:]

        legal_moves = [
            move.uci()
            for move in self.board.legal_moves
        ]

        return {
            "board_fen": self.board.fen(),
            "board_ascii": str(self.board),
            "bot_color": self.color,
            "side_to_move": side_to_move,
            "last_move": self.last_move,
            "evaluation": self.evaluation,
            "move_number": self.board.fullmove_number,
            "material": self.get_material_summary(),
            "status": status,
            "in_check": self.board.is_check(),
            "legal_moves": legal_moves,
            "recent_moves": recent_moves,
        }

    def is_recent_game(self, event: dict) -> bool:
        """
        Only resume games that had activity within the last 24 hours.

        Prefer lastMoveAt because an old game that was recently resumed
        should still be considered recent. Fall back to createdAt for
        games that have never had a move.
        """
        now_ms = int(time.time() * 1000)

        last_activity_ms = event.get("lastMoveAt")

        if last_activity_ms is None:
            last_activity_ms = event.get("createdAt")

        if last_activity_ms is None:
            # If Lichess doesn't provide either timestamp, don't reject
            # the game solely because of missing metadata.
            return True

        try:
            age_seconds = (now_ms - int(last_activity_ms)) / 1000
        except (TypeError, ValueError):
            return True

        return age_seconds <= STALE_GAME_SECONDS


# ============================================================
# Helpers
# ============================================================

def parse_moves(move_string: str) -> list[str]:
    if not move_string:
        return []

    return move_string.split()


def deciseconds_to_ms(value) -> Optional[int]:
    if value is None:
        return None

    try:
        return int(value) * 100
    except (TypeError, ValueError):
        return None


def challenge_allowed(challenge: dict) -> bool:
    rated = challenge.get("rated", False)

    if rated and not ALLOW_RATED:
        return False

    if not rated and not ALLOW_CASUAL:
        return False

    # Only standard chess for now.
    variant = challenge.get("variant", {})
    key = variant.get("key")

    if key not in (None, "standard"):
        return False

    return True


# ============================================================
# Bot manager
# ============================================================

class BotManager:
    def __init__(self, api: LichessAPI, bot_id: str):
        self.api = api
        self.bot_id = bot_id

        # Ignore games that existed before this process started.
        # Lichess reports createdAt in milliseconds since Unix epoch.
        self.start_time_ms = int(time.time() * 1000)

        self.executor = ThreadPoolExecutor(
            max_workers=MAX_GAMES,
            thread_name_prefix="lichess-game",
        )

        self.games: dict[str, GameWorker] = {}
        self.games_lock = threading.Lock()

    def active_game_count(self) -> int:
        with self.games_lock:
            return len(self.games)

    def start_game(self, game: dict) -> None:
        game_id = game.get("id")

        if not game_id:
            return

        # Lichess reports lastMoveAt in milliseconds since Unix epoch.
        last_move_at = game.get("lastMoveAt")

        if last_move_at is not None:
            try:
                last_move_time = int(last_move_at) / 1000.0
                age = time.time() - last_move_time

                if age > STALE_GAME_SECONDS:
                    log(
                        f"[{game_id}] Game has been inactive for "
                        f"{age / 86400:.1f} days; aborting"
                    )

                    try:
                        self.api.abort_game(game_id)
                        log(
                            f"[{game_id}] Stale game aborted"
                        )
                    except Exception as exc:
                        log_error(
                            f"[{game_id}] Could not abort stale game: "
                            f"{exc}"
                        )

                    return

            except (TypeError, ValueError):
                log_error(
                    f"[{game_id}] Invalid lastMoveAt: "
                    f"{last_move_at!r}"
                )

        # If lastMoveAt is missing, don't automatically assume the game
        # is stale. Let the game worker inspect the game normally.
        with self.games_lock:
            if game_id in self.games:
                log(
                    f"[{game_id}] Already running; ignoring duplicate"
                )
                return

            if len(self.games) >= MAX_GAMES:
                log_error(
                    f"[{game_id}] Maximum game count "
                    f"({MAX_GAMES}) reached"
                )
                return

            worker = GameWorker(
                api=self.api,
                game_id=game_id,
                bot_id=self.bot_id,
                chat=GroqChat(GROQ_API_KEY),
            )

            self.games[game_id] = worker

        self.executor.submit(
            self._run_game,
            worker,
        )

    def _run_game(self, worker: GameWorker) -> None:
        try:
            worker.run()
        finally:
            with self.games_lock:
                self.games.pop(worker.game_id, None)

    def stop_all(self) -> None:
        with self.games_lock:
            workers = list(self.games.values())

        for worker in workers:
            worker.stop_event.set()

        self.executor.shutdown(
            wait=True,
            cancel_futures=False,
        )


# ============================================================
# Main event loop
# ============================================================

def main() -> None:
    if not LICHESS_TOKEN:
        raise RuntimeError(
            "LICHESS_TOKEN is not set.\n"
            "Run:\n"
            "  export LICHESS_TOKEN='your_token'"
        )

    if not GROQ_API_KEY:
        raise RuntimeError(
            "GROQ_API_KEY is not set.\n"
            "Run:\n"
            "  export GROQ_API_KEY='your_key'"
        )

    api = LichessAPI(LICHESS_TOKEN)

    log("Connecting to Lichess...")

    account = api.get_account()

    bot_id = account["id"]
    username = account.get("username", bot_id)
    title = account.get("title")

    log("")
    log(f"Account: {username}")
    log(f"ID:      {bot_id}")
    log(f"Title:   {title}")
    log("")

    if title != "BOT":
        log_error(
            "WARNING: this account is not marked as a BOT account."
        )

    manager = BotManager(api, bot_id)

    log(
        f"Waiting for challenges/events "
        f"(max {MAX_GAMES} simultaneous games)..."
    )

    try:
        while True:
            try:
                for event in api.stream_events():
                    event_type = event.get("type")

                    if event_type == "challenge":
                        challenge = event.get("challenge", {})
                        challenge_id = challenge.get("id")

                        if not challenge_id:
                            continue

                        challenger = challenge.get(
                            "challenger",
                            {},
                        ).get("name", "unknown")

                        if not challenge_allowed(challenge):
                            log(
                                f"Declining challenge "
                                f"{challenge_id} from {challenger}"
                            )

                            try:
                                api.decline_challenge(
                                    challenge_id
                                )
                            except Exception as exc:
                                log_error(
                                    f"Could not decline "
                                    f"{challenge_id}: {exc}"
                                )

                            continue

                        if not AUTO_ACCEPT_CHALLENGES:
                            log(
                                f"Ignoring challenge "
                                f"{challenge_id} from {challenger}"
                            )
                            continue

                        if (
                            manager.active_game_count()
                            >= MAX_GAMES
                        ):
                            log(
                                f"Declining challenge "
                                f"{challenge_id}: "
                                f"maximum games reached"
                            )

                            try:
                                api.decline_challenge(
                                    challenge_id
                                )
                            except Exception as exc:
                                log_error(
                                    f"Could not decline "
                                    f"{challenge_id}: {exc}"
                                )

                            continue

                        log(
                            f"Accepting challenge "
                            f"{challenge_id} from {challenger}"
                        )

                        try:
                            api.accept_challenge(
                                challenge_id
                            )
                        except Exception as exc:
                            log_error(
                                f"Could not accept "
                                f"{challenge_id}: {exc}"
                            )

                    elif event_type == "gameStart":
                        game = event.get("game", {})
                        game_id = game.get("id")

                        if not game_id:
                            continue

                        log(
                            f"Game started: {game_id} "
                            f"({manager.active_game_count() + 1} "
                            f"active)"
                        )

                        manager.start_game(game)

            except requests.HTTPError as exc:
                log_error(
                    f"Lichess event stream HTTP error: {exc}"
                )

                time.sleep(RECONNECT_DELAY_SECONDS)

            except (
                requests.ConnectionError,
                requests.Timeout,
                ConnectionError,
                OSError,
            ) as exc:
                log_error(
                    f"Lichess event stream disconnected: {exc}"
                )

                time.sleep(RECONNECT_DELAY_SECONDS)

            except KeyboardInterrupt:
                raise

            except Exception as exc:
                log_error(
                    f"Event stream error: {exc}"
                )

                time.sleep(RECONNECT_DELAY_SECONDS)

    except KeyboardInterrupt:
        log("")
        log("Stopping bot...")

    finally:
        manager.stop_all()
        log("Bot stopped.")


if __name__ == "__main__":
    main()
