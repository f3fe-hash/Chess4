#!/usr/bin/env python3

import argparse
import json
import os
import random
import shutil
import signal
import sys
from pathlib import Path

import chess
import chess.pgn
import chess.engine
import zstandard


# ============================================================
# Global stop handling
# ============================================================

stop_requested = False


def handle_signal(signum, frame):
    """
    Request a clean shutdown.

    Docker sends SIGTERM when stopping a container.
    SIGINT handles Ctrl+C when running interactively.
    """

    global stop_requested

    if not stop_requested:
        print(
            "\n[STOP] Shutdown requested. "
            "Finishing current operation and checkpointing...",
            flush=True,
        )

    stop_requested = True


signal.signal(signal.SIGTERM, handle_signal)
signal.signal(signal.SIGINT, handle_signal)


# ============================================================
# PGN loading
# ============================================================

def iter_games(path):
    """
    Stream games from either:

        .pgn
        .pgn.zst

    without loading the entire database into RAM.

    Games are yielded one at a time.
    """

    if str(path).endswith(".zst"):
        with open(path, "rb") as compressed:
            dctx = zstandard.ZstdDecompressor()

            with dctx.stream_reader(compressed) as reader:
                import io

                text = io.TextIOWrapper(
                    reader,
                    encoding="utf-8",
                    errors="replace",
                )

                while True:
                    game = chess.pgn.read_game(text)

                    if game is None:
                        break

                    yield game

    else:
        with open(
            path,
            "r",
            encoding="utf-8",
            errors="replace",
        ) as f:
            while True:
                game = chess.pgn.read_game(f)

                if game is None:
                    break

                yield game


# ============================================================
# Stockfish evaluation
# ============================================================

def evaluate_position(engine, board, depth):
    """
    Evaluate a position from White's perspective.

    Returns:

        Positive = White advantage
        Negative = Black advantage

    Values are in pawns.

    Mate positions are represented as:

        +1000.0 = White has mate
        -1000.0 = Black has mate
    """

    result = engine.analyse(
        board,
        chess.engine.Limit(depth=depth),
    )

    score = result["score"].pov(chess.WHITE)

    if score.is_mate():
        mate = score.mate()

        if mate is None:
            return None

        return 1000.0 if mate > 0 else -1000.0

    return score.score(
        mate_score=100000
    ) / 100.0


# ============================================================
# Board encoding
# ============================================================

PIECE_VALUES = {
    chess.PAWN:   1,
    chess.KNIGHT: 2,
    chess.BISHOP: 3,
    chess.ROOK:   4,
    chess.QUEEN:  5,
    chess.KING:   6,
}


def board_to_array(board):
    """
    Convert a chess.Board into a 64-element list.

    Square indexing matches Chess4:

        A1 = 0
        B1 = 1
        ...
        H1 = 7
        A2 = 8
        ...
        H8 = 63

    Values:

         0 = empty
        +1 = white pawn
        +2 = white knight
        +3 = white bishop
        +4 = white rook
        +5 = white queen
        +6 = white king

        -1 = black pawn
        -2 = black knight
        -3 = black bishop
        -4 = black rook
        -5 = black queen
        -6 = black king
    """

    position = [0] * 64

    for square, piece in board.piece_map().items():
        value = PIECE_VALUES[piece.piece_type]

        if piece.color == chess.BLACK:
            value = -value

        position[square] = value

    return position


def fen_to_array(fen):
    """
    Convert a FEN string into a 64-element array.
    """

    board = chess.Board(fen)

    return board_to_array(board)


# ============================================================
# Checkpointing
# ============================================================

def save_checkpoint(
    path,
    games,
    examined,
    positions,
    seen,
):
    """
    Save progress atomically.

    A temporary file is written first and then renamed into
    place. This greatly reduces the chance of ending up with
    a corrupt checkpoint if the machine loses power while
    saving.

    The checkpoint is deliberately saved at game boundaries.
    """

    checkpoint = {
        "games": games,
        "examined": examined,
        "positions": positions,
        "seen": list(seen),
    }

    temp_path = path.with_suffix(
        path.suffix + ".tmp"
    )

    with open(
        temp_path,
        "w",
        encoding="utf-8",
    ) as f:
        json.dump(
            checkpoint,
            f,
            separators=(",", ":"),
        )

        f.flush()
        os.fsync(f.fileno())

    os.replace(
        temp_path,
        path,
    )


