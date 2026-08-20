# Laboratory environment

Observed during Experiment 00 on 2026-08-19 and 2026-08-20. Raw commands and
outputs are preserved in
[`results/00_machine-baseline/2026-08-19-rerun.md`](../results/00_machine-baseline/2026-08-19-rerun.md).

| Item | Observed record |
| --- | --- |
| Instance type / CPU model | EC2 `g4dn.4xlarge`; Intel Xeon Platinum 8259CL at 2.50 GHz; 8 cores, 16 logical CPUs |
| NVIDIA driver and CUDA compatibility | Driver `580.173.02`; host `nvidia-smi` reported CUDA 13.0 compatibility |
| GPU model and VRAM | NVIDIA Tesla T4; 15,360 MiB |
| Python, PyTorch, and CUDA build | Python 3.12.3; canonical interpreter `/home/ubuntu/cs149-gpu-systems-lab/.venv/bin/python`; NumPy 2.5.2; PyTorch `2.6.0+cu124`; PyTorch CUDA build 12.4 |
| OS and kernel | Ubuntu 24.04.4 LTS; Linux `7.0.0-1010-aws`; x86_64 |
| Docker / NVIDIA Container Toolkit | Docker 28.1.1; `nvidia-ctk` was not found on `PATH` |
| Kubernetes / EKS context | `kubectl` was not found on `PATH`; not otherwise assessed in Experiment 00 |
| Ray version | `ray` was not found on `PATH`; not otherwise assessed in Experiment 00 |

## Execution-context boundary

The host terminal and coding sandbox expose different views of the same
machine:

| Context | `nvidia-smi` | PyTorch in canonical interpreter |
| --- | --- | --- |
| Host terminal | Tesla T4 visible | `cuda available: True` |
| Coding sandbox | Query exits with status 9 | Package loads, but `cuda available: False` |

This distinction matters: an installed CUDA-enabled framework does not by
itself grant a process access to the GPU device. Device nodes, permissions, and
the execution boundary must also expose the accelerator.

## Reproducible Python environment

The first host measurements used the pre-existing
`/home/ubuntu/venvs/torch-gpu` virtual environment. On 2026-08-20, the
environment was recreated inside the project from
`requirements-gpu-cu124.txt`. The project-local environment reported the same
PyTorch and CUDA build, detected the Tesla T4, and passed the same matrix
multiplication correctness check with zero mismatches.

The `.venv` directory itself is not portable and is ignored by Git. Recreate it
from the committed requirements file on each machine.

Safety note: treat this instance as a shared laboratory. Before a GPU
experiment, use host-terminal `nvidia-smi` to check whether a workload already
owns the device.
