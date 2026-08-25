#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly SCRIPT_DIR
readonly SOURCE_FILE="$SCRIPT_DIR/memory_locality.cpp"
readonly CXX_BIN="${CXX:-g++}"
readonly BINARY_PATH="/tmp/cs149-experiment-03-memory-locality"
readonly VECTOR_REPORT="/tmp/cs149-experiment-03-vectorization.txt"
readonly ROWS="${ROWS:-1000000}"
readonly COLUMNS="16"
readonly REPETITIONS="${REPETITIONS:-7}"

echo "compiler: $($CXX_BIN --version | head -n 1)"
echo "source: $SOURCE_FILE"
echo "temporary binary: $BINARY_PATH"
echo "rows: $ROWS"
echo "columns: $COLUMNS"
echo "repetitions: $REPETITIONS"
echo "OpenMP placement: one thread, OMP_PLACES=cores, OMP_PROC_BIND=spread"

# SIMD vectorization and loop interchange remain disabled in both traversal
# paths. This prevents the compiler from changing vector width or silently
# swapping the column-major loop nesting, which would add another variable to
# the memory-access comparison.
"$CXX_BIN" \
    -std=c++20 \
    -O3 \
    -march=native \
    -fopenmp \
    -Wall \
    -Wextra \
    -Wpedantic \
    -fno-tree-loop-vectorize \
    -fno-tree-slp-vectorize \
    -fno-loop-interchange \
    -fopt-info-vec-all="$VECTOR_REPORT" \
    "$SOURCE_FILE" \
    -o "$BINARY_PATH"

# The report must not identify any loop in this source as vectorized. This is a
# fixed experimental guard—not the controlled change measured in Experiment 03.
if grep -E 'memory_locality\.cpp:.*loop vectorized' "$VECTOR_REPORT"; then
    echo "error: a source loop was unexpectedly vectorized" >&2
    exit 1
fi
echo "compiler evidence: no source loop was reported as vectorized"

# These variables apply only to the benchmark process and do not alter the
# user's shell. One OpenMP worker keeps multi-core scaling out of this test.
OMP_DYNAMIC=FALSE \
OMP_NUM_THREADS=1 \
OMP_PLACES=cores \
OMP_PROC_BIND=spread \
"$BINARY_PATH" \
    --rows "$ROWS" \
    --columns "$COLUMNS" \
    --repetitions "$REPETITIONS"
