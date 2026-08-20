# 00 — Machine baseline

**Status:** complete

**Original timebox:** 30 minutes

**Collaborative rerun:** 2026-08-19 and 2026-08-20

## Completion summary

The inventory, correctness check, and host-observation checkpoints are
complete. Raw commands and outputs are preserved in
[`results/00_machine-baseline/2026-08-19-rerun.md`](../../results/00_machine-baseline/2026-08-19-rerun.md).

Observed:

- Recreated the validated environment at the project-local `.venv`; it has
  NumPy 2.5.2 and PyTorch 2.6.0+cu124, sees the Tesla T4 from the host, and is
  now the canonical lab environment.
- Confirmed that system Python and the canonical interpreter see the same host,
  while only the canonical interpreter contains PyTorch.
- Confirmed an execution-context boundary: the canonical interpreter sees the
  T4 from the host terminal, but the coding sandbox does not expose the device.
- Confirmed that tensors created without a device argument reside on the CPU.
- Compared the same `512x512 float32` matrix multiplication on CPU and CUDA.
- Preserved the initial failure at `rtol=1e-4`, `atol=1e-5` (10 of 262,144
  elements failed), then compared both results with a `float64` CPU reference.
- The CPU and CUDA `float32` errors were of the same order. The check passed
  with the workload-specific criterion `rtol=1e-4`, `atol=1e-4`.
- Observed a sustained CUDA workload with one-second `nvidia-smi` samples:
  tensor storage was 192 MiB, process-level GPU memory was 329 MiB,
  utilization reached 100%, temperature rose from 28 C to 39 C, and P-state
  changed from P8 to P0.
- After process exit, memory returned immediately to 0 MiB; utilization fell
  to 0% after the sampling window, while temperature and P-state decayed more
  slowly.
- Clarified that 100% `utilization.gpu` means at least one kernel executed
  throughout the sample period. It does not prove that all GPU cores were
  occupied or efficiently used.

The experiment is complete. CUDA asynchronous timing remains a separate
follow-up rather than an expansion of this baseline.

## 1. Question

What hardware and software does this laboratory actually expose to a PyTorch
program, and what does a first GPU operation look like from the host's
perspective?

## 2. Concept

GPU usability has several layers: physical hardware, the host driver, device
access granted to the process, the selected Python environment, and the
framework's CUDA build. Passing one layer does not prove that the next layer is
usable. An idle GPU reading provides the baseline against which workload
behavior can be compared.

## 3. Hypothesis and prediction

- **Hypothesis:** The host has a usable NVIDIA GPU, but system Python cannot
  submit PyTorch work because PyTorch is unavailable there. A dedicated
  environment should expose a CUDA-enabled PyTorch build when the process also
  has access to the GPU device.
- **Prediction:** PyTorch will create tensors on the CPU by default. Moving the
  same inputs to CUDA will produce a numerically close result and increase GPU
  memory use, utilization, temperature, and performance state while work is in
  flight.
- **What would change my mind?** A canonical environment that failed to load
  PyTorch on the host, a CUDA result whose error was materially worse than the
  CPU `float32` error, or a sustained synchronized workload that produced no
  observable host GPU state change would contradict the hypothesis.

The starting prompt also predicted that CUDA launches may be asynchronous from
Python. That was not measured here; it is intentionally the follow-up
experiment rather than an extra claim in this one.

## 4. Smallest useful experiment

- **Baseline:** Idle host inventory plus a `512x512 float32` matrix
  multiplication on the CPU.
- **One controlled change:** Copy the exact same inputs to CUDA and perform the
  same matrix multiplication there.
- **Correctness oracle:** Compare CPU and CUDA results directly, and compare
  each `float32` result with a `float64` CPU reference. State the tolerance.
- **Measurement method:** Record interpreter/framework visibility, tensor
  placement, numerical error, and one-second `nvidia-smi` samples during a
  sustained version of the same kind of CUDA operation. Do not make a timing or
  speedup claim.

## 5. Implementation log

Interpreter inventories from the coding sandbox:

```bash
python3 scripts/collect_environment.py
/home/ubuntu/venvs/torch-gpu/bin/python scripts/collect_environment.py
```

Host-terminal environment and GPU checks:

```bash
nvidia-smi --query-gpu=name,driver_version,memory.total,temperature.gpu --format=csv,noheader
.venv/bin/python experiments/00_machine-baseline/check_torch_env.py
```

Correctness workload and host-side sampling:

```bash
.venv/bin/python experiments/00_machine-baseline/first_cuda_workload.py
bash experiments/00_machine-baseline/sample_gpu_state.sh
```

