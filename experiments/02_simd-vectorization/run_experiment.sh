#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly SCRIPT_DIR
readonly SOURCE_FILE="$SCRIPT_DIR/simd_vectorization.cpp"
readonly CXX_BIN="${CXX:-g++}"
readonly OBJDUMP_BIN="${OBJDUMP:-objdump}"
readonly SCALAR_BINARY="/tmp/cs149-experiment-02-scalar"
readonly VECTOR_BINARY="/tmp/cs149-experiment-02-vector"
readonly SCALAR_REPORT="/tmp/cs149-experiment-02-scalar-vectorization.txt"
readonly VECTOR_REPORT="/tmp/cs149-experiment-02-vector-vectorization.txt"
readonly SCALAR_ASSEMBLY="/tmp/cs149-experiment-02-scalar-assembly.txt"
readonly VECTOR_ASSEMBLY="/tmp/cs149-experiment-02-vector-assembly.txt"
readonly SCALAR_KERNEL_ASSEMBLY="/tmp/cs149-experiment-02-scalar-kernel-assembly.txt"
readonly VECTOR_KERNEL_ASSEMBLY="/tmp/cs149-experiment-02-vector-kernel-assembly.txt"
readonly SCALAR_OUTPUT="/tmp/cs149-experiment-02-scalar-output.txt"
readonly VECTOR_OUTPUT="/tmp/cs149-experiment-02-vector-output.txt"
readonly ELEMENTS="${ELEMENTS:-8000000}"
readonly REPETITIONS="${REPETITIONS:-7}"

# Every flag in this array is shared by both binaries. The only controlled
# compile-time change is added below: -fno-tree-vectorize for the scalar
# baseline versus GCC's normal -O3 vectorization for the changed binary.
readonly COMMON_FLAGS=(
    -std=c++20
    -O3
    -march=native
    -fopenmp
    -Wall
    -Wextra
    -Wpedantic
)

echo "compiler: $($CXX_BIN --version | head -n 1)"
echo "source: $SOURCE_FILE"
echo "elements: $ELEMENTS"
echo "repetitions: $REPETITIONS"
echo "OpenMP placement: one thread, OMP_PLACES=cores, OMP_PROC_BIND=spread"

echo "building scalar baseline with loop vectorization disabled"
"$CXX_BIN" \
    "${COMMON_FLAGS[@]}" \
    -fno-tree-vectorize \
    -fopt-info-vec-all="$SCALAR_REPORT" \
    "$SOURCE_FILE" \
    -o "$SCALAR_BINARY"

echo "building vectorized change with loop vectorization enabled"
"$CXX_BIN" \
    "${COMMON_FLAGS[@]}" \
    -fopt-info-vec-all="$VECTOR_REPORT" \
    "$SOURCE_FILE" \
    -o "$VECTOR_BINARY"

# A compiler flag is only an intention. The optimization report and disassembly
# verify that the measured loop actually changed. On this machine, scalar float
# arithmetic is marked by instructions ending in "ss"; packed float SIMD is
# marked by instructions ending in "ps".
"$OBJDUMP_BIN" -d -Mintel "$SCALAR_BINARY" > "$SCALAR_ASSEMBLY"
"$OBJDUMP_BIN" -d -Mintel "$VECTOR_BINARY" > "$VECTOR_ASSEMBLY"

# Extract only the OpenMP-outlined run_kernel function. Searching the whole
# executable could accidentally display a matching instruction from setup,
# correctness checking, or the C++ runtime instead of the measured loop.
sed -n '/<.*run_kernel.*omp_fn\.0>:/,/^$/p' \
    "$SCALAR_ASSEMBLY" > "$SCALAR_KERNEL_ASSEMBLY"
sed -n '/<.*run_kernel.*omp_fn\.0>:/,/^$/p' \
    "$VECTOR_ASSEMBLY" > "$VECTOR_KERNEL_ASSEMBLY"

measured_expression_line="$(
    grep -n -m 1 'transform_element(input' "$SOURCE_FILE" | cut -d: -f1
)"

echo "compiler evidence for the measured source loop:"
grep -F "$SOURCE_FILE:$measured_expression_line:" "$VECTOR_REPORT" \
    | grep 'loop vectorized' \
    | sort -u

if grep -F "$SOURCE_FILE:$measured_expression_line:" "$SCALAR_REPORT" \
    | grep -q 'loop vectorized'; then
    echo "error: scalar baseline unexpectedly vectorized the measured loop" >&2
    exit 1
fi
echo "scalar compiler report: no vectorized measured loop"

echo "sample scalar floating-point instructions:"
grep -E -m 3 'vfmadd[0-9]+ss' "$SCALAR_KERNEL_ASSEMBLY"

echo "sample packed floating-point instructions:"
grep -E -m 3 'vfmadd[0-9]+ps' "$VECTOR_KERNEL_ASSEMBLY"

echo "running scalar baseline"
OMP_DYNAMIC=FALSE \
OMP_NUM_THREADS=1 \
OMP_PLACES=cores \
OMP_PROC_BIND=spread \
"$SCALAR_BINARY" \
    --label scalar \
    --elements "$ELEMENTS" \
    --repetitions "$REPETITIONS" | tee "$SCALAR_OUTPUT"

echo "running vectorized change"
OMP_DYNAMIC=FALSE \
OMP_NUM_THREADS=1 \
OMP_PLACES=cores \
OMP_PROC_BIND=spread \
"$VECTOR_BINARY" \
    --label vectorized \
    --elements "$ELEMENTS" \
    --repetitions "$REPETITIONS" | tee "$VECTOR_OUTPUT"

scalar_median="$(awk -F, '$1 == "SUMMARY" { print $3 }' "$SCALAR_OUTPUT")"
vector_median="$(awk -F, '$1 == "SUMMARY" { print $3 }' "$VECTOR_OUTPUT")"
scalar_checksum="$(awk -F, '$1 == "CHECKSUM" { print $3 }' "$SCALAR_OUTPUT")"
vector_checksum="$(awk -F, '$1 == "CHECKSUM" { print $3 }' "$VECTOR_OUTPUT")"

# Report, rather than assume, the relation between vector width and speedup.
# Checksums from the two independently compiled binaries must also agree within
# the same explicit floating-point tolerance used by the benchmark.
awk \
    -v scalar="$scalar_median" \
    -v vector="$vector_median" \
    'BEGIN { printf "COMPARISON,scalar_over_vectorized_speedup,%.3f\n", scalar / vector }'

awk \
    -v scalar="$scalar_checksum" \
    -v vector="$vector_checksum" \
    'BEGIN {
        difference = scalar - vector;
        if (difference < 0) difference = -difference;
        limit = 1.0e-6 + 1.0e-5 * (scalar < 0 ? -scalar : scalar);
        printf "CROSS_CHECK,checksum_absolute_difference,%.9f\n", difference;
        if (difference > limit) exit 1;
    }'

echo "temporary reports, assembly, binaries, and raw process output are under /tmp/cs149-experiment-02-*"
