import socket
import time
from datetime import datetime, timedelta
from pathlib import Path


# ============================================================
# Configuration
# ============================================================

INPUT_FILE = Path("data/training_positions.txt")
OUTPUT_FILE = Path("data/training_positions_fixed.txt")

UCI_HOST = "127.0.0.1"
UCI_PORT = 8080

PROGRESS_INTERVAL = 1000


# ============================================================
# FastKat UCI communication
# ============================================================

class UCIClient:
    def __init__(self, host, port):
        self.socket = socket.create_connection((host, port))
        self.socket.settimeout(None)

        self.reader = self.socket.makefile(
            "r",
            encoding="utf-8",
            newline="\n",
        )

        self.writer = self.socket.makefile(
            "w",
            encoding="utf-8",
            newline="\n",
        )

    def send(self, command):
        self.writer.write(command + "\n")
        self.writer.flush()

    def read_line(self):
        line = self.reader.readline()

        if not line:
            raise RuntimeError(
                "FastKat UCI server disconnected unexpectedly"
            )

        return line.strip()

    def wait_for(self, expected):
        while True:
            line = self.read_line()

            if line == expected:
                return

    def close(self):
        try:
            self.send("quit")
        except (BrokenPipeError, OSError):
            pass

        try:
            self.writer.close()
        except OSError:
            pass

        try:
            self.reader.close()
        except OSError:
            pass

        try:
            self.socket.close()
        except OSError:
            pass


def evaluate_position(client, fen):
    """
    Evaluate a position using FastKat.

    Returns the evaluation in pawns from White's perspective.
    """

    client.send(f"position fen {fen}")
    client.send("eval")

    while True:
        line = client.read_line()

        if line.startswith("info score cp"):
            parts = line.split()

            try:
                cp_index = parts.index("cp")
                cp = int(parts[cp_index + 1])
                return cp / 100.0
            except (ValueError, IndexError):
                continue


# ============================================================
# Progress reporting
# ============================================================

def format_duration(seconds):
    if seconds < 60:
        return f"{seconds:.1f}s"

    if seconds < 3600:
        minutes = int(seconds // 60)
        remaining_seconds = int(seconds % 60)

        return f"{minutes}m {remaining_seconds}s"

    hours = int(seconds // 3600)
    minutes = int((seconds % 3600) // 60)

    return f"{hours}h {minutes}m"


def print_progress(processed, total, skipped, start_time):
    elapsed = time.monotonic() - start_time

    if processed > 0:
        average_time = elapsed / processed
        remaining = total - processed
        eta_seconds = remaining * average_time
    else:
        average_time = 0
        eta_seconds = 0

    percentage = (
        processed / total * 100
        if total > 0
        else 0
    )

    finish_time = (
        datetime.now()
        + timedelta(seconds=eta_seconds)
    )

    print()
    print("=" * 70)
    print("Progress")
    print("=" * 70)
    print(
        f"Processed:       {processed:,} / {total:,} "
        f"({percentage:.2f}%)"
    )
    print(f"Skipped:         {skipped:,}")
    print(f"Elapsed:         {format_duration(elapsed)}")
    print(f"Average/line:    {average_time:.3f}s")
    print(f"Remaining:       {total - processed:,}")
    print(f"Estimated left:  {format_duration(eta_seconds)}")
    print(
        f"Estimated done:  "
        f"{finish_time.strftime('%Y-%m-%d %H:%M:%S')}"
    )
    print("=" * 70)
    print()


# ============================================================
# Dataset processing
# ============================================================

def process_dataset():
    if not INPUT_FILE.exists():
        raise FileNotFoundError(
            f"Input file not found: {INPUT_FILE}"
        )

    print(f"Reading dataset: {INPUT_FILE}")

    lines = INPUT_FILE.read_text().splitlines()
    total = len(lines)

    print(f"Total input lines: {total:,}")
    print()
    print(
        f"Connecting to FastKat UCI server "
        f"at {UCI_HOST}:{UCI_PORT}..."
    )

    client = UCIClient(
        UCI_HOST,
        UCI_PORT,
    )

    start_time = time.monotonic()

    try:
        # --------------------------------------------------------
        # UCI initialization
        # --------------------------------------------------------

        client.send("uci")
        client.wait_for("uciok")

        client.send("isready")
        client.wait_for("readyok")

        print("Connected to FastKat.")
        print()

        # --------------------------------------------------------
        # Process positions
        # --------------------------------------------------------

        output_lines = []

        processed = 0
        skipped = 0

        for index, line in enumerate(lines, 1):
            line = line.strip()

            if not line:
                skipped += 1
                continue

            # Expected format:
            #
            # [FEN evaluation]

            if not line.startswith("[") or not line.endswith("]"):
                print(
                    f"Skipping malformed line {index}: {line}"
                )

                skipped += 1
                continue

            content = line[1:-1]

            try:
                fen, stockfish_eval = content.rsplit(" ", 1)
                stockfish_eval = float(stockfish_eval)

            except ValueError:
                print(
                    f"Skipping malformed line {index}: {line}"
                )

                skipped += 1
                continue

            # ----------------------------------------------------
            # Get FastKat's evaluation.
            # ----------------------------------------------------

            fastkat_eval = evaluate_position(
                client,
                fen,
            )

            # ----------------------------------------------------
            # Residual target:
            #
            #     Stockfish - FastKat
            #
            # Positive => FastKat underestimated White's position.
            # Negative => FastKat overestimated White's position.
            # ----------------------------------------------------

            correction = stockfish_eval - fastkat_eval

            output_lines.append(
                f"[{fen} {correction:.6f}]"
            )

            processed += 1

            # ----------------------------------------------------
            # Per-position logging
            # ----------------------------------------------------

            print(
                f"[{index}/{total}] "
                f"Stockfish: {stockfish_eval:+.2f}  "
                f"FastKat: {fastkat_eval:+.2f}  "
                f"Correction: {correction:+.2f}"
            )

            # ----------------------------------------------------
            # Progress summary every 1,000 positions
            # ----------------------------------------------------

            if processed % PROGRESS_INTERVAL == 0:
                print_progress(
                    processed,
                    total,
                    skipped,
                    start_time,
                )

        # --------------------------------------------------------
        # Write output
        # --------------------------------------------------------

        OUTPUT_FILE.parent.mkdir(
            parents=True,
            exist_ok=True,
        )

        OUTPUT_FILE.write_text(
            "\n".join(output_lines) + "\n"
        )

        # --------------------------------------------------------
        # Final statistics
        # --------------------------------------------------------

        elapsed = time.monotonic() - start_time

        print()
        print("=" * 70)
        print("Dataset processing complete")
        print("=" * 70)
        print(f"Input lines:      {total:,}")
        print(f"Processed:        {processed:,}")
        print(f"Skipped:          {skipped:,}")
        print(f"Elapsed:          {format_duration(elapsed)}")

        if processed:
            print(
                f"Average/position: "
                f"{elapsed / processed:.3f}s"
            )

        print()
        print(f"Wrote {len(output_lines):,} positions to:")
        print(OUTPUT_FILE)
        print("=" * 70)

    finally:
        client.close()


# ============================================================
# Main
# ============================================================

if __name__ == "__main__":
    process_dataset()