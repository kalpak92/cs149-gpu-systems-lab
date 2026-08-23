# CS149 GPU & AI Systems Lab

An evidence-first, hands-on companion to Stanford CS149. The aim is to develop
reliable intuition about parallel execution, GPU performance, and distributed
AI infrastructure through small measurements on a real NVIDIA Tesla T4—not to
collect APIs, reproduce complete course notes, or build a large framework.

This is an independent learning repository. It is not affiliated with Stanford
and does not contain assignment solutions.

## The end state

By the end of the course, this repository should contain a small, reproducible body of evidence showing that I can:

- decompose work into parallel tasks and explain the limits to speedup;
- reason about CPU threads, SIMD, GPU threads/warps/blocks, memory hierarchy, data movement, and synchronization;
- write and measure small PyTorch and CUDA experiments;
- diagnose a performance result using measurements rather than intuition alone;
- connect a single-GPU observation to production AI concerns: placement, isolation, utilization, communication, and distributed training; and
- explain the trade-offs in my own words, with the code and evidence to support it.

Read [the roadmap](docs/course-roadmap.md) before beginning. Each lab follows
[the experiment protocol](docs/experiment-protocol.md) and is recorded with
[the experiment template](templates/experiment.md).

## Current progress

| Item | Status | Record |
| --- | --- | --- |
| Lecture 1 — Why Parallelism? Why Efficiency? | Reviewed | [Working notes](docs/lecture-01.md) |
| Experiment 00 — Machine baseline | Complete | [Experiment](experiments/00_machine-baseline/README.md) · [Raw evidence](results/00_machine-baseline/2026-08-19-rerun.md) |
| GPU Python environment | Validated on the host T4 | [Environment record](docs/environment.md) |
| Next step | Lecture 2 — A Modern Multi-Core Processor (Part I) | Derive Experiment 01 after the lecture |

## Reproduce the GPU Python environment

The tested host uses Python 3.12, Linux x86_64, an NVIDIA Tesla T4, and driver
580.173.02. First confirm that the intended execution context can see an idle
GPU:

```bash
nvidia-smi
```

Then recreate the project-local virtual environment:

```bash
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements-gpu-cu124.txt
.venv/bin/python scripts/collect_environment.py
```

The environment directory is disposable and ignored by Git; the committed
[`requirements-gpu-cu124.txt`](requirements-gpu-cu124.txt) is the reproducible
input. It pins the CUDA 12.4 PyTorch build validated during Experiment 00. The
`cu124` label describes the CUDA runtime bundled with the PyTorch wheel, not the
maximum CUDA version reported by the host driver.

Recreating the Python packages does not guarantee GPU access. The destination
also needs a compatible NVIDIA driver and must expose the GPU device to the
process. A container or sandbox can load the same PyTorch package while
reporting `cuda available: False` if the device is not passed through. A
different accelerator or driver environment may require a different
requirements file.

## Repository map

```text
.agents/       Repository-scoped Codex skills for this learning workflow
docs/          Learning contract, roadmap, and environment notes
experiments/   One small, independent experiment per directory
templates/     Copyable templates for experiments and results
scripts/       Small reproducibility helpers (not a framework)
results/       Raw measurements and selected evidence
```

Codex automatically discovers skills under `.agents/skills/` when it starts
inside this repository, including after the repository is cloned to another
machine.

Small textual evidence under `results/` may be committed. Large profiler
exports, databases, and other generated artifacts are ignored.

## Render source-controlled diagrams

Static documentation diagrams keep editable Mermaid source under
`docs/diagrams/` and generated SVGs under `docs/assets/`. The repository-scoped
`pretty-mermaid` skill is vendored at a recorded upstream commit and uses a
locked Node.js dependency environment separate from the canonical Python/GPU
environment.

After explaining the dependency boundary and receiving approval, recreate the
renderer environment once:

```bash
cd .agents/skills/pretty-mermaid
npm ci
```

Then render a diagram from the repository root:

```bash
node .agents/skills/pretty-mermaid/scripts/render.mjs \
  --input docs/diagrams/experiment-00-gpu-usability.mmd \
  --output docs/assets/experiment-00-gpu-usability-flow.svg \
  --theme zinc-light \
  --font sans-serif \
  --node-spacing 16 \
  --layer-spacing 28
```

## Working agreement

For every experiment, begin by writing a question and a prediction. **Hard
rule: one experiment answers one question, has one controlled change, and stays
within a 30–60 minute timebox.** Prefer a correct baseline, then one controlled
change, then a measurement. If a new tool, abstraction, or optimization becomes
tempting, record it as a later experiment instead of expanding the current one.
A surprising result is a prompt to investigate—not a failure.

Do not read or paste full assignment solutions. This lab is for your implementations, measurements, and explanations.

## Learning cycle

1. Study the next course concept and capture one understood point, one unclear
   point, and one infrastructure connection.
2. Turn one of those points into a precise experimental question.
3. Record the hypothesis, prediction, and what would change your mind before
   implementation.
4. Build the smallest correct baseline and make one controlled change.
5. Measure, preserve surprising results, and run only the smallest
   discriminating check needed to interpret them.
6. Write the final takeaway in your own words, add the Production / AI
   Infrastructure Connection, and record one follow-up question.
7. Commit when the code, evidence, and explanation tell a coherent story.

## Suggested conventions

- Name experiments `NN-short-topic`, e.g. `03-vectorization-baseline`.
- Keep source code in the same experiment folder as its notes.
- Save generated output under `results/<experiment-name>/`.
- Record hardware, software, input size, repetitions, warm-up policy, and timing method alongside every benchmark.
- Commit after an experiment when the notes and evidence tell a coherent story.
