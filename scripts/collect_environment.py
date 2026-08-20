#!/usr/bin/env python3
"""Print a compact, read-only inventory for a GPU lab experiment."""

from __future__ import annotations

import platform
import subprocess
import sys


def run(command: list[str]) -> str:
    try:
        return subprocess.check_output(command, text=True, stderr=subprocess.STDOUT).strip()
    except (FileNotFoundError, subprocess.CalledProcessError) as error:
        return f"unavailable ({error})"


print(f"python: {sys.version.split()[0]}")
print(f"platform: {platform.platform()}")
print(f"kernel: {platform.release()}")
print(f"cpu count: {__import__('os').cpu_count()}")
print("nvidia-smi:")
print(run(["nvidia-smi", "--query-gpu=name,driver_version,memory.total,temperature.gpu", "--format=csv,noheader"]))

try:
    import torch

    print(f"torch: {torch.__version__}")
    print(f"torch CUDA build: {torch.version.cuda}")
    print(f"cuda available: {torch.cuda.is_available()}")
    if torch.cuda.is_available():
        print(f"torch device: {torch.cuda.get_device_name(0)}")
        print(f"device capability: {torch.cuda.get_device_capability(0)}")
except ImportError:
    print("torch: unavailable (not installed in this Python environment)")
