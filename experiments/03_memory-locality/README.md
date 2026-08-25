# 03 — Memory locality

**Status:** complete

**Timebox:** 30–60 minutes

**Date:** 2026-08-25

## 1. Question

On one CPU thread, how much slower is column-major traversal than row-major
traversal when both process the same large row-major matrix and perform the
same arithmetic once per element?

This experiment measures the effect of access order. It does not independently
attribute the result to a particular cache level, TLB behavior, hardware
prefetcher, or memory-bandwidth counter.

## 2. Concept

This machine uses 64-byte cache lines, and each `float32` value occupies four
bytes:

```text
64 bytes per cache line / 4 bytes per float = 16 floats per cache line
```

The experiment stores a row-major matrix with exactly 16 columns and aligns its
base address to 64 bytes. One logical row therefore occupies one cache line.

- Row-major traversal consumes all 16 values before moving to the next line.
- Column-major traversal consumes one value from each row, scans the entire
working set, and returns later for the next value in those rows.

![Row-major versus column-major cache-line traversal](../../docs/assets/memory-locality-traversal.svg)

With one million rows, one column pass touches one million distinct cache
lines. The intervening working set is much larger than the processor caches,
so the simple model expects many lines to be fetched again during later column
passes. This is a hypothesis about data movement; the timing result alone
cannot identify which cache or translation mechanism supplied each access.

## 3. Hypothesis and prediction

- **Hypothesis:**
  - **Row-major** traversal will have better spatial locality because it uses
    every **`float`** in a fetched cache line before moving on.
  - **Column-major** traversal will move through the full working set between
    uses of neighboring values from the same logical row, increasing cache-line
    movement.
- **Prediction before running:** **Column-major** traversal will be approximately
  `8x` slower than **row-major** traversal.
- **Reasoning behind `8x`:**
  1. One 64-byte cache line holds sixteen 32-bit float values.
  2. **Row-major** traversal uses all sixteen values before moving to the next
     line.
  3. **Column-major** traversal uses one value, moves to another line, and may
     need to fetch the earlier line again during a later column pass.
  4. A simple model therefore suggests as much as `16x` more cache-line traffic.
     Wall time does not have to increase by the same amount because prefetching
     and overlapping memory operations can hide some of that cost. The timing
     prediction is therefore lower: approximately `8x`.
- **What would change my mind?** Less than `2x` slowdown would make this locality
  model incomplete: the caches or prefetchers may retain or supply the data
  more effectively than expected, or the workload may not be limited primarily
  by memory movement. More than `16x` slowdown would mean cache-line utilization
  alone is insufficient; TLB pressure, cache conflicts, store behavior, or
  another mechanism may also matter.

The prediction is a model to test, not a claim that cache-line traffic must map
proportionally to wall time.

## 4. Smallest useful experiment

- **Baseline:** Traverse a row-major `1,000,000 × 16` matrix row by row.
- **One controlled change:** Traverse the same matrix column by column.
- **Fixed execution:** Use one OpenMP worker, 64-byte-aligned input and output,
  the same scalar arithmetic, and the same compiler settings. Disable compiler
  SIMD vectorization and loop interchange for both traversals so those
  transformations do not become additional variables.
- **Manipulation check:** Confirm a 64-byte host cache line, 64-byte-aligned
  buffers, a four-byte row-major inner stride, and a 64-byte column-major inner
  stride before timing.
- **Correctness oracle:** Warm up both traversals, reject non-finite output, and
  compare every output element with an explicit floating-point tolerance before
  starting measured repetitions. Compare final checksums as a second guard.
- **Measurement:** Alternate traversal order across seven repetitions and
  report raw kernel-only wall times, medians, and the column/row slowdown ratio.
  Allocation, initialization, correctness scanning, and checksums remain
  outside the timed region.

Hardware counters are deliberately excluded. If the timing crosses a
pre-recorded threshold, the smallest discriminating counter or workload-size
check will be recorded rather than silently expanding the baseline experiment.

## 5. Implementation log

