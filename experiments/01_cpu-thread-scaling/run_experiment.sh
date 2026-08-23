#!/usr/bin/env bash

set -euo pipefail

readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly SOURCE_FILE="$SCRIPT_DIR/thread_scaling.cpp"
readonly CXX_BIN="${CXX:-g++}"
readonly BINARY_PATH="/tmp/cs149-experiment-01-thread-scaling"
readonly VECTOR_REPORT="/tmp/cs149-experiment-01-vectorization.txt"
readonly ELEMENTS="${ELEMENTS:-8000000}"
readonly REPETITIONS="${REPETITIONS:-7}"

echo "compiler: $($CXX_BIN --version | head -n 1)"
echo "source: $SOURCE_FILE"
echo "temporary binary: $BINARY_PATH"
echo "elements: $ELEMENTS"
echo "repetitions: $REPETITIONS"
echo "OpenMP placement: OMP_PLACES=cores, OMP_PROC_BIND=spread"

# Keep every optimization flag fixed during Experiment 01. The vectorization
# report is diagnostic evidence that will help design Experiment 02; it is not
# a second controlled change in this thread-scaling measurement.
"$CXX_BIN" \
    -std=c++20 \
    -O3 \
    -march=native \
    -fopenmp \
    -Wall \
    -Wextra \
    -Wpedantic \
    -fopt-info-vec-optimized="$VECTOR_REPORT" \
    "$SOURCE_FILE" \
    -o "$BINARY_PATH"

echo "optimized-vectorization report:"
# Limit the displayed report to loops in this source file. GCC also reports
# vectorization inside C++ standard-library headers, which is true but not
# useful evidence for understanding this experiment's measured loop.
grep -E 'thread_scaling\.cpp:.*loop vectorized' "$VECTOR_REPORT"

# These variables apply only to the benchmark child process; they do not alter
# the user's shell or the host configuration. "cores" supplies one OpenMP
# place per physical core, while "spread" distributes workers across places.
OMP_DYNAMIC=FALSE \
OMP_PLACES=cores \
OMP_PROC_BIND=spread \
"$BINARY_PATH" \
    --elements "$ELEMENTS" \
    --repetitions "$REPETITIONS"