def load_checkpoint(path):
    """
    Load an existing checkpoint.

    If no checkpoint exists, return an empty state.
    """

    if not path.exists():
        return {
            "games": 0,
            "examined": 0,
            "positions": 0,
            "seen": set(),
        }

    print(
        f"Loading checkpoint: {path}",
        flush=True,
    )

    try:
        with open(
            path,
            "r",
            encoding="utf-8",
        ) as f:
            checkpoint = json.load(f)

    except (OSError, json.JSONDecodeError) as exc:
        print(
            f"ERROR: Could not load checkpoint: {exc}",
            file=sys.stderr,
        )

        return {
            "games": 0,
            "examined": 0,
            "positions": 0,
            "seen": set(),
        }

    return {
        "games": int(
            checkpoint.get("games", 0)
        ),
        "examined": int(
            checkpoint.get("examined", 0)
        ),
        "positions": int(
            checkpoint.get("positions", 0)
        ),
        "seen": set(
            checkpoint.get("seen", [])
        ),
    }


# ============================================================
# Position output
# ============================================================

def append_position(
    file_handle,
    fen,
    evaluation,
):
    """
    Append one position to the output file.

    The file is flushed after every position so that a machine
    failure doesn't leave a large amount of evaluated data
    sitting only in the Python buffer.
    """

    file_handle.write(
        f"[{fen} {evaluation:+.4f}]\n"
    )

    file_handle.flush()


# ============================================================
# Dataset loader
# ============================================================

def load_positions(path):
    """
    Load positions from:

        [<FEN> <eval>]

    Returns:

        [
            (fen, evaluation),
            ...
        ]
    """

    positions = []

    if not path.exists():
        return positions

    with open(
        path,
        "r",
        encoding="utf-8",
    ) as f:
        for line_number, line in enumerate(
            f,
            start=1,
        ):
            line = line.strip()

            if not line:
                continue

            if (
                not line.startswith("[")
                or not line.endswith("]")
            ):
                print(
                    f"Warning: invalid line "
                    f"{line_number}: {line}",
                    file=sys.stderr,
                )
                continue

            content = line[1:-1]
            fields = content.split()

            if len(fields) != 7:
                print(
                    f"Warning: invalid line "
                    f"{line_number}: {line}",
                    file=sys.stderr,
                )
                continue

            fen = " ".join(
                fields[:6]
            )

            try:
                evaluation = float(
                    fields[6]
                )

            except ValueError:
                print(
                    f"Warning: invalid evaluation "
                    f"on line {line_number}: {line}",
                    file=sys.stderr,
                )
                continue

            positions.append(
                (fen, evaluation)
            )

    return positions


# ============================================================
# Main
# ============================================================

