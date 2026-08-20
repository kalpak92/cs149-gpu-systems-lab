# Lecture 1 — Why Parallelism? Why Efficiency?

**Status:** working notes

Lecture 1 is not mainly about learning a particular threading API. It introduces the systems question that will recur throughout CS149:

> When can work be done at the same time, and how much of the available hardware becomes useful when we try?

The answer depends on more than the number of processors. It depends on dependencies, work distribution, communication, synchronization, and the cost of moving data to the place where it will be used.

## Lecture map

```text
                         CS149
               WHY PARALLELISM?
               WHY EFFICIENCY?
                       |
       +---------------+----------------+
       |               |                |
   PERFORMANCE     PARALLELISM      EFFICIENCY
       |               |                |
  Useful work      Split work      Useful work
  per unit time    safely          per cost/energy
       |               |                |
       v               v                v
  Hardware        Execute       But parallelism
  capability      together      is not free
                       |                |
                       +--------+-------+
                                v
       Dependencies - Communication - Synchronization
                       - Data movement
                                |
                                v
                        Limited speedup
                                |
                                v
                    Parallel programming
                                |
              Decompose - Schedule - Coordinate
                                |
                                v
                    Hardware <-> software
                                |
                                v
                 CPUs - GPUs - AI accelerators
```

The rest of the lecture unpacks this map: why hardware trends create pressure for parallel software, why speedup is limited, and why data movement and coordination are central performance concerns.

## The central mental model

```text
                         COMPUTATION
                              │
                              ▼
                    Can the work be split safely?
                              │
                    ┌─────────┴─────────┐
                    │                   │
                   YES                  NO
                    │                   │
                    ▼                   ▼
          How should work be split?   Dependencies limit parallelism
                    │
                    ▼
          How do workers communicate?
                    │
                    ▼
             How do they synchronize?
                    │
                    ▼
             Where is the data located?
                    │
                    ▼
            How efficiently is hardware used?
```

Parallelism is therefore not the same thing as speedup. A workload may expose many independent tasks but still perform poorly if workers spend their time waiting, coordinating, or fetching data.

## Parallelism has limits

### Amdahl's Law

Even if the parallel portion scales perfectly, the sequential portion places an upper bound on total speedup:

$$
\text{speedup} \leq \frac{1}{S + \frac{1-S}{N}}
$$

where $S$ is the sequential fraction and $N$ is the number of workers.

A small amount of unavoidable sequential work can become the dominant bottleneck as the number of workers increases. This same limit appears later in distributed systems and AI infrastructure.

Some work is inherently sequential, and some work becomes sequential because later operations depend on earlier results. The parallel portion can scale only until the sequential portion, synchronization, and coordination overhead dominate.


The useful question is not:

> How many cores or GPUs do I have?

It is:

> How much independent useful work exists, and what prevents the workers from doing it efficiently?

It helps to distinguish several levels of parallelism:

```text
available parallelism
        ↓
parallelism exposed by the workload
        ↓
parallelism the programming model can express
        ↓
parallelism mapped onto hardware
        ↓
useful parallelism achieved
```

A workload can therefore contain substantial theoretical parallelism while achieving poor utilization in practice.

This is the beginning of the performance mindset for the rest of the course: establish a correct baseline, change one thing, measure, and explain the result using an execution model.

## Parallelism versus efficiency

Parallelism asks whether multiple operations can happen at once. Efficiency asks whether the resources assigned to the work are producing useful progress.

$$
\text{speedup} = \frac{\text{parallel performance}}{\text{serial performance}}
$$

$$
\text{efficiency} \approx \frac{\text{speedup}}{\text{number of workers}}
$$

When measuring execution time, the equivalent form is:

$$
\text{speedup} = \frac{\text{serial execution time}}{\text{parallel execution time}}
$$

For example, four workers producing a 3.8x speedup is much more useful than ten workers producing a 2x speedup. The second case uses more resources while leaving much of their potential idle or spent on overhead.

Common reasons efficiency falls include:

- a sequential dependency that every worker must wait for;
- uneven work distribution, leaving some workers idle;
- communication and synchronization between workers;
- contention for shared resources; and
- waiting for data from a slower level of the memory hierarchy.

A useful way to reason about where execution time goes is:

```text
                    TIME
                     │
          ┌──────────┼──────────┐
          │          │          │
      compute      memory    coordination
          │          │          │
          │          │          │
       useful      data       waiting
        work       movement   / overhead
```

