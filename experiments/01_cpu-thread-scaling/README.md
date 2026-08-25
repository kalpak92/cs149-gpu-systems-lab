# 01 — CPU thread scaling

**Status:** complete

**Timebox:** 30–60 minutes

**Date:** 2026-08-23

## 1. Question

How does the execution time of one fixed, compute-heavy, element-independent
OpenMP loop scale from one to eight physical CPU cores on this machine?

This experiment asks **where** the speedup curve stops being proportional. A
timing curve alone cannot prove **why** it bends; scheduling overhead, CPU
frequency changes, cache effects, memory bandwidth, and virtualization noise
remain hypotheses until a later measurement distinguishes them.

## 2. Concept

OpenMP divides independent loop iterations among worker threads. Each worker
has its own instruction stream, and the operating system can schedule those
threads on separate physical cores:

```text
fixed input
    |
    +-- chunk 0 --> worker 0 --> physical core
    +-- chunk 1 --> worker 1 --> physical core
    +-- chunk 2 --> worker 2 --> physical core
    `-- chunk 3 --> worker 3 --> physical core
```

![One-thread baseline versus eight-thread OpenMP execution](../../docs/assets/cpu-thread-scaling.svg)

The baseline worker processes the entire loop itself. In the eight-thread run,
static scheduling assigns disjoint iteration chunks to eight workers, and each
worker follows its own instruction stream on a distinct physical-core place.
The implicit barrier at the end means the loop is complete only after every
worker finishes its assigned chunk.

If the work is evenly divisible and no other resource becomes limiting, more
cores can finish it sooner. Perfect scaling is not guaranteed: workers must be
started and coordinated, CPU frequency may change with the number of active
cores, and the cores share parts of the cache and memory system.

This experiment isolates **thread-level parallelism**. The compiler and its
vectorization settings remain fixed, so the experiment does not independently
measure instruction-level parallelism or SIMD.

## 3. Hypothesis and prediction

- **Hypothesis:** Independent, compute-heavy loop iterations will execute in
  parallel on separate physical cores, producing an initial speedup. Speedup
  will be sublinear because parallel overhead and shared-resource limits keep
  the additional cores from contributing perfectly proportional throughput.
- **Prediction before running:** Four threads will achieve roughly `3x`
  speedup. Eight threads will achieve roughly `4–6x` speedup, with efficiency
  declining as thread count rises.
- **What would change my mind?** Less than `2x` speedup at eight threads would
  contradict the expectation that this workload exposes useful physical-core
  parallelism. Nearly linear `7–8x` speedup would contradict the expectation
  of a moderate scaling loss on this machine.

## 4. Smallest useful experiment

- **Baseline:** Run the same compiled OpenMP loop with one worker thread.
- **One controlled variable:** Set the worker count to `1`, `2`, `4`, or `8`.
  The first value is the baseline; the other values sample one scaling curve.
- **Workload:** Each element follows the same fixed arithmetic chain and has no
  dependency on another element. Separate input and output arrays ensure every
  repetition solves the same problem.
- **Correctness oracle:** Compute the complete one-thread output first. Before
  timing, compare every multi-threaded output with that reference using an
  explicit floating-point tolerance.
- **Measurement method:** Use two conceptual phases: one untimed correctness
  and warm-up pass for each thread count, followed by seven timed repetitions.
  Report raw wall times, the median, speedup, and parallel efficiency. Time only
  the OpenMP loop—not allocation, initialization, or correctness scanning.
- **Thread placement:** Use `OMP_PLACES=cores`, `OMP_PROC_BIND=spread`, and
  `OMP_DYNAMIC=FALSE` so the `1–8` thread sweep uses distinct physical-core
  places rather than adding SMT siblings.

The fixed compiler flags are `-O3 -march=native -fopenmp`. The generated binary
is host-specific because `-march=native` selects instructions available on the
current CPU; the recorded compiler and CPU model are therefore part of the
evidence.

## 5. Implementation log

The implementation is [`thread_scaling.cpp`](thread_scaling.cpp). Its comments
explain the workload, correctness boundary, OpenMP scheduling choice, and
timing method. [`run_experiment.sh`](run_experiment.sh) compiles into `/tmp`,
keeps generated binaries out of the repository, records the fixed environment,
and runs the benchmark.

Reproduce the original measurement from the repository root:

```bash
bash experiments/01_cpu-thread-scaling/run_experiment.sh
```

The initial result crossed the pre-recorded `7x` threshold at eight threads.
The smallest discriminating check increased only the fixed element count so
each timed interval was approximately ten times longer:

```bash
ELEMENTS=80000000 REPETITIONS=7 \
  bash experiments/01_cpu-thread-scaling/run_experiment.sh
