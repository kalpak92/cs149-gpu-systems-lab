# Experiment protocol

Use this protocol for every lab. The outcome is an explanation backed by a small measurement, not a large codebase.

## Non-negotiable scope rule

**One experiment answers one question.** It may have one baseline and one controlled change. It has a 30–60 minute timebox. A desire to build a profiler, framework, reusable library, or a more advanced optimization is evidence of a *new* question: record it under “One follow-up experiment” and stop.

## Before coding

1. State one precise question.
2. Write a falsifiable hypothesis and a prediction. Include the direction and, when reasonable, an approximate magnitude.
3. Write **what would change your mind**: a result that would make the hypothesis untenable and why.
4. Identify the smallest useful workload and the one variable you will change.
5. Define correctness: an expected value, a reference implementation, or a tolerance.
6. State what you will measure and how: wall time, GPU time, bandwidth, CPU utilization, memory use, or profiler timeline.

## During implementation

1. Build the simplest correct baseline first.
2. Validate it with small inputs before timing it.
3. Change one thing at a time.
4. For GPU timing, warm up first and synchronize appropriately; asynchronous launches otherwise make timings misleading.
5. Record commands, versions, and relevant configuration in the experiment README.

## After measuring

1. Report the data, including enough repetitions to see variance.
2. Compare the result with the prediction.
3. If it is surprising, list plausible causes and run the smallest discriminating follow-up.
4. Explain the observation using the underlying execution or memory model.
5. Add the required Production / AI Infrastructure Connection. Make one honest, concrete connection; it need not cover every infrastructure concern.

## Definition of done

An experiment is done when its README includes a prediction, what would change the author’s mind, a result, a concrete Production / AI Infrastructure Connection, and one follow-up experiment; the code can be rerun; and the conclusion distinguishes observed facts from interpretation.
