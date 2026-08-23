#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <omp.h>

namespace {

// The laboratory machine has eight physical cores and sixteen logical CPUs.
// Stopping this experiment at eight threads keeps physical-core scaling
// separate from the later question, "What do SMT siblings contribute?"
constexpr int kThreadCounts[] = {1, 2, 4, 8};

// Each stage performs the same multiply-and-add recurrence for one element.
// Repeating it raises arithmetic work per input byte, reducing the chance that
// a trivial array copy becomes the experiment's real subject. The value stays
// finite and close to its input over all stages.
constexpr int kStagesPerBlock = 8;
constexpr int kStageBlocks = 16;
constexpr int kComputeStages = kStagesPerBlock * kStageBlocks;
constexpr float kScale = 1.000001F;
constexpr float kBias = 0.000001F;

// Thread partitioning should not materially change an element's result, but
// floating-point code should state an explicit numerical criterion instead of
// silently assuming bitwise equality.
constexpr double kRelativeTolerance = 1.0e-6;
constexpr double kAbsoluteTolerance = 1.0e-6;

struct Options {
    std::size_t elements = 8'000'000;
    int repetitions = 7;
};

struct Comparison {
    std::size_t mismatches = 0;
    double max_absolute_difference = 0.0;
};

[[nodiscard]] Options parse_options(int argc, char** argv) {
    Options options;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];

        if (argument == "--elements" && index + 1 < argc) {
            options.elements = std::stoull(argv[++index]);
        } else if (argument == "--repetitions" && index + 1 < argc) {
            options.repetitions = std::stoi(argv[++index]);
        } else if (argument == "--help") {
            std::cout
                << "Usage: thread_scaling [--elements N] [--repetitions N]\n";
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

// Eight explicit stages form one readable block. Keeping the stages in source
// order preserves the dependency chain *within* an element. Different array
// elements remain independent, so a compiler may place several elements in
// SIMD lanes without changing the algorithm.
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

// Repeating the eight-stage helper sixteen times produces 128 arithmetic
// stages without hiding the loop to be vectorized behind a nested runtime
// loop. This explicit repetition is intentional laboratory code: it makes the
// outer element loop suitable for Experiment 02's vectorization on/off test.
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

void run_kernel(
    const std::vector<float>& input,
    std::vector<float>& output,
    int requested_threads
) {
    const auto element_count = static_cast<std::ptrdiff_t>(input.size());

    // Static scheduling assigns fixed contiguous chunks with minimal runtime
    // scheduling work. There is no race: iteration i reads input[i] and is the
    // only iteration that writes output[i]. The num_threads clause is the one
    // measured control varied across runs.
#pragma omp parallel for schedule(static) num_threads(requested_threads)
    for (std::ptrdiff_t index = 0; index < element_count; ++index) {
        output[static_cast<std::size_t>(index)] =
            transform_element(input[static_cast<std::size_t>(index)]);
    }
}

[[nodiscard]] int observe_team_size(int requested_threads) {
    int observed_threads = 0;

    // Verify outside the timed region that the runtime honored the requested
    // team size. The single region has an implicit barrier before return.
#pragma omp parallel num_threads(requested_threads)
    {
#pragma omp single
        observed_threads = omp_get_num_threads();
    }

    return observed_threads;
}

[[nodiscard]] Comparison compare_outputs(
    const std::vector<float>& reference,
    const std::vector<float>& candidate
) {
    Comparison comparison;

    for (std::size_t index = 0; index < reference.size(); ++index) {
        const double expected = static_cast<double>(reference[index]);
        const double actual = static_cast<double>(candidate[index]);
        const double difference = std::abs(actual - expected);
        const double limit =
            kAbsoluteTolerance + kRelativeTolerance * std::abs(expected);

        comparison.max_absolute_difference =
            std::max(comparison.max_absolute_difference, difference);
        if (!std::isfinite(actual) || difference > limit) {
            ++comparison.mismatches;
        }
    }

    return comparison;
}

[[nodiscard]] double checksum(const std::vector<float>& values) {
    // The checksum is deliberately outside the timed region. Printing it makes
    // the computed output observable and guards against dead-code elimination.
    double sum = 0.0;
    for (const float value : values) {
        sum += static_cast<double>(value);
    }
    return sum;
}

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

        // Dynamic teams would allow the OpenMP runtime to silently use fewer
        // workers than requested, so disable them before checking team sizes.
        omp_set_dynamic(0);

        std::vector<float> input(options.elements);
        std::vector<float> reference(options.elements);
        std::vector<float> output(options.elements);

        // Deterministic initialization makes every repetition solve the same
        // problem. Initialization is not part of the timed region.
        for (std::size_t index = 0; index < input.size(); ++index) {
            input[index] =
                0.001F + static_cast<float>(index % 1024) * 0.000001F;
        }

        std::cout << std::fixed << std::setprecision(9);
        std::cout << "METADATA,elements," << options.elements << '\n';
        std::cout << "METADATA,compute_stages," << kComputeStages << '\n';
        std::cout << "METADATA,repetitions," << options.repetitions << '\n';
        std::cout << "METADATA,omp_wtick_seconds," << omp_get_wtick() << '\n';

        // Correctness comes before performance: the one-thread result is the
        // reference, and every requested team size must match it before any
        // measured repetition begins. These runs also warm code and data.
        run_kernel(input, reference, 1);
        for (const int requested_threads : kThreadCounts) {
            const int observed_threads = observe_team_size(requested_threads);
            if (observed_threads != requested_threads) {
                std::cerr
                    << "OpenMP created " << observed_threads
                    << " threads after " << requested_threads
                    << " were requested\n";
                return EXIT_FAILURE;
            }

            run_kernel(input, output, requested_threads);
            const Comparison comparison = compare_outputs(reference, output);

            std::cout
                << "CHECK," << requested_threads << ','
                << comparison.mismatches << ','
                << comparison.max_absolute_difference << '\n';

            if (comparison.mismatches != 0) {
                std::cerr << "correctness check failed; timing was not started\n";
                return EXIT_FAILURE;
            }
        }

        std::vector<std::vector<double>> timings(std::size(kThreadCounts));

        // Alternate thread counts within each repetition so a slow drift in
        // host load or temperature is less likely to affect only one setting.
        // omp_get_wtime is a monotonic wall-clock timer provided by OpenMP.
        for (int repetition = 1; repetition <= options.repetitions; ++repetition) {
            for (std::size_t position = 0; position < std::size(kThreadCounts); ++position) {
                const int requested_threads = kThreadCounts[position];
                const double start_seconds = omp_get_wtime();
                run_kernel(input, output, requested_threads);
                const double elapsed_seconds = omp_get_wtime() - start_seconds;

                timings[position].push_back(elapsed_seconds);
                std::cout
                    << "RUN," << requested_threads << ','
                    << repetition << ',' << elapsed_seconds << '\n';
            }
        }

        const double baseline_seconds = median(timings.front());
        std::cout << "SUMMARY,threads,median_seconds,speedup,efficiency\n";

        for (std::size_t position = 0; position < std::size(kThreadCounts); ++position) {
            const int requested_threads = kThreadCounts[position];
            const double median_seconds = median(timings[position]);
            const double speedup = baseline_seconds / median_seconds;
            const double efficiency = speedup / requested_threads;

            std::cout
                << "SUMMARY," << requested_threads << ','
                << median_seconds << ',' << speedup << ',' << efficiency << '\n';
        }

        std::cout << "CHECKSUM," << checksum(output) << '\n';
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
