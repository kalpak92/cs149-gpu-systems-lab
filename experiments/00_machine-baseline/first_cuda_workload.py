#!/usr/bin/env python3

"""Validate one PyTorch operation against a CPU reference on the Tesla T4."""

from __future__ import annotations

import torch


MATRIX_SIZE = 512
RTOL = 1e-4
ATOL = 1e-4


def main() -> None:
    torch.manual_seed(0)

    # With no device argument, PyTorch creates these tensors on the CPU.
    a_cpu = torch.randn(MATRIX_SIZE, MATRIX_SIZE, dtype=torch.float32)
    b_cpu = torch.randn(MATRIX_SIZE, MATRIX_SIZE, dtype=torch.float32)
    expected_cpu = a_cpu @ b_cpu
    reference_cpu64 = a_cpu.to(torch.float64) @ b_cpu.to(torch.float64)

    # Copy the exact same inputs to the GPU so device placement is the only change.
    a_cuda = a_cpu.to("cuda")
    b_cuda = b_cpu.to("cuda")
    actual_cuda = a_cuda @ b_cuda

    # The device-to-host copy waits for the CUDA result and makes it comparable.
    actual_cpu = actual_cuda.cpu()
    cpu_error = (expected_cpu.to(torch.float64) - reference_cpu64).abs()
    cuda_error = (actual_cpu.to(torch.float64) - reference_cpu64).abs()
    pairwise_difference = (actual_cpu - expected_cpu).abs()
    close_mask = torch.isclose(actual_cpu, expected_cpu, rtol=RTOL, atol=ATOL)
    mismatch_count = close_mask.numel() - close_mask.count_nonzero().item()

    print(f"input device without an explicit device argument: {a_cpu.device}")
    print(f"CUDA result device: {actual_cuda.device}")
    print(f"copied result device: {actual_cpu.device}")
    print(f"CPU float32 maximum error versus float64: {cpu_error.max().item():.8g}")
    print(f"CUDA float32 maximum error versus float64: {cuda_error.max().item():.8g}")
    print(f"CPU float32 mean error versus float64: {cpu_error.mean().item():.8g}")
    print(f"CUDA float32 mean error versus float64: {cuda_error.mean().item():.8g}")
    print(f"CPU/CUDA maximum absolute difference: {pairwise_difference.max().item():.8g}")
    print(f"mismatches with rtol={RTOL} and atol={ATOL}: {mismatch_count}")

    torch.testing.assert_close(actual_cpu, expected_cpu, rtol=RTOL, atol=ATOL)
    print("correct within the stated tolerance: True")


if __name__ == "__main__":
    main()
