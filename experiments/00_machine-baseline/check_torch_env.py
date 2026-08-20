#!/usr/bin/env python3

"""Check whether this Python environment can see Torch and the Tesla T4."""

from __future__ import annotations

import torch


def main() -> None:
    print(f"torch: {torch.__version__}")
    print(f"torch CUDA build: {torch.version.cuda}")
    print(f"cuda available: {torch.cuda.is_available()}")
    if torch.cuda.is_available():
        print(f"device: {torch.cuda.get_device_name(0)}")


if __name__ == "__main__":
    main()