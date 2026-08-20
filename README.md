# CS149 GPU & AI Systems Lab

An evidence-first, hands-on companion to Stanford CS149. The aim is not to collect APIs or finish a large project quickly. It is to develop reliable intuition about parallel execution, GPU performance, and distributed AI infrastructure on a real NVIDIA T4 machine.

## The end state

By the end of the course, this repository should contain a small, reproducible body of evidence showing that I can:

- decompose work into parallel tasks and explain the limits to speedup;
- reason about CPU threads, SIMD, GPU threads/warps/blocks, memory hierarchy, data movement, and synchronization;
- write and measure small PyTorch and CUDA experiments;
- diagnose a performance result using measurements rather than intuition alone;
- connect a single-GPU observation to production AI concerns: placement, isolation, utilization, communication, and distributed training; and
- explain the trade-offs in my own words, with the code and evidence to support it.

Read [the roadmap](docs/course-roadmap.md) before beginning. Each lab follows [the experiment protocol](docs/experiment-protocol.md) and is recorded with [the experiment template](templates/experiment.md).

## Reproduce the GPU Python environment

The lab uses a project-local virtual environment. The environment directory is
disposable and ignored by Git; the committed requirements file is the
reproducible input.

```bash
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements-gpu-cu124.txt
.venv/bin/python scripts/collect_environment.py
```

`requirements-gpu-cu124.txt` pins the CUDA 12.4 PyTorch build validated on the
Tesla T4 during Experiment 00. The `cu124` label describes the CUDA runtime
bundled with the PyTorch wheel, not the maximum CUDA version reported by the
host driver. A different accelerator or driver environment may require a
different requirements file.

## Repository map

```text
docs/          Learning contract, roadmap, and environment notes
experiments/   One small, independent experiment per directory
templates/     Copyable templates for experiments and results
scripts/       Small reproducibility helpers (not a framework)
    results/       Raw measurements and selected evidence; large generated artifacts are ignored
```

## Working agreement

For every experiment, begin by writing a question and a prediction. **Hard rule: one experiment answers one question, has one controlled change, and stays within a 30–60 minute timebox.** Prefer a correct serial baseline, then one controlled change, then a measurement. If a new tool, abstraction, or optimization becomes tempting, record it as the next experiment instead of expanding the current one. A surprising result is a prompt to investigate—not a failure.

Do not read or paste full assignment solutions. This lab is for your implementations, measurements, and explanations.

## First session

1. Read `docs/course-roadmap.md`.
2. Use `templates/experiment.md` to expand the existing `experiments/00_machine-baseline/README.md`; retain its starting prompt.
3. Complete only the **Question**, **Hypothesis**, **Prediction**, and **What would change my mind?** sections.
4. Run `python3 scripts/collect_environment.py` and attach its output to the experiment notes.
5. Stop and interpret the inventory before doing any optimization.

## Suggested conventions

- Name experiments `NN-short-topic`, e.g. `03-vectorization-baseline`.
- Keep source code in the same experiment folder as its notes.
- Save generated output under `results/<experiment-name>/`.
- Record hardware, software, input size, repetitions, warm-up policy, and timing method alongside every benchmark.
- Commit after an experiment when the notes and evidence tell a coherent story.
