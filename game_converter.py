#!/usr/bin/env python3

import argparse
import random
import subprocess
import sys
import tempfile
from pathlib import Path

import chess
import chess.pgn
import chess.engine
import zstandard


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
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            while True:
                game = chess.pgn.read_game(f)

                if game is None:
                    break

                yield game


def evaluate_position(engine, board, depth):
    result = engine.analyse(
        board,
        chess.engine.Limit(depth=depth),
    )

    score = result["score"].pov(chess.WHITE)

    if score.is_mate():
        mate = score.mate()

        if mate is None:
            return None

        # Treat mate as an enormous evaluation.
        return 100000.0 if mate > 0 else -100000.0

    return score.score(mate_score=100000) / 100.0


def main():
    parser = argparse.ArgumentParser(
        description="Generate roughly equal FEN positions from Lichess PGN data."
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
        default=Path("pgo_positions.fen"),
        help="Output FEN file",
    )

    parser.add_argument(
        "--positions",
        type=int,
        default=1000,
        help="Number of roughly equal positions to generate",
    )

    parser.add_argument(
        "--eval",
        type=float,
        default=0.50,
        help="Maximum absolute Stockfish evaluation in pawns",
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
        print(f"Input file does not exist: {args.input}")
        return 1

    if not Path(args.stockfish).exists():
        # Also allow executables found through PATH.
        import shutil

        if shutil.which(args.stockfish) is None:
            print(f"Could not find Stockfish: {args.stockfish}")
            return 1

    random.seed(args.seed)

    positions = []
    seen = set()

    games = 0
    examined = 0

    print(f"Reading:       {args.input}")
    print(f"Target FENs:   {args.positions}")
    print(f"Eval window:   ±{args.eval:.2f}")
    print(f"Stockfish:     {args.stockfish}")
    print(f"Depth:         {args.depth}")
    print()

    engine = chess.engine.SimpleEngine.popen_uci(args.stockfish)

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

                # Only keep standard chess positions.
                if board.is_variant_end():
                    continue

                fen = board.fen()

                # Avoid duplicates.
                if fen in seen:
                    continue

                evaluation = evaluate_position(
                    engine,
                    board,
                    args.depth,
                )

                if evaluation is None:
                    continue

                if abs(evaluation) <= args.eval:
                    seen.add(fen)
                    positions.append(fen)

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
                    f"kept {len(positions):,}"
                )

    finally:
        engine.quit()

    with open(args.output, "w", encoding="utf-8") as f:
        for fen in positions:
            f.write(fen)
            f.write("\n")

    print()
    print(f"Finished.")
    print(f"Games processed: {games:,}")
    print(f"Positions tested: {examined:,}")
    print(f"Positions saved:  {len(positions):,}")
    print(f"Output:           {args.output}")

    return 0


if __name__ == "__main__":
    sys.exit(main())