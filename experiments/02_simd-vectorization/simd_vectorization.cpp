#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <omp.h>

/*
 * Experiment 02: SIMD vectorization benchmark.
 *
 * This source is compiled twice:
 *   1. scalar baseline: the compiler's loop vectorizer is disabled
 *   2. vectorized change: the loop vectorizer is enabled
 *
 * Both binaries use one OpenMP thread and process the same input. This keeps
 * multi-core parallelism out of the comparison. The experiment asks what SIMD
 * contributes when the compiler can apply one instruction to several
 * independent array elements at once.
 *
 * Timing includes only the element-processing loop. Allocation, input setup,
 * correctness checks, checksums, and printing are deliberately outside it.
 */

namespace {

// Every element follows the same 128-stage arithmetic recurrence. The stages
// within one element depend on one another, but separate elements do not. That
// independence is what allows a compiler to place multiple elements in SIMD
// lanes.
constexpr int kStagesPerBlock = 8;
constexpr int kStageBlocks = 16;
constexpr int kComputeStages = kStagesPerBlock * kStageBlocks;
constexpr float kScale = 1.000001F;
constexpr float kBias = 0.000001F;

// A higher-precision reference will not round at exactly the same points as
// float machine instructions. Explicit tolerances distinguish harmless
// floating-point roundoff from a materially incorrect result.
constexpr long double kRelativeTolerance = 1.0e-5L;
constexpr long double kAbsoluteTolerance = 1.0e-6L;
constexpr std::size_t kMaximumReferenceSamples = 4096;

struct Options {
    std::string label = "unspecified";
    std::size_t elements = 8'000'000;
    int repetitions = 7;
};

struct Comparison {
    std::size_t samples = 0;
    std::size_t mismatches = 0;
    long double max_absolute_difference = 0.0L;
    long double max_relative_difference = 0.0L;
};

/*
 * Parses the experiment's CLI knobs: a label for the result stream, the number
 * of elements to process, and the repetition count used in the timing sample.
 * The same binary is compiled twice with different optimization settings, so the
 * command-line interface stays identical across both runs.
 */
[[nodiscard]] Options parse_options(int argc, char** argv) {
    Options options;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];

        if (argument == "--label" && index + 1 < argc) {
            options.label = argv[++index];
        } else if (argument == "--elements" && index + 1 < argc) {
            options.elements = std::stoull(argv[++index]);
        } else if (argument == "--repetitions" && index + 1 < argc) {
            options.repetitions = std::stoi(argv[++index]);
        } else if (argument == "--help") {
            std::cout
                << "Usage: simd_vectorization --label NAME "
                << "[--elements N] [--repetitions N]\n";
            std::exit(EXIT_SUCCESS);
        } else {
            throw std::invalid_argument("unknown or incomplete argument: " + argument);
        }
    }

    if (options.elements == 0 || options.repetitions <= 0) {
        throw std::invalid_argument("elements and repetitions must be positive");
    }

    return options;
}

// Writing eight stages explicitly gives the optimizer a visible straight-line
// dependency chain. One scalar instruction can update one element at a stage;
// one packed SIMD instruction can update several independent elements at that
// same stage. SIMD does not remove the 128 stages.
[[nodiscard, gnu::always_inline]] inline float apply_eight_stages(float value) {
    value = value * kScale + kBias;
    value = value * kScale + kBias;
    value = value * kScale + kBias;
    value = value * kScale + kBias;
    value = value * kScale + kBias;
    value = value * kScale + kBias;
    value = value * kScale + kBias;
    value = value * kScale + kBias;
    return value;
}

// Sixteen blocks times eight stages gives the same 128-stage workload used in
// Experiment 01. Repetition is intentional here: it leaves the outer array
// loop as the loop the compiler can vectorize.
[[nodiscard, gnu::always_inline]] inline float transform_element(float value) {
    value = apply_eight_stages(value);
    value = apply_eight_stages(value);
    value = apply_eight_stages(value);
    value = apply_eight_stages(value);
    value = apply_eight_stages(value);
    value = apply_eight_stages(value);
    value = apply_eight_stages(value);
    value = apply_eight_stages(value);
    value = apply_eight_stages(value);
    value = apply_eight_stages(value);
    value = apply_eight_stages(value);
    value = apply_eight_stages(value);
    value = apply_eight_stages(value);
    value = apply_eight_stages(value);
    value = apply_eight_stages(value);
    value = apply_eight_stages(value);
    return value;
}

