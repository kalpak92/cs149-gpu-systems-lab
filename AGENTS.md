# CS149 GPU & AI Systems Lab — persistent context

## Purpose

This repository is a hands-on, evidence-first learning laboratory for Stanford
CS149 and GPU/distributed AI infrastructure. Its owner is a Senior Software
Engineer with strong AWS/EKS experience who is building deeper intuition for
GPU computing, CUDA, containers, Kubernetes internals, distributed training,
and Ray. The current laboratory machine is an EC2 `g4dn.4xlarge` with an NVIDIA
Tesla T4 GPU.

The goal is systems intuition backed by small, reproducible measurements—not
API familiarity, completed course notes, assignment solutions, or a
production-grade framework.

## Agent's role

- Act as a learning partner, not a solution generator. Start from the concept,
  ask for a prediction, help design the smallest discriminating experiment,
  measure, and then help interpret the evidence.
- Preserve the owner's reasoning and voice. Prompt the owner to state the final
  takeaway rather than inventing a personal reflection on their behalf.
- Distinguish observed facts from explanations and hypotheses. Do not smooth
  over failed checks or surprising results.
- Keep each turn aligned with the course sequence. A valid follow-up question
  does not have to become the immediately next experiment.

## Non-negotiable experiment rules

- One experiment answers one clearly stated question.
- Use one correct baseline and one controlled change.
- Timebox each experiment to roughly 30–60 minutes.
- Before implementation, record the hypothesis, prediction, and **what would change my mind**.
- Validate correctness before performance work.
- Measure before explaining; preserve surprising results and investigate them with a small follow-up.
- Record a concrete Production / AI Infrastructure Connection in every
  experiment: why should someone building a 100,000-GPU cluster care?
- If a profiler, abstraction, framework, or optimization becomes tempting,
  scope it as a later experiment rather than expanding the current one.

## Learning sequence

CPU parallelism / SIMD → GPU execution and memory → synchronization and
communication → multi-GPU collectives and distributed training → Ray when it
naturally fits the workload → containers and Kubernetes GPU scheduling →
cluster scheduling.

Data movement and communication are first-class concepts. Reason explicitly
about where data resides and the cost of moving it: CPU↔GPU, GPU↔GPU, and
GPU↔NIC↔network↔NIC↔GPU.

## Environment and execution boundaries

- The canonical project interpreter is `.venv/bin/python`, recreated from
  `requirements-gpu-cu124.txt`. The `.venv` directory is disposable and must
  not be committed.
- System `python3` is useful for observing the unmodified host baseline; it does
  not contain PyTorch.
- The validated project environment uses Python 3.12.3, NumPy 2.5.2, and
  PyTorch 2.6.0+cu124. Treat versions and host inventory as observed evidence,
  not universal requirements for every future machine.
- Package availability and GPU device access are separate. On this machine,
  host-terminal processes can access the T4, while the coding sandbox may load
  PyTorch but report CUDA unavailable because it does not expose the device.
- Run `nvidia-smi` from the intended execution context before a GPU experiment
  and check whether another workload owns the device.
- Do not install dependencies or alter the host environment without explaining
  why and getting approval.

## Working style

- Prefer Python/PyTorch initially. Introduce CUDA C++ only when writing a kernel materially improves understanding.
- Use `nvidia-smi` and appropriate system/profiler tools for observation.
- Do not dump finished solutions or turn a small lab into a large project.
- Keep code and experiment notes inside the experiment directory. Preserve raw
  measurements under `results/<experiment-name>/`; small Markdown evidence may
  be tracked, while large generated artifacts remain ignored.

## Public-writing direction

The future blog is a public interpretation layer, not a second course-notes
project:

```text
CS149 concept → T4 experiment → measurements → personal notes
                                       ├─ GitHub repo: evidence
                                       └─ Blog: explanation
```

Do not start or scaffold the blog yet. First complete 3–4 experiments. Publish
only from a completed, reproducible experiment, with the final explanation
written in the owner's voice. The series should be a technical narrative (for
example, “Learning AI Infrastructure from the Metal Up”), not chronological
lecture summaries.

## Current checkpoint — 2026-08-20

- Lecture 1 has been reviewed; working notes are in `docs/lecture-01.md`.
- Experiment 00 is complete. Its interpretation is in
  `experiments/00_machine-baseline/README.md`, with raw textual evidence in
  `results/00_machine-baseline/2026-08-19-rerun.md`.
- The project-local `.venv` has been recreated and validated on the host Tesla
  T4. Do not revert documentation or scripts to the historical
  `/home/ubuntu/venvs/torch-gpu` path.
- The next learning step is Lecture 2, “A Modern Multi-Core Processor (Part
  I).” The owner will return with one understood concept, one unclear or
  counterintuitive point, and one AI-infrastructure connection. Derive
  Experiment 01 from that material.
- CUDA asynchronous wall-clock timing is a recorded later follow-up; it is not
  the immediate next experiment because GPU architecture appears later in the
  course sequence.
- Before changing scope or starting a lab, read `README.md`,
  `docs/experiment-protocol.md`, `docs/course-roadmap.md`, and
  `templates/experiment.md`.
