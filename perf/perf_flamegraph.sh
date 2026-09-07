#!/bin/bash

set -e

FLAMEGRAPH="$HOME/FlameGraph"
OUTPUT="flamegraph.svg"
CHESS="../build/Chess"

echo "========================================"
echo " Chess Flame Graph Profiler"
echo "========================================"
echo

# Check that Chess exists
if [ ! -x "$CHESS" ]; then
    echo "Error: $CHESS was not found or is not executable."
    exit 1
fi

# Check FlameGraph
if [ ! -f "$FLAMEGRAPH/stackcollapse-perf.pl" ]; then
    echo "Error: FlameGraph was not found at:"
    echo "  $FLAMEGRAPH"
    exit 1
fi

# Check perf
if ! command -v perf &> /dev/null; then
    echo "Error: perf is not installed."
    exit 1
fi

#echo "Setting perf_event_paranoid to -1..."
#sudo sysctl -w kernel.perf_event_paranoid=-1

echo
echo "========================================"
echo " Starting Chess under perf"
echo "========================================"
echo
echo "Chess will continuously run matches."
echo "Press Ctrl+C when you have collected enough data."
echo

# Record performance data
perf record -F 99 -g -- "$CHESS"

echo
echo "Converting perf data..."
perf script > out.perf

echo "Folding stack traces..."
"$FLAMEGRAPH/stackcollapse-perf.pl" out.perf > out.folded

echo "Generating Flame Graph..."
"$FLAMEGRAPH/flamegraph.pl" out.folded > "$OUTPUT"

echo
echo "========================================"
echo " Flame Graph generated!"
echo "========================================"
echo
echo "File: $(realpath "$OUTPUT")"
echo

if command -v firefox &> /dev/null; then
    firefox "$OUTPUT" &> /dev/null &
fi