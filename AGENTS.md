# CS149 GPU & AI Systems Lab — persistent context

## Purpose

This repository is a hands-on, evidence-first learning laboratory for Stanford CS149 and GPU/distributed AI infrastructure. Its owner is a Senior Software Engineer with strong AWS/EKS experience, building deeper intuition for GPU computing, CUDA, containers, Kubernetes internals, distributed training, and Ray. The laboratory machine is an EC2 instance with an NVIDIA Tesla T4 GPU.

The goal is systems intuition backed by small, reproducible measurements—not API familiarity, completed course notes, or a production-grade framework.

## Non-negotiable experiment rules

- One experiment answers one clearly stated question.
- Use one correct baseline and one controlled change.
- Timebox each experiment to roughly 30–60 minutes.
- Before implementation, record the hypothesis, prediction, and **what would change my mind**.
- Validate correctness before performance work.
- Measure before explaining; preserve surprising results and investigate them with a small follow-up.
- Record a concrete Production / AI Infrastructure Connection in every experiment: why should someone building a 100,000-GPU cluster care?
- If a profiler, abstraction, framework, or optimization becomes tempting, scope it as the next experiment rather than expanding the current one.

## Learning sequence

CPU parallelism / SIMD → GPU execution and memory → synchronization and communication → multi-GPU collectives and distributed training → Ray when it naturally fits the workload → containers and Kubernetes GPU scheduling → cluster scheduling.

Data movement and communication are first-class concepts. Reason explicitly about where data resides and the cost of moving it: CPU↔GPU, GPU↔GPU, and GPU↔NIC↔network↔NIC↔GPU.

## Working style

- Prefer Python/PyTorch initially. Introduce CUDA C++ only when writing a kernel materially improves understanding.
- Use `nvidia-smi` and appropriate system/profiler tools for observation.
- Do not install new dependencies or alter the host environment without explaining why and getting approval.
- Do not dump finished solutions or turn a small lab into a large project.
- Keep code, commands, measurements, and notes within the experiment directory; generated artifacts belong under `results/`.

## Public-writing direction

The future blog is a public interpretation layer, not a second course-notes project:

```text
CS149 concept → T4 experiment → measurements → personal notes
                                       ├─ GitHub repo: evidence
                                       └─ Blog: explanation
```

Do not start or scaffold the blog yet. First complete 3–4 experiments. Publish only from a completed, reproducible experiment, with the final explanation written in the owner’s voice. The series should be a technical narrative (for example, “Learning AI Infrastructure from the Metal Up”), not chronological lecture summaries.

## Current state

- Scaffold exists; begin with `experiments/00_machine-baseline/README.md`.
- Host inventory observed: 16 CPUs, Tesla T4 (15 GiB VRAM), NVIDIA driver 580.173.02. The system `python3` has no PyTorch; identify the intended virtual environment before installing anything.
- Read `README.md`, `docs/experiment-protocol.md`, `docs/course-roadmap.md`, and `templates/experiment.md` before changing scope or starting a lab.