No dependencies were installed.

GPU commands were run from the host terminal because the coding sandbox does
not expose the NVIDIA device. The raw record keeps the two execution contexts
separate.

## 6. Result

### Environment visibility

```text
Host: EC2 g4dn.4xlarge, 16 logical CPUs
GPU: Tesla T4, 15360 MiB
Driver: 580.173.02
System Python: Python 3.12.3; torch unavailable
Canonical Python: project .venv; Python 3.12.3; NumPy 2.5.2
Canonical PyTorch: torch 2.6.0+cu124; CUDA build 12.4
```

From the host terminal, the canonical interpreter reported CUDA available and
identified the Tesla T4. In the coding sandbox, the same interpreter loaded
PyTorch but reported CUDA unavailable, and `nvidia-smi` exited with status 9.
This is evidence about device exposure, not a contradiction in GPU health.

The original run used `/home/ubuntu/venvs/torch-gpu`. Recreating its direct
dependencies in the project-local `.venv` produced the same device visibility,
numerical errors, and passing correctness result. The environment can now be
recreated from the committed requirements file rather than a host-specific
path.

### Correctness

```text
Initial criterion: rtol=1e-4, atol=1e-5
Mismatches: 10 / 262144
CPU float32 maximum error versus float64: 5.6197562e-05
CUDA float32 maximum error versus float64: 8.9566939e-05
CPU/CUDA maximum absolute difference: 9.1552734e-05

Revised criterion: rtol=1e-4, atol=1e-4
Mismatches: 0
Correct within the stated tolerance: True
```

The failed check was retained. Comparing each `float32` result with the
`float64` reference showed errors of the same order. Near zero, the relative
tolerance contributes almost nothing, making the first absolute tolerance too
strict for this workload.

### Host observation

```text
Idle:       0 MiB,   0% utilization, 28 C, P8
Active:   329 MiB, 100% utilization, 39 C, P0
After:      0 MiB,   0% utilization, 36 C, P0 (two seconds later)
```

The three preallocated `4096x4096 float32` tensors accounted for 192 MiB. The
process used 329 MiB in total, showing framework/runtime overhead beyond tensor
storage. Device memory returned to zero immediately after process exit, while
temperature and P-state decayed more slowly.

## 7. Explanation

The driver-reported CUDA version describes driver compatibility; it does not
prove that PyTorch is installed in a particular interpreter. Likewise, a
CUDA-enabled PyTorch package does not prove that its process can access the GPU.
The host terminal and coding sandbox demonstrated these layers separately.

CPU and GPU matrix multiplication can follow different floating-point
instruction and accumulation paths. Exact equality is therefore the wrong
oracle. The higher-precision reference helped distinguish an expected rounding
difference from a grossly incorrect CUDA result.

The one-second `nvidia-smi` samples show device lifecycle and coarse activity.
`100% utilization.gpu` means at least one kernel was executing throughout the
sample window; it does not establish full core occupancy or efficient use.

## 8. What I learned

- I learned that hardware presence, driver compatibility, Python package
  availability, and process-level access to the GPU are distinct checks.
- Baselining the same matrix multiplication on the CPU and GPU made
  floating-point precision concrete. The results did not need to be exactly
  equal to be correct; they needed a justified tolerance and a higher-precision
  reference that could discriminate expected rounding from a bad result.
- I learned that `100% utilization.gpu` shows continuous kernel activity during
  the sample window, not necessarily full or efficient use of every GPU core.
- Seeing 192 MiB of tensor storage but 329 MiB of process-level GPU memory made
  the additional CUDA/PyTorch process overhead visible. That extra footprint
  can include the CUDA context, allocator, runtime, and loaded library state.

One uncertainty remains: `nvidia-smi` measured the total 329 MiB footprint but
did not attribute the additional 137 MiB to individual components. Explaining
that breakdown would require a separate measurement rather than assuming it
was entirely CUDA libraries.

## 9. Production / AI Infrastructure Connection

A GPU node can be healthy and provisioned while a workload cannot use its
accelerator because the container or sandbox does not expose the device, the
runtime environment is incomplete, or the framework build is incompatible. At
100,000-GPU scale, a small percentage of nodes with one broken layer becomes a
large pool of stranded capacity. Health checks therefore need to distinguish
physical inventory, driver/device exposure, framework usability, and actual
workload activity. Telemetry such as `utilization.gpu` must also be interpreted
carefully before making scheduling or efficiency decisions.

## 10. One follow-up experiment

How much does CUDA's asynchronous execution distort naive Python wall-clock
timing of a GPU operation, and what changes when explicit synchronization is
added?
