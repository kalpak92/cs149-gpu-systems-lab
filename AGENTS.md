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

## Current checkpoint — 2026-08-25

- Lecture 1 has been reviewed; working notes are in `docs/lecture-01.md`.
- Lecture 2 has been reviewed in `docs/lecture-02.md`. The owner stated the
  conceptual distinctions among ILP, SIMD, multi-core execution, and hardware
  multithreading; identified real CPU scaling as the practical uncertainty;
  and connected host preprocessing throughput to GPU utilization.
- Experiment 00 is complete. Its interpretation is in
  `experiments/00_machine-baseline/README.md`, with raw textual evidence in
  `results/00_machine-baseline/2026-08-19-rerun.md`.
- The project-local `.venv` has been recreated and validated on the host Tesla
  T4. Do not revert documentation or scripts to the historical
  `/home/ubuntu/venvs/torch-gpu` path.
- Experiment 01 measured one fixed compute-heavy OpenMP loop with `1`, `2`,
  `4`, and `8` physical-core workers. The pre-recorded prediction expected
  roughly `3x` speedup at four threads and `4–6x` at eight. The longer check
  instead observed `3.990x` and `7.964x`, with zero correctness mismatches.
  Code and notes are in `experiments/01_cpu-thread-scaling/`; raw evidence is
  in `results/01_cpu-thread-scaling/2026-08-23-run.md`.
- Experiment 01 is complete. The owner concluded that OpenMP made the
  independent iterations straightforward to distribute but did not create the
  parallelism or guarantee speedup; the favorable result depended on balanced,
  compute-heavy work, static scheduling, and one worker per physical core, and
  should not be generalized to memory-bound, imbalanced, or synchronized work.
- Experiment 02 compiled the same 128-stage loop twice at one OpenMP thread.
  The scalar baseline disabled the loop vectorizer; the changed binary enabled
  it. The owner predicted approximately `8x`, with less than `4x` and more than
  `8x` as the pre-recorded thresholds. Compiler reports and disassembly showed
  scalar `vfmadd132ss` instructions versus 256-bit packed `vfmadd132ps`
  instructions, correctness passed, and the measured median speedup was
  `7.881x`. Code and notes are in `experiments/02_simd-vectorization/`; raw
  evidence is in `results/02_simd-vectorization/2026-08-23-run.md`.
- Experiment 02 is complete. The owner concluded that eight float lanes
  produced nearly eight times the throughput and made it concrete that one
  instruction stream can process several independent elements. They also
  distinguished this from Experiment 01's separate worker instruction streams
  on multiple cores and noted that dependencies, branches, irregular memory
  access, or memory-bandwidth pressure can prevent an eight-lane instruction
  from producing `8x` application speedup.
- The next learning step is to define one narrow Experiment 03 question about
  memory locality. Record the owner's prediction and what would change their
  mind before implementation; do not start from a finished implementation.
- CUDA asynchronous wall-clock timing is a recorded later follow-up; it is not
  the immediate next experiment because GPU architecture appears later in the
  course sequence.
- Before changing scope or starting a lab, read `README.md`,
  `docs/experiment-protocol.md`, `docs/course-roadmap.md`, and
  `templates/experiment.md`.
