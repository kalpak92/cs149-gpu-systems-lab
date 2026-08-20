#!/usr/bin/env python3

"""Keep the GPU observable for a short host-side state sampling window."""

from __future__ import annotations

import argparse
import time

import torch


MATRIX_SIZE = 4096


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--duration-seconds", type=float, default=12.0)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    if args.duration_seconds <= 0:
        raise ValueError("--duration-seconds must be positive")

    a = torch.randn(MATRIX_SIZE, MATRIX_SIZE, device="cuda", dtype=torch.float32)
    b = torch.randn(MATRIX_SIZE, MATRIX_SIZE, device="cuda", dtype=torch.float32)
    output = torch.empty_like(a)

    allocated_mib = torch.cuda.memory_allocated() / (1024**2)
    print(f"allocated tensor memory: {allocated_mib:.0f} MiB", flush=True)

    deadline = time.monotonic() + args.duration_seconds
    while time.monotonic() < deadline:
        torch.mm(a, b, out=output)
        torch.cuda.synchronize()

    print("sustained CUDA workload complete", flush=True)


if __name__ == "__main__":
    main()
