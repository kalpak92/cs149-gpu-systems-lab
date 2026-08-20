# Course roadmap and capstone

This roadmap is intentionally topic-based rather than tied to a particular CS149 offering. Align the order with the lectures and assignments you are following; do not rush ahead merely to populate the repository.

| Phase | Core question | Evidence to produce | AI infrastructure connection |
| --- | --- | --- | --- |
| 00 — Machine baseline | What hardware and software actually run my workload? | GPU/CPU/software inventory and a first PyTorch tensor operation | Node labels, device plugins, capacity vs allocatable resources |
| 01 — Parallel thinking | When can a problem be split safely, and what limits speedup? | Serial vs parallel CPU baseline and a speedup curve | Service concurrency, Amdahl’s law, bottleneck isolation |
| 02 — CPU parallelism | What are threads, synchronization, races, and locality? | A race-free threaded workload plus a deliberately bad variant | Container CPU limits, noisy neighbors, worker design |
| 03 — Vector/SIMD | Why does data layout influence throughput? | Scalar vs vectorized implementation and correctness check | Tensor-library kernels and batching efficiency |
| 04 — GPU execution | How do grids, blocks, threads, warps, and occupancy relate? | Tiny PyTorch/CUDA workload with a launch-configuration comparison | GPU sharing, scheduling granularity, utilization |
| 05 — GPU memory | Why do coalescing, reuse, and memory movement dominate? | Two access patterns or a tiled-vs-naive comparison | HBM pressure, input pipelines, kernel fusion |
| 06 — Coordination | What do reductions, scans, atomics, and barriers cost? | One primitive with a correctness oracle and a contention comparison | Gradient aggregation and collective communication |
| 07 — Performance method | How do I locate a real bottleneck instead of guessing? | Benchmark protocol plus one profiler trace and interpretation | Capacity planning, observability, SLO-oriented tuning |
| 08 — Multi-GPU/distributed | Where is data located, what does it cost to move, and where does scaling stop? | Small distributed PyTorch/NCCL exercise when resources permit; introduce Ray only when it naturally fits the workload | DDP, all-reduce, collective communication, EKS networking |
| 09 — Orchestration | How are GPU workloads scheduled, isolated, and observed? | A minimal container/Kubernetes or Ray deployment investigation | Device plugins, MIG/time sharing, quotas, autoscaling |
| 10 — Capstone | Can I tell one systems story end-to-end? | Reproducible mini workload, measurements, analysis, and lessons | A deployment-aware GPU workload design |

## Capstone: GPU workload anatomy

The capstone is a small workload—not a production platform. Suggested shape:

1. Choose a simple AI-adjacent kernel or pipeline (for example: batched image transform, embedding lookup + reduction, or a tiny model inference path).
2. Establish correctness and a baseline.
3. Measure compute, memory movement, and host/device overhead separately where possible.
4. Improve one bottleneck with a justified change.
5. Explain how the workload would behave when scheduled in a container and, conceptually, across multiple workers.
6. Publish a short decision record: what you would optimize next, what you would leave alone, and why.

The capstone succeeds if the reasoning is sound even if the speedup is modest.