/*
 * Uses a slower, higher-precision arithmetic path as an independent oracle.
 *
 * This loop is intentionally separate from the vectorized kernel. It runs only
 * on a sampled subset of elements after timing so it cannot distort the
 * measured SIMD throughput while still providing a trustworthy correctness
 * check.
 */
[[nodiscard]] long double transform_reference(long double value) {
    const long double scale = static_cast<long double>(kScale);
    const long double bias = static_cast<long double>(kBias);

    for (int stage = 0; stage < kComputeStages; ++stage) {
        value = value * scale + bias;
    }
    return value;
}

/*
 * Executes the benchmarked element-wise transform with a fixed single-thread
 * OpenMP team so the comparison isolates SIMD width from thread-level
 * parallelism.
 *
 * The workload is intentionally not split across cores; the compiler is free to
 * vectorize the innermost per-element arithmetic within one worker, which is
 * the behavior this experiment is measuring.
 */
void run_kernel(
    const std::vector<float>& input,
    std::vector<float>& output
) {
    const auto element_count = static_cast<std::ptrdiff_t>(input.size());

    // Exactly one OpenMP worker executes this loop in both binaries. OpenMP is
    // retained to keep the workload comparable with Experiment 01, but no work
    // is split across CPU cores here. The compiler may still use SIMD lanes
    // inside this one worker.
#pragma omp parallel for schedule(static) num_threads(1)
    for (std::ptrdiff_t index = 0; index < element_count; ++index) {
        output[static_cast<std::size_t>(index)] =
            transform_element(input[static_cast<std::size_t>(index)]);
    }
}

/*
 * Confirms that the runtime really did use a single OpenMP worker.
 *
 * This is a guardrail against a false comparison: if OpenMP silently creates
 * more than one thread, the benchmark would no longer isolate SIMD width from
 * thread-level parallelism and the result would be misleading.
 */
[[nodiscard]] int observe_team_size() {
    int observed_threads = 0;

    // Check the execution boundary before timing. If the runtime silently used
    // multiple workers, this would no longer be a SIMD-only comparison.
#pragma omp parallel num_threads(1)
    {
#pragma omp single
        observed_threads = omp_get_num_threads();
    }

    return observed_threads;
}

/*
 * Validates the transformed output against a higher-precision reference.
 *
 * The check is intentionally split into two passes: a full scan for NaN/Inf and
 * a sparse comparison over evenly spaced input indices. Sampling keeps the
 * oracle small while still covering the full output range and making it easy to
 * report the worst observed numerical error for the SIMD kernel.
 */
[[nodiscard]] Comparison check_correctness(
    const std::vector<float>& input,
    const std::vector<float>& output
) {
    Comparison comparison;
    // Round the division upward so even a non-multiple input size produces no
    // more than kMaximumReferenceSamples reference calculations.
    const std::size_t stride = std::max<std::size_t>(
        1,
        (input.size() + kMaximumReferenceSamples - 1) /
            kMaximumReferenceSamples);

    // First scan every output for NaN or infinity. Then compare evenly spaced
    // elements with the slower higher-precision reference. Sampling keeps the
    // oracle small while covering the full index range.
    for (const float value : output) {
        if (!std::isfinite(value)) {
            ++comparison.mismatches;
        }
    }

    for (std::size_t index = 0; index < input.size(); index += stride) {
        const long double expected =
            transform_reference(static_cast<long double>(input[index]));
        const long double actual = static_cast<long double>(output[index]);
        const long double absolute_difference = std::abs(actual - expected);
        const long double denominator = std::max(
            std::abs(expected), std::numeric_limits<long double>::min());
        const long double relative_difference = absolute_difference / denominator;
        const long double limit =
            kAbsoluteTolerance + kRelativeTolerance * std::abs(expected);

        ++comparison.samples;
        comparison.max_absolute_difference =
            std::max(comparison.max_absolute_difference, absolute_difference);
        comparison.max_relative_difference =
            std::max(comparison.max_relative_difference, relative_difference);

        if (!std::isfinite(actual) || absolute_difference > limit) {
            ++comparison.mismatches;
        }
    }

    return comparison;
}