```

Raw commands and results are preserved in
[`results/01_cpu-thread-scaling/2026-08-23-run.md`](../../results/01_cpu-thread-scaling/2026-08-23-run.md).

No dependency installation is required. This host already provides GCC and
the GNU OpenMP runtime.

## 6. Result

Correctness passed before timing: the `1`, `2`, `4`, and `8` thread outputs all
matched the one-thread reference with zero mismatches and zero observed maximum
absolute difference.

Initial measurement with 8,000,000 elements:

| Threads | Median time (s) | Speedup | Efficiency |
| ---: | ---: | ---: | ---: |
| 1 | 0.097404 | 1.000 | 100.0% |
| 2 | 0.048885 | 1.993 | 99.6% |
| 4 | 0.024521 | 3.972 | 99.3% |
| 8 | 0.012413 | 7.847 | 98.1% |

The eight-thread result exceeded the `7x` threshold that was recorded under
"What would change my mind?" Because its timed interval was only about 12 ms,
the result was preserved and checked with ten times as many elements.

Longer-interval check with 80,000,000 elements:

| Threads | Median time (s) | Speedup | Efficiency |
| ---: | ---: | ---: | ---: |
| 1 | 0.973131 | 1.000 | 100.0% |
| 2 | 0.487712 | 1.995 | 99.8% |
| 4 | 0.243918 | 3.990 | 99.7% |
| 8 | 0.122185 | 7.964 | 99.6% |

The longer run reproduced the near-linear curve and remained outside the
predicted `4–6x` range.

## 7. Explanation

**Observed:** For this fixed workload, wall time fell almost in proportion to
the number of physical-core workers from one through eight. Increasing the
input by `10x` produced nearly the same speedup curve, so the original result
was not merely a sub-millisecond timer-resolution artifact. The prediction of
moderate scaling loss was not supported.

**Interpretation:** The loop was intentionally favorable to multi-core
execution: iterations were independent, work was uniform, scheduling was
static, there were no reductions or locks, arithmetic work was high relative
to each element's load and store, and the runtime placed one worker on each of
eight physical-core places. These properties are consistent with the observed
curve.

The experiment did not collect CPU-frequency, cache, memory-bandwidth, or
hardware-counter measurements. It therefore does not prove that those costs
were absent or identify the small remaining deviation from ideal scaling. It
also does not predict how an imbalanced, memory-bound, or synchronized workload
would scale.

## 8. What I learned

I learned that OpenMP makes it straightforward to divide independent loop
iterations among CPU worker threads, but it does not create parallelism or
guarantee speedup by itself. This workload scaled much closer to linearly than
I predicted because every iteration was independent, the work was evenly
balanced and compute-heavy, static scheduling added little overhead, and each
worker was placed on a separate physical core. I should not generalize the
observed `7.964x` speedup to arbitrary CPU or AI preprocessing workloads,
because memory-bound, imbalanced, or synchronization-heavy work may scale much
less efficiently.

## 9. Production / AI Infrastructure Connection

CPU workers often decode, tokenize, augment, or batch independent inputs before
submitting them to a GPU. If that host-side work scales poorly, an expensive
accelerator can wait for input even though it is healthy and available. A
thread-scaling curve helps distinguish "more CPU workers are available" from
"more useful preprocessing throughput is actually delivered."

## 10. One follow-up experiment

Hold the OpenMP worker count at one and compile the same vectorizer-friendly
loop with compiler vectorization enabled and disabled. Confirm the generated
instruction difference, then measure what SIMD contributes without multi-core
scaling in the same comparison.