Performance engineering is often the process of identifying which of these dominates, changing one thing, and measuring again.

## Memory is part of the computation

A processor can execute instructions quickly and still make little progress if the data it needs is far away. A useful simplified hierarchy is:

```text
registers → L1 cache → L2/L3 cache → DRAM → storage
 faster, smaller                         slower, larger
```

Caches reduce the effective latency of memory accesses by exploiting locality. They do not make DRAM itself faster; they make DRAM access less necessary when the needed data is already available in a nearer cache.

Caches exploit locality:

- **Temporal locality:** data used recently is likely to be used again soon.
- **Spatial locality:** data near recently used data is likely to be used soon.

For example, a sequential loop over an array often benefits from spatial locality because adjacent values arrive together in a cache line. The same computation with scattered accesses may do the same arithmetic but cause more expensive memory accesses.

Useful performance depends on computation, work distribution, data movement/locality, and coordination cost. This is a systems relationship, not an additive performance equation: improving one factor does not guarantee an equivalent increase in total performance.

### Latency is not throughput

These terms describe different things:

- **Latency:** how long one operation or result takes to become available.
- **Throughput:** how much work can be completed per unit of time.

A system can have high throughput while the latency of an individual operation remains large, especially when many independent operations are in flight. Conversely, a low-latency operation may have poor total throughput if it cannot be kept busy.

For this course, the practical question is often whether parallelism can hide latency. If many workers are all stalled on memory, adding more workers does not automatically create useful throughput. The memory system, access pattern, and amount of independent work matter.

## Preview: work and span

We will formalize this later, but two useful terms make the dependency idea precise:

```text
        A
      /   \
     B     C
      \   /
        D
```

- **Work:** the total amount of computation in the dependency graph.
- **Span:** the longest dependency chain through the graph.

The theoretical maximum parallelism is roughly:

$$
\text{parallelism} \approx \frac{\text{Work}}{\text{Span}}
$$

The graph may contain a lot of total work, but the span limits how much can proceed at the same time.

## Why this leads to GPUs and AI infrastructure

Modern CPUs, GPUs, and AI accelerators expose increasing amounts of parallel hardware. That shifts more responsibility to software: the program must expose independent work and organize data so the hardware can use it.

The same reasoning scales from one CPU loop to a GPU cluster:

```text
AI operation
    │
    ▼
split work across GPU threads / GPUs
    │
    ▼
move data to the right memory or device
    │
    ▼
communicate and synchronize when dependencies require it
    │
    ▼
keep expensive hardware doing useful work
```

At cluster scale, the costs become more visible: GPU memory, host memory, PCIe, GPU-to-GPU links, network interfaces, and the network all have different latency and throughput characteristics. A workload can have abundant mathematical parallelism and still scale poorly because communication or data movement dominates.

That is why Lecture 1 matters for AI infrastructure. GPU utilization, batching, memory layout, kernel efficiency, collective communication, and scheduling are all later expressions of the same basic problem:

> Put independent work near the data it needs, coordinate only when necessary, and measure how much of the hardware is doing useful work.

## What I should carry forward

1. More parallel hardware does not guarantee proportional speedup.
2. Efficiency is limited by dependencies, imbalance, coordination, contention, and data movement—not just arithmetic.
3. Cache locality is a performance concern because it reduces memory latency and helps computation units stay busy.
4. Latency and throughput are different measurements; parallel systems often use concurrency to hide latency and increase throughput.
5. The right unit of analysis is the whole path from work to data to synchronization, not the processor in isolation.

## Connection to this laboratory

The next concrete step is the machine baseline in [`experiments/00_machine-baseline/README.md`](../experiments/00_machine-baseline/README.md). Before measuring a speedup, record what hardware and software are actually available. Later experiments should make one part of this model visible at a time:

```text
baseline hardware
    → CPU parallelism
    → locality and vectorization
    → GPU execution and memory
    → synchronization and communication
    → multi-GPU and scheduling
```

The experiment protocol is intentionally narrow because a measurement is useful only when the question and controlled change are clear. See [`docs/experiment-protocol.md`](experiment-protocol.md).

## Source context

- [Stanford CS149 course material](https://gfxcourses.stanford.edu/cs149/fall25/)
- [Lecture 1 slides: Efficiency](https://gfxcourses.stanford.edu/cs149/fall25content/media/efficiency/01_efficiency_hyF1AJq.pdf)
- [Shared discussion and video-summary context](https://chatgpt.com/share/6a854f39-ee60-83e8-ad30-087caa293bd1)
