#!/usr/bin/env python3

import argparse
import random
import sys
from pathlib import Path

import chess
import chess.pgn
import chess.engine
import zstandard


# ============================================================
# PGN loading
# ============================================================

def iter_games(path):
    """
    Stream games from either:
        .pgn
        .pgn.zst

    without loading the entire database into RAM.
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

    return score.score(mate_score=100000) / 100.0


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
# Dataset save/load
# ============================================================

def save_positions(path, positions):
    """
    Save positions in:

        [<FEN> <eval>]

    format.
    """

    with open(path, "w", encoding="utf-8") as f:
        for fen, evaluation in positions:
            f.write(
                f"[{fen} {evaluation:+.4f}]\n"
            )


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

    with open(path, "r", encoding="utf-8") as f:
        for line_number, line in enumerate(
            f,
            start=1,
        ):
            line = line.strip()

            if not line:
                continue

            if not line.startswith("[") or not line.endswith("]"):
                print(
                    f"Warning: invalid line {line_number}: {line}",
                    file=sys.stderr,
                )
                continue

            content = line[1:-1]
            fields = content.split()

            # FEN has six fields + evaluation.
            if len(fields) != 7:
                print(
                    f"Warning: invalid line {line_number}: {line}",
                    file=sys.stderr,
                )
                continue

            fen = " ".join(fields[:6])

            try:
                evaluation = float(fields[6])
            except ValueError:
                print(
                    f"Warning: invalid evaluation on "
                    f"line {line_number}: {line}",
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
            "Lichess PGN data and evaluate every position "
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
        default=Path("training_positions.txt"),
        help="Output training data file",
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
        help="Don't sample positions before this ply",
    )

    parser.add_argument(
        "--max-ply",
        type=int,
        default=100,
        help="Don't sample positions after this ply",
    )

    parser.add_argument(
        "--sample-rate",
        type=float,
        default=0.10,
        help="Probability of testing each eligible position",
    )

    parser.add_argument(
        "--seed",
        type=int,
        default=12345,
        help="Random seed",
    )

    args = parser.parse_args()

    if not args.input.exists():
        print(
            f"Input file does not exist: {args.input}"
        )
        return 1

    if not Path(args.stockfish).exists():
        import shutil

        if shutil.which(args.stockfish) is None:
            print(
                f"Could not find Stockfish: {args.stockfish}"
            )
            return 1

    random.seed(args.seed)

    positions = []
    seen = set()

    games = 0
    examined = 0

    print(f"Reading:       {args.input}")
    print(f"Target positions: {args.positions}")
    print(f"Stockfish:     {args.stockfish}")
    print(f"Depth:         {args.depth}")
    print(f"PLY range:     {args.min_ply}-{args.max_ply}")
    print(f"Sample rate:   {args.sample_rate}")
    print()

    engine = chess.engine.SimpleEngine.popen_uci(
        args.stockfish
    )

    try:
        for game in iter_games(args.input):
            games += 1

            board = game.board()

            for ply, move in enumerate(
                game.mainline_moves(),
                start=1,
            ):
                board.push(move)

                if ply < args.min_ply:
                    continue

                if ply > args.max_ply:
                    break

                if len(positions) >= args.positions:
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

                seen.add(fen)

                positions.append(
                    (fen, evaluation)
                )

                print(
                    f"[{len(positions):5d}/{args.positions}] "
                    f"eval={evaluation:+.2f} "
                    f"ply={ply} "
                    f"{fen}"
                )

            if len(positions) >= args.positions:
                break

            if games % 1000 == 0:
                print(
                    f"Processed {games:,} games | "
                    f"examined {examined:,} positions | "
                    f"saved {len(positions):,}"
                )

    finally:
        engine.quit()

    # Save all evaluated positions.
    save_positions(
        args.output,
        positions,
    )

    print()
    print("Finished.")
    print(f"Games processed: {games:,}")
    print(f"Positions tested: {examined:,}")
    print(f"Positions saved:  {len(positions):,}")
    print(f"Output:           {args.output}")

    # Verify the file can be loaded.
    print()
    print("Testing position loader...")

    loaded = load_positions(args.output)

    print(
        f"Loaded {len(loaded):,} positions."
    )

    if loaded:
        fen, evaluation = loaded[0]

        print()
        print("First position:")
        print(f"FEN:  {fen}")
        print(f"Eval: {evaluation:+.4f}")

        array = fen_to_array(fen)

        print()
        print("Board array:")
        print(array)

    return 0


if __name__ == "__main__":
    sys.exit(main())
