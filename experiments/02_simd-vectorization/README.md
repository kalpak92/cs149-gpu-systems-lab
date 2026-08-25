# 02 — SIMD vectorization

**Status:** complete

**Timebox:** 30–60 minutes

**Date:** 2026-08-23

## 1. Question

How much faster does the same compute-heavy, element-independent loop run on
one CPU thread when compiler SIMD vectorization is enabled rather than
disabled?

This experiment measures the performance contribution of SIMD. It does not
measure multi-core scaling, memory locality, or GPU execution.

## 2. Concept

A scalar floating-point instruction updates one element. A packed SIMD
instruction applies the same operation to several independent elements held in
one vector register. For the eight-wide example in Lecture 2:

```text
scalar execution                       eight-wide SIMD execution

instruction 0 -> element i             one packed instruction
instruction 1 -> element i+1            |-- lane 0 -> element i
instruction 2 -> element i+2            |-- lane 1 -> element i+1
...                                     |-- lane 2 -> element i+2
instruction 7 -> element i+7            |   ...
                                        `-- lane 7 -> element i+7

loop index advances by 1                loop index advances by 8
```

The vector instruction does not eliminate the algorithm's arithmetic stages.
In this workload, each element still passes through all 128 dependent stages;
SIMD may execute the same stage for several independent elements together.

![Scalar versus packed floating-point fused multiply-add](../../docs/assets/simd-scalar-vs-packed-fma.svg)

The `132` portion of both instruction names describes how the instruction's
three operands are arranged; it does not mean 132-bit arithmetic. The suffix
is the important distinction here: `ss` applies the operation to one scalar
single-precision value, while `ps` applies it independently across packed
single-precision lanes.

A 256-bit vector register is not inherently a floating-point register. An
instruction can interpret the same 256 stored bits as eight 32-bit floats,
eight 32-bit integers, sixteen 16-bit values, or another supported layout. The
element width determines the lane count; the instruction determines the
arithmetic rules. Floating-point operations support fractions, rounding,
infinity, and NaN, while integer operations use integer instructions and have
different overflow behavior. Eight `float32` values and eight `int32` values
therefore occupy the same number of lanes, but they do not execute the same
operation merely because their element widths match.

## 3. Hypothesis and prediction

- **Hypothesis:** The compiler can map independent loop iterations to SIMD
  lanes, increasing one thread's throughput without adding CPU cores.
- **Prediction before running:** The vectorized binary will be approximately
  `8x` faster than the non-vectorized binary because a 256-bit vector can cover
  eight 32-bit `float` elements per packed instruction.
- **What would change my mind?** Less than `4x` speedup would show that lane
  coverage does not translate proportionally into end-to-end loop speedup, so
  another limit matters. More than `8x` would show that vector width alone is
  insufficient to explain the result and that another compiler or hardware
  effect contributed.

The `8x` prediction is deliberately testable, not assumed. Register width sets
an upper-level intuition about work per packed instruction; dependencies,
instruction throughput, loop overhead, cleanup iterations, CPU frequency, and
other generated-code differences can change the measured ratio.

## 4. Smallest useful experiment

- **Baseline:** Compile the loop with `-O3 -march=native -fopenmp` and
  `-fno-tree-vectorize`.
- **One controlled change:** Compile the same source with the same flags but
  allow the `-O3` loop vectorizer to run.
- **Fixed execution:** Run both binaries with exactly one OpenMP worker placed
  on one physical-core location.
- **Workload:** Use the same 128-stage, compute-heavy, element-independent
  transformation as Experiment 01 over 8,000,000 `float` values.
- **Manipulation check:** Inspect GCC's vectorization report and disassembled
  machine instructions before interpreting timings. The expected distinction
  is scalar single-precision operations (`ss`) versus packed single-precision
  operations (`ps`).
- **Correctness oracle:** Before timing, reject any non-finite output and
  compare evenly spaced samples against an independently computed `long
  double` reference using explicit relative and absolute tolerances. Also
  compare the final checksums from the two binaries.
- **Measurement:** Warm up each binary, then record seven kernel-only wall-time
  samples and compare their medians.

The source is identical in the two builds. Allocation, initialization,
correctness checking, checksum calculation, and printing are outside the timed
region.

## 5. Implementation log

[`simd_vectorization.cpp`](simd_vectorization.cpp) contains the workload,
correctness oracle, and timer. Its comments explain the experiment for readers
who have not previously used OpenMP or SIMD.

[`run_experiment.sh`](run_experiment.sh) performs both builds, checks compiler
and assembly evidence, runs the scalar baseline followed by the vectorized
change, and calculates the median speedup. Generated binaries and diagnostics
remain in `/tmp`.

From the repository root:

```bash
bash experiments/02_simd-vectorization/run_experiment.sh
```

No dependency installation is expected: the repository's prior experiment
already established GCC and the GNU OpenMP runtime on this host.

## 6. Result

Correctness passed before timing in both binaries. Each result contained only
finite values, 4,095 evenly spaced samples matched the higher-precision
reference within the stated tolerance, and the scalar and vectorized checksums
were identical.

The compiler and machine-code checks confirmed the controlled change:

| Binary | Compiler report for measured loop | Representative instruction |
| --- | --- | --- |
| Scalar baseline | No vectorized-loop report | `vfmadd132ss` on an `xmm` register |
| Vectorized change | 32-byte vectors, plus a 16-byte fallback | `vfmadd132ps` on a `ymm` register |

`ss` means scalar single-precision floating point. `ps` means packed
single-precision floating point, and a 256-bit `ymm` register holds eight
32-bit `float` lanes.

Seven full-size timing samples produced:

| Binary | Median time (s) | Relative throughput |
| --- | ---: | ---: |
| Scalar baseline | 0.768161 | 1.000x |
| Vectorized change | 0.097474 | 7.881x |

The observed `7.881x` speedup was above the pre-recorded `4x` lower threshold,
below the `8x` upper threshold, and close to the predicted `8x`.

Raw environment, compiler, correctness, and timing evidence is preserved in
[`results/02_simd-vectorization/2026-08-23-run.md`](../../results/02_simd-vectorization/2026-08-23-run.md).

## 7. Explanation

**Observed:** The vectorized binary used 256-bit packed float operations in the
measured kernel and completed the same checked output in about one-eighth the
scalar time. Both binaries used one OpenMP worker, so adding CPU cores did not
produce this speedup.

**Interpretation:** The result supports the model that this deliberately
regular loop maps eight independent `float` elements to eight SIMD lanes. It
does not show that every loop gains `8x`: dependencies between iterations,
branches, non-contiguous memory, scalar remainder work, and other instruction
costs can prevent or dilute SIMD execution.

This experiment did not collect CPU-frequency or hardware-counter data, so it
does not attribute the remaining difference between `7.881x` and exactly `8x`
to a particular mechanism. Because the result stayed within the pre-recorded
thresholds, that small gap did not require expanding this experiment.

## 8. What I learned

Eight float lanes produced nearly eight times the throughput: the vectorized
loop was `7.881x` faster than the scalar loop. This made it concrete that one
CPU thread can use SIMD instructions to process eight independent `float`
elements at each arithmetic stage.

Experiment 02 used one OpenMP thread so that it isolated SIMD rather than
multi-core execution. Experiment 01 tested multi-core scaling using a binary
whose measured loop had a vectorized main path. Its OpenMP workers therefore
combined SIMD processing inside each worker with separate worker instruction
streams running across physical cores. This clarified that SIMD and multi-core
execution are separate dimensions of parallelism: SIMD processes multiple data
elements within one instruction stream, while multi-core execution runs
multiple worker instruction streams on separate cores.

I should not generalize the `7.881x` result to every loop. This workload was
deliberately easy to vectorize: iterations were independent, every element
followed the same arithmetic path, memory access was contiguous, and there
were no branches or synchronization. Dependencies, branches, irregular memory
access, or memory-bandwidth pressure could prevent all SIMD lanes from being
used effectively. Eight lanes provide the capacity to process eight values per
packed instruction, but they do not guarantee an `8x` speedup for arbitrary
code.

## 9. Production / AI Infrastructure Connection

CPU-side input pipelines often decode, normalize, tokenize, or transform many
independent values before submitting batches to a GPU. A vectorizable hot loop
can increase preprocessing throughput on each CPU worker; a loop that appears
parallel in source may still leave SIMD hardware unused. At cluster scale,
that difference can determine whether costly GPUs receive batches promptly or
wait on host preprocessing.

## 10. One follow-up experiment

Hold the algorithm and thread count fixed, then change the memory-access pattern
to compare contiguous access with a locality-hostile pattern. That is
Experiment 03; it stays outside this SIMD-only measurement.
