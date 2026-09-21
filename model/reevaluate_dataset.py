#!/usr/bin/env python3

import argparse
import re
import socket
import sys
import time
from pathlib import Path


DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 8080

ENTRY_RE = re.compile(
    r"^\[(?P<fen>.+?) (?P<eval>[+-]?\d+(?:\.\d+)?)\]$"
)

SCORE_RE = re.compile(
    r"^info\s+score\s+cp\s+(?P<score>-?\d+)"
)


def send_line(sock, line):
    sock.sendall((line + "\n").encode("ascii"))


def read_line(sock, buffer):
    while b"\n" not in buffer:
        data = sock.recv(4096)

        if not data:
            raise ConnectionError(
                "UCI server closed the connection"
            )

        buffer += data

    line, buffer = buffer.split(b"\n", 1)

    return (
        line.decode("utf-8", errors="replace").strip(),
        buffer,
    )


def wait_for(sock, buffer, predicate):
    while True:
        line, buffer = read_line(sock, buffer)

        if predicate(line):
            return line, buffer


def connect_engine(host, port):
    print(f"Connecting to {host}:{port}...")

    sock = socket.create_connection(
        (host, port),
        timeout=10,
    )

    # Don't impose a timeout while waiting for commands.
    sock.settimeout(None)

    buffer = b""

    send_line(sock, "uci")

    _, buffer = wait_for(
        sock,
        buffer,
        lambda line: line == "uciok",
    )

    send_line(sock, "isready")

    _, buffer = wait_for(
        sock,
        buffer,
        lambda line: line == "readyok",
    )

    print("Engine ready.")

    return sock, buffer


def evaluate_position(sock, buffer, fen):
    """
    Set the position and request the engine's raw/static evaluation.

    Expected UCI output:

        info score cp 123
    """

    send_line(sock, f"position fen {fen}")
    send_line(sock, "eval")

    while True:
        line, buffer = read_line(sock, buffer)

        match = SCORE_RE.match(line)

        if match:
            score_cp = int(match.group("score"))
            score = score_cp / 100.0

            return score, buffer


def format_eval(value):
    return f"{value:+.4f}"


def process_file(
    input_path,
    output_path,
    host,
    port,
    progress_interval,
):
    total = sum(1 for _ in input_path.open("r"))

    print(f"Input:     {input_path}")
    print(f"Output:    {output_path}")
    print(f"Positions: {total:,}")
    print()

    sock, buffer = connect_engine(host, port)

    processed = 0
    changed = 0
    skipped = 0

    start_time = time.monotonic()

    # Write to a temporary file first.
    temp_path = output_path.with_suffix(
        output_path.suffix + ".tmp"
    )

    try:
        with (
            input_path.open("r") as input_file,
            temp_path.open("w") as output_file,
        ):
            for line_number, line in enumerate(
                input_file,
                start=1,
            ):
                stripped = line.strip()

                # Preserve blank lines.
                if not stripped:
                    output_file.write(line)
                    continue

                match = ENTRY_RE.match(stripped)

                if not match:
                    print(
                        f"\nWARNING: Could not parse line "
                        f"{line_number}: {stripped}",
                        file=sys.stderr,
                    )

                    output_file.write(line)
                    skipped += 1
                    continue

                fen = match.group("fen")
                old_eval = float(match.group("eval"))

                try:
                    new_eval, buffer = evaluate_position(
                        sock,
                        buffer,
                        fen,
                    )
                except Exception as exc:
                    print(
                        f"\nERROR on position "
                        f"{line_number}: {exc}",
                        file=sys.stderr,
                    )

                    print(
                        f"FEN: {fen}",
                        file=sys.stderr,
                    )

                    raise

                output_file.write(
                    f"[{fen} {format_eval(new_eval)}]\n"
                )

                processed += 1

                if abs(new_eval - old_eval) > 0.00005:
                    changed += 1

                if (
                    processed % progress_interval == 0
                    or processed == total
                ):
                    elapsed = time.monotonic() - start_time

                    rate = (
                        processed / elapsed
                        if elapsed > 0
                        else 0
                    )

                    remaining = (
                        (total - processed) / rate
                        if rate > 0
                        else 0
                    )

                    print(
                        f"\r{processed:,}/{total:,} "
                        f"({processed / total * 100:6.2f}%) | "
                        f"{rate:.1f} pos/s | "
                        f"ETA {remaining / 60:.1f} min",
                        end="",
                        flush=True,
                    )

        # Only replace the destination after the entire
        # dataset was successfully processed.
        temp_path.replace(output_path)

    except Exception:
        # Leave the temporary file around for debugging/
        # recovery, but don't overwrite the requested output.
        print(
            f"\nProcessing failed. Partial output is at:"
            f"\n  {temp_path}",
            file=sys.stderr,
        )
        raise

    finally:
        sock.close()

    elapsed = time.monotonic() - start_time

    print()
    print()
    print("Done.")
    print(f"Processed: {processed:,}")
    print(f"Changed:   {changed:,}")
    print(f"Skipped:   {skipped:,}")
    print(f"Time:      {elapsed / 3600:.2f} hours")

    if elapsed > 0:
        print(
            f"Rate:      "
            f"{processed / elapsed:.2f} positions/sec"
        )


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Replace dataset evaluations with "
            "FastKat's raw UCI 'eval' evaluation."
        )
    )

    parser.add_argument(
        "input",
        type=Path,
        help="Existing dataset file",
    )

    parser.add_argument(
        "output",
        type=Path,
        help="Output dataset file",
    )

    parser.add_argument(
        "--host",
        default=DEFAULT_HOST,
        help=f"UCI server host "
             f"(default: {DEFAULT_HOST})",
    )

    parser.add_argument(
        "--port",
        type=int,
        default=DEFAULT_PORT,
        help=f"UCI server port "
             f"(default: {DEFAULT_PORT})",
    )

    parser.add_argument(
        "--progress",
        type=int,
        default=1000,
        help=(
            "Print progress every N positions "
            "(default: 1000)"
        ),
    )

    args = parser.parse_args()

    if not args.input.exists():
        print(
            f"ERROR: Input file does not exist: "
            f"{args.input}",
            file=sys.stderr,
        )
        sys.exit(1)

    if args.input.resolve() == args.output.resolve():
        print(
            "ERROR: Input and output must be different files.",
            file=sys.stderr,
        )
        sys.exit(1)

    process_file(
        args.input,
        args.output,
        args.host,
        args.port,
        args.progress,
    )


if __name__ == "__main__":
    main()