def main():
    parser = argparse.ArgumentParser(
        description=(
            "Generate chess training positions from "
            "Lichess PGN data and evaluate positions "
            "with Stockfish."
        )
    )

    parser.add_argument(
        "input",
        type=Path,
        help="Lichess .pgn or .pgn.zst file",
    )

    parser.add_argument(
        "--stockfish",
        default="stockfish",
        help="Path to Stockfish executable",
    )

    parser.add_argument(
        "--output",
        type=Path,
        default=Path(
            "training_positions.txt"
        ),
        help="Output training data file",
    )

    parser.add_argument(
        "--checkpoint",
        type=Path,
        default=Path(
            "checkpoint.json"
        ),
        help="Checkpoint file",
    )

    parser.add_argument(
        "--positions",
        type=int,
        default=1000,
        help="Number of positions to generate",
    )

    parser.add_argument(
        "--depth",
        type=int,
        default=16,
        help="Stockfish analysis depth",
    )

    parser.add_argument(
        "--min-ply",
        type=int,
        default=16,
        help=(
            "Don't sample positions before this ply"
        ),
    )

    parser.add_argument(
        "--max-ply",
        type=int,
        default=100,
        help=(
            "Don't sample positions after this ply"
        ),
    )

    parser.add_argument(
        "--sample-rate",
        type=float,
        default=0.10,
        help=(
            "Probability of testing each eligible "
            "position"
        ),
    )

    parser.add_argument(
        "--checkpoint-games",
        type=int,
        default=100,
        help=(
            "Save checkpoint every N completed games"
        ),
    )

    parser.add_argument(
        "--threads",
        type=int,
        default=4,
        help="Number of Stockfish threads",
    )

    parser.add_argument(
        "--hash-mb",
        type=int,
        default=512,
        help="Stockfish hash size in MB",
    )

    parser.add_argument(
        "--max-eval",
        type=float,
        default=None,
        help=(
            "Only save positions whose absolute "
            "evaluation is <= this value"
        ),
    )

    parser.add_argument(
        "--seed",
        type=int,
        default=12345,
        help="Random seed",
    )

    args = parser.parse_args()

    # --------------------------------------------------------
    # Validate arguments.
    # --------------------------------------------------------

    if not args.input.exists():
        print(
            f"Input file does not exist: {args.input}",
            file=sys.stderr,
        )
        return 1

    if args.positions <= 0:
        print(
            "--positions must be greater than zero",
            file=sys.stderr,
        )
        return 1

    if args.depth <= 0:
        print(
            "--depth must be greater than zero",
            file=sys.stderr,
        )
        return 1

    if args.threads <= 0:
        print(
            "--threads must be greater than zero",
            file=sys.stderr,
        )
        return 1

    if args.hash_mb <= 0:
        print(
            "--hash-mb must be greater than zero",
            file=sys.stderr,
        )
        return 1

    if not 0.0 <= args.sample_rate <= 1.0:
        print(
            "--sample-rate must be between 0 and 1",
            file=sys.stderr,
        )
        return 1

    if args.min_ply < 0:
        print(
            "--min-ply cannot be negative",
            file=sys.stderr,
        )
        return 1

    if args.max_ply < args.min_ply:
        print(
            "--max-ply must be >= --min-ply",
            file=sys.stderr,
        )
        return 1

    if args.checkpoint_games <= 0:
        print(
            "--checkpoint-games must be greater than zero",
            file=sys.stderr,
        )
        return 1

    # --------------------------------------------------------
    # Locate Stockfish.
    # --------------------------------------------------------

    stockfish_path = Path(
        args.stockfish
    )

    if not stockfish_path.exists():
        if shutil.which(args.stockfish) is None:
            print(
                f"Could not find Stockfish: "
                f"{args.stockfish}",
                file=sys.stderr,
            )
            return 1

    # --------------------------------------------------------
    # Prepare output directory.
    # --------------------------------------------------------

    args.output.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    args.checkpoint.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    # --------------------------------------------------------
    # Load checkpoint.
    # --------------------------------------------------------

    random.seed(args.seed)

    checkpoint = load_checkpoint(
        args.checkpoint
    )

    games = checkpoint["games"]
    examined = checkpoint["examined"]
    saved = checkpoint["positions"]
    seen = checkpoint["seen"]

    # --------------------------------------------------------
    # Handle an already-completed job.
    # --------------------------------------------------------

    if saved >= args.positions:
        print(
            f"Target already reached: "
            f"{saved:,}/{args.positions:,}"
        )
        return 0

    # --------------------------------------------------------
    # Display configuration.
    # --------------------------------------------------------

    print()
    print(
        f"Reading:             {args.input}"
    )
    print(
        f"Target positions:    {args.positions:,}"
    )
    print(
        f"Starting game:       {games + 1:,}"
    )
    print(
        f"Already saved:       {saved:,}"
    )
    print(
        f"Stockfish:           {args.stockfish}"
    )
    print(
        f"Threads:              {args.threads}"
    )
    print(
        f"Hash:                 {args.hash_mb} MB"
    )
    print(
        f"Depth:                {args.depth}"
    )
    print(
        f"PLY range:            "
        f"{args.min_ply}-{args.max_ply}"
    )
    print(
        f"Sample rate:          {args.sample_rate}"
    )
    print(
        f"Checkpoint interval:  "
        f"{args.checkpoint_games} games"
    )

    if args.max_eval is not None:
        print(
            f"Maximum |eval|:       "
            f"{args.max_eval:.2f}"
        )

    print()

    # --------------------------------------------------------
    # Open output file in append mode.
    # --------------------------------------------------------

    output_file = open(
        args.output,
        "a",
        encoding="utf-8",
        buffering=1,
    )

    engine = None

    try:
        # ----------------------------------------------------
        # Start Stockfish.
        # ----------------------------------------------------

        engine = chess.engine.SimpleEngine.popen_uci(
            args.stockfish
        )

        engine.configure({
            "Threads": args.threads,
            "Hash": args.hash_mb,
        })

        # ----------------------------------------------------
        # Process games.
        # ----------------------------------------------------

        for game_number, game in enumerate(
            iter_games(args.input),
            start=1,
        ):
            # Skip games already completed by a previous run.
            if game_number <= games:
                continue

            if stop_requested:
                break

            board = game.board()

            for ply, move in enumerate(
                game.mainline_moves(),
                start=1,
            ):
                if stop_requested:
                    break

                board.push(move)

                if ply < args.min_ply:
                    continue

                if ply > args.max_ply:
                    break

                if saved >= args.positions:
                    break

                if random.random() > args.sample_rate:
                    continue

                examined += 1

                # Skip variants.
                if board.is_variant_end():
                    continue

                fen = board.fen()

                # Avoid duplicate positions.
                if fen in seen:
                    continue

                evaluation = evaluate_position(
                    engine,
                    board,
                    args.depth,
                )

                if evaluation is None:
                    continue

                # Optional equal-position filter.
                if (
                    args.max_eval is not None
                    and abs(evaluation)
                    > args.max_eval
                ):
                    continue

                seen.add(fen)

                append_position(
                    output_file,
                    fen,
                    evaluation,
                )

                saved += 1

                print(
                    f"[{saved:6d}/{args.positions:6d}] "
                    f"eval={evaluation:+.2f} "
                    f"ply={ply} "
                    f"{fen}",
                    flush=True,
                )

            # ------------------------------------------------
            # This game has now been completely processed.
            # ------------------------------------------------

            if not stop_requested:
                games = game_number

            # ------------------------------------------------
            # Periodic checkpoint.
            # ------------------------------------------------

            if (
                games > 0
                and games % args.checkpoint_games == 0
            ):
                save_checkpoint(
                    args.checkpoint,
                    games,
                    examined,
                    saved,
                    seen,
                )

                print(
                    f"[CHECKPOINT] "
                    f"games={games:,} "
                    f"examined={examined:,} "
                    f"saved={saved:,}",
                    flush=True,
                )

            if saved >= args.positions:
                break

            if stop_requested:
                break

    except Exception as exc:
        print(
            f"\nERROR: {exc}",
            file=sys.stderr,
        )

        # Save whatever progress is known.
        save_checkpoint(
            args.checkpoint,
            games,
            examined,
            saved,
            seen,
        )

        raise

    finally:
        # ----------------------------------------------------
        # Always checkpoint before shutting down.
        # ----------------------------------------------------

        save_checkpoint(
            args.checkpoint,
            games,
            examined,
            saved,
            seen,
        )

        if engine is not None:
            engine.quit()

        output_file.close()

    # --------------------------------------------------------
    # Final status.
    # --------------------------------------------------------

    print()

    if stop_requested:
        print(
            "Paused safely."
        )
    else:
        print(
            "Finished."
        )

    print(
        f"Games processed:  {games:,}"
    )

    print(
        f"Positions tested: {examined:,}"
    )

    print(
        f"Positions saved:  {saved:,}"
    )

    print(
        f"Output:            {args.output}"
    )

    print(
        f"Checkpoint:        {args.checkpoint}"
    )

    print()

    # --------------------------------------------------------
    # Verify the output file.
    # --------------------------------------------------------

    if args.output.exists():
        print(
            "Testing position loader..."
        )

        loaded = load_positions(
            args.output
        )

        print(
            f"Loaded {len(loaded):,} positions."
        )

        if loaded:
            fen, evaluation = loaded[0]

            print()
            print(
                "First position:"
            )

            print(
                f"FEN:  {fen}"
            )

            print(
                f"Eval: {evaluation:+.4f}"
            )

            array = fen_to_array(
                fen
            )

            print()
            print(
                "Board array:"
            )

            print(
                array
            )

    return 0


if __name__ == "__main__":
    sys.exit(main())