/*
 * Returns a simple checksum of the transformed output so the final result is
 * still observable after the timed kernel completes. This serial reduction is
 * intentionally outside the benchmark window because the experiment measures the
 * element-wise transform cost, not the cost of reading the vector back.
 */
[[nodiscard]] double checksum(const std::vector<float>& values) {
    // Reading the output keeps the computation observable. It is outside the
    // timed region because the experiment measures transformation throughput,
    // not the cost of a serial reduction.
    double sum = 0.0;
    for (const float value : values) {
        sum += static_cast<double>(value);
    }
    return sum;
}

/*
 * Returns the median runtime across the repeated timing samples.
 *
 * A median is more stable than the mean for a small batch of runs because it is
 * less sensitive to one unusually slow or fast repetition while still
 * reflecting the central tendency of the observed kernel runtime.
 */
[[nodiscard]] double median(std::vector<double> samples) {
    std::sort(samples.begin(), samples.end());
    const std::size_t middle = samples.size() / 2;

    if (samples.size() % 2 == 1) {
        return samples[middle];
    }
    return (samples[middle - 1] + samples[middle]) / 2.0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parse_options(argc, argv);

        // Dynamic teams could violate the fixed-one-thread experiment design.
        omp_set_dynamic(0);
        if (observe_team_size() != 1) {
            std::cerr << "OpenMP did not create exactly one worker\n";
            return EXIT_FAILURE;
        }

        std::vector<float> input(options.elements);
        std::vector<float> output(options.elements);

        // Deterministic input means the scalar and vectorized binaries solve
        // the same problem. Initialization is not timed.
        for (std::size_t index = 0; index < input.size(); ++index) {
            input[index] =
                0.001F + static_cast<float>(index % 1024) * 0.000001F;
        }

        std::cout << std::fixed << std::setprecision(9);
        std::cout << "METADATA,label," << options.label << '\n';
        std::cout << "METADATA,elements," << options.elements << '\n';
        std::cout << "METADATA,compute_stages," << kComputeStages << '\n';
        std::cout << "METADATA,repetitions," << options.repetitions << '\n';
        std::cout << "METADATA,openmp_threads,1\n";
        std::cout << "METADATA,omp_wtick_seconds," << omp_get_wtick() << '\n';

        // Correctness must pass before performance measurement. This first run
        // also warms the instruction path and touches all input/output pages.
        run_kernel(input, output);
        const Comparison comparison = check_correctness(input, output);
        std::cout
            << "CHECK," << options.label << ','
            << comparison.samples << ','
            << comparison.mismatches << ','
            << static_cast<double>(comparison.max_absolute_difference) << ','
            << static_cast<double>(comparison.max_relative_difference) << '\n';

        if (comparison.mismatches != 0) {
            std::cerr << "correctness check failed; timing was not started\n";
            return EXIT_FAILURE;
        }

        std::vector<double> timings;
        timings.reserve(static_cast<std::size_t>(options.repetitions));

        for (int repetition = 1; repetition <= options.repetitions; ++repetition) {
            const double start = omp_get_wtime();
            run_kernel(input, output);
            const double elapsed = omp_get_wtime() - start;

            timings.push_back(elapsed);
            std::cout
                << "RUN," << options.label << ','
                << repetition << ',' << elapsed << '\n';
        }

        std::cout
            << "SUMMARY," << options.label << ',' << median(timings) << '\n';
        std::cout
            << "CHECKSUM," << options.label << ',' << checksum(output) << '\n';

        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
