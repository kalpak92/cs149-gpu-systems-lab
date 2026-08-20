#!/usr/bin/env bash

set -euo pipefail

readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly PROJECT_ROOT="$(cd -- "$SCRIPT_DIR/../.." && pwd)"
readonly PYTHON_BIN="${PYTHON_BIN:-$PROJECT_ROOT/.venv/bin/python}"
readonly DURATION_SECONDS="${DURATION_SECONDS:-12}"
readonly WORKLOAD_SCRIPT="$SCRIPT_DIR/sustained_cuda_workload.py"

sample_gpu() {
    nvidia-smi \
        --query-gpu=timestamp,memory.used,utilization.gpu,temperature.gpu,pstate \
        --format=csv,noheader
}

echo "baseline:"
sample_gpu

"$PYTHON_BIN" "$WORKLOAD_SCRIPT" --duration-seconds "$DURATION_SECONDS" &
readonly workload_pid=$!
echo "workload PID: $workload_pid"

while kill -0 "$workload_pid" 2>/dev/null; do
    sample_gpu
    sleep 1
done

wait "$workload_pid"

echo "immediately after process exit:"
sample_gpu
sleep 2
echo "two seconds after process exit:"
sample_gpu