[`memory_locality.cpp`](memory_locality.cpp) contains the aligned matrix
allocation, two traversal orders, manipulation checks, correctness oracle, and
timing loop. Its comments explain how row-major indexing maps to byte strides
and which work lies outside the timed region.

[`run_experiment.sh`](run_experiment.sh) compiles with vectorization and loop
interchange disabled, confirms that GCC did not report a vectorized source
loop, and runs the benchmark with one OpenMP worker. Generated compiler reports
and binaries remain in `/tmp`.

From the repository root:

```bash
bash experiments/03_memory-locality/run_experiment.sh
```

The defaults implement the pre-recorded model: one million 64-byte logical
rows, seven timed repetitions, and exactly sixteen `float32` columns.

## 6. Result

All manipulation and correctness checks passed before timing:

- the observed L1 data-cache line size was 64 bytes;
- all three matrix buffers were aligned to 64-byte boundaries;
- the row-major inner stride was four bytes and the column-major inner stride
  was 64 bytes;
- GCC reported no vectorized source loop, and both paths used one OpenMP
  worker; and
- both outputs matched the higher-precision reference and each other with zero
  mismatches and identical checksums.

Seven full-size timing samples produced:

| Traversal | Median time (s) | Relative time |
| --- | ---: | ---: |
| Row-major baseline | 0.012747 | 1.000x |
| Column-major change | 0.163357 | 12.815x |

Column-major traversal was `12.815x` slower. The prediction correctly expected
a large slowdown but underestimated its magnitude: the observed ratio was
higher than `8x`, above the `2x` lower threshold, and below the `16x` upper
threshold.

Raw environment, correctness, and timing evidence is preserved in
[`results/03_memory-locality/2026-08-25-run.md`](../../results/03_memory-locality/2026-08-25-run.md).

## 7. Explanation

**Observed:** Both traversal orders processed the same 16,000,000 values with
the same scalar operation and produced identical checked output. Changing only
the loop nesting changed the inner byte stride from four to 64 and increased
median wall time by `12.815x`.

**Interpretation:** The result is consistent with the locality model. The
row-major path consumes all sixteen values in each logical cache-line-sized row
before moving on. The column-major path makes sixteen passes over a matrix that
is larger than the shared last-level cache, giving earlier lines a long reuse
distance and increasing data movement.

The timing does not prove a cache-miss count or attribute the slowdown among
cache capacity, hardware prefetching, TLB behavior, cache conflicts, and store
allocation or writeback. Those mechanisms remain hypotheses until measured
with a focused counter experiment. Because the result stayed inside the
pre-recorded thresholds, that attribution was not added to this experiment.

## 8. What I learned

I learned that cache capacity and cache-line size describe different things.
This CPU reports a 32 KiB L1 data cache per physical core, but it transfers data
in 64-byte cache lines. Because one 32-bit `float` occupies four bytes, each
line can hold sixteen float values.

The `12.815x` slowdown made it concrete that identical arithmetic and identical
output do not imply similar performance. Row-major traversal used all sixteen
values in each logical cache-line-sized row before moving on, while
column-major traversal moved to another line after each value and returned
later during another pass over the larger-than-cache matrix.

I should not generalize the exact `12.815x` ratio to every strided workload.
This experiment deliberately used one cache-line-sized row, a 64 MB matrix,
scalar code, one CPU thread, and a specific processor. Other matrix shapes,
working-set sizes, cache hierarchies, prefetchers, TLB behavior, store patterns,
and compiler transformations can produce a different slowdown.

## 9. Production / AI Infrastructure Connection

CPU input pipelines and tensor libraries frequently encounter contiguous
tensors, transposed views, image channels, embedding tables, and strided batch
layouts. Two representations can contain the same values and require the same
arithmetic while generating very different memory traffic. At cluster scale,
poor host-side locality can reduce batch-production throughput and leave
expensive GPUs waiting for input.

## 10. One follow-up experiment — deferred

How does increasing arithmetic work per loaded value change the performance
difference between memory-dominated and compute-dominated workloads?

This is a possible question to revisit after Lecture 3, not the next scheduled
experiment. The experiment should be defined only after the lecture reflection
establishes whether this is the most useful uncertainty to investigate.
