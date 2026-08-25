#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <omp.h>
#include <unistd.h>

/*
 * Experiment 03: memory-locality benchmark.
 *
 * The program performs the same arithmetic once per matrix element using two
 * traversal orders:
 *   - row-major: visit adjacent float values within each stored row
 *   - column-major: visit the same column across successive stored rows
 *
 * Both paths use one OpenMP worker. Compiler SIMD vectorization and loop
 * interchange are disabled by the runner, so the controlled change is the
 * order of memory addresses—not the thread count or vector width.
 *
 * Only the traversal function is timed. Allocation, initialization,
 * correctness checks, checksums, and printing remain outside the timer.
 */

namespace {

constexpr std::size_t kExpectedCacheLineBytes = 64;
constexpr std::size_t kDefaultColumns =
    kExpectedCacheLineBytes / sizeof(float);
constexpr float kScale = 1.0001F;
constexpr float kBias = 0.0001F;
constexpr double kRelativeTolerance = 1.0e-6;
constexpr double kAbsoluteTolerance = 1.0e-6;

static_assert(
    kDefaultColumns * sizeof(float) == kExpectedCacheLineBytes,
    "one logical row must occupy exactly one expected cache line"
);

struct Options {
    std::size_t rows = 1'000'000;
    std::size_t columns = kDefaultColumns;
    int repetitions = 7;
};

struct FreeAlignedMemory {
    void operator()(float* pointer) const noexcept {
        // Pair with std::aligned_alloc so the buffer can be released with
        // std::free through std::unique_ptr.
        std::free(pointer);
    }
};

using AlignedFloatBuffer = std::unique_ptr<float[], FreeAlignedMemory>;

struct Comparison {
    std::size_t row_reference_mismatches = 0;
    std::size_t column_reference_mismatches = 0;
    std::size_t cross_mismatches = 0;
    double max_row_reference_difference = 0.0;
    double max_column_reference_difference = 0.0;
    double max_cross_difference = 0.0;
};

[[nodiscard]] Options parse_options(int argc, char** argv) {
    Options options;

    // Keep the defaults aligned with the experiment writeup unless the user
    // explicitly overrides them on the command line.
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];

        if (argument == "--rows" && index + 1 < argc) {
            options.rows = std::stoull(argv[++index]);
        } else if (argument == "--columns" && index + 1 < argc) {
            options.columns = std::stoull(argv[++index]);
        } else if (argument == "--repetitions" && index + 1 < argc) {
            options.repetitions = std::stoi(argv[++index]);
        } else if (argument == "--help") {
            std::cout
                << "Usage: memory_locality [--rows N] [--columns N] "
                << "[--repetitions N]\n";
            std::exit(EXIT_SUCCESS);
        } else {
            throw std::invalid_argument("unknown or incomplete argument: " + argument);
        }
    }

    if (options.rows == 0 || options.columns == 0 || options.repetitions <= 0) {
        throw std::invalid_argument("rows, columns, and repetitions must be positive");
    }
    if (options.columns * sizeof(float) != kExpectedCacheLineBytes) {
        throw std::invalid_argument(
            "this experiment requires exactly 16 float columns per 64-byte row"
        );
    }
    if (options.rows > static_cast<std::size_t>(PTRDIFF_MAX) ||
        options.columns > static_cast<std::size_t>(PTRDIFF_MAX)) {
        throw std::invalid_argument("matrix dimensions exceed OpenMP loop limits");
    }
    if (options.rows > SIZE_MAX / options.columns) {
        throw std::invalid_argument("matrix element count overflows size_t");
    }

    return options;
}

[[nodiscard]] AlignedFloatBuffer allocate_aligned(std::size_t elements) {
    const std::size_t bytes = elements * sizeof(float);

    // std::aligned_alloc requires the allocation size to be a multiple of the
    // requested alignment. Sixteen float columns make every complete row and
    // therefore the complete matrix size a multiple of 64 bytes.
    if (bytes % kExpectedCacheLineBytes != 0) {
        throw std::invalid_argument("aligned allocation size is not a multiple of 64");
    }

    void* raw_memory = std::aligned_alloc(kExpectedCacheLineBytes, bytes);
    if (raw_memory == nullptr) {
        throw std::bad_alloc();
    }

    return AlignedFloatBuffer(static_cast<float*>(raw_memory));
}

[[nodiscard, gnu::always_inline]] inline float transform(float value) {
    // The arithmetic is deliberately small so memory access remains visible.
    // Both traversal orders execute this exact operation once per element.
    return value * kScale + kBias;
}

[[gnu::noinline]] void run_row_major(
    const float* input,
    float* output,
    std::size_t rows,
    std::size_t columns
) {
    const auto row_count = static_cast<std::ptrdiff_t>(rows);

    // Row-major order consumes adjacent elements from one logical row before
    // moving to the next. With 16 float columns, the inner-loop byte stride is
    // 4 and one row occupies one 64-byte cache line.
#pragma omp parallel for schedule(static) num_threads(1)
    for (std::ptrdiff_t row = 0; row < row_count; ++row) {
        const std::size_t row_start = static_cast<std::size_t>(row) * columns;
        for (std::size_t column = 0; column < columns; ++column) {
            const std::size_t element = row_start + column;
            output[element] = transform(input[element]);
        }
    }
}

[[gnu::noinline]] void run_column_major(
    const float* input,
    float* output,
    std::size_t rows,
    std::size_t columns
) {
    const auto column_count = static_cast<std::ptrdiff_t>(columns);

    // Column-major order changes only the loop nesting. Successive inner-loop
    // elements are one complete row—or 64 bytes—apart in row-major storage.
    // The program still visits every element exactly once.
#pragma omp parallel for schedule(static) num_threads(1)
    for (std::ptrdiff_t column = 0; column < column_count; ++column) {
        for (std::size_t row = 0; row < rows; ++row) {
            const std::size_t element =
                row * columns + static_cast<std::size_t>(column);
            output[element] = transform(input[element]);
        }
    }
}

[[nodiscard]] int observe_team_size() {
    int observed_threads = 0;

    // If this check did not return one, the comparison would mix locality with
    // multi-core execution and would no longer answer the stated question.
#pragma omp parallel num_threads(1)
    {
#pragma omp single
        observed_threads = omp_get_num_threads();
    }

    return observed_threads;
}

[[nodiscard]] Comparison compare_outputs(
    const float* input,
    const float* row_output,
    const float* column_output,
    std::size_t elements
) {
    Comparison comparison;
    const long double scale = static_cast<long double>(kScale);
    const long double bias = static_cast<long double>(kBias);

    // Validate every element. The long-double calculation is an independent,
    // higher-precision numerical reference; the cross-check additionally
    // requires both traversal orders to agree with each other.
    for (std::size_t element = 0; element < elements; ++element) {
        const long double expected =
            static_cast<long double>(input[element]) * scale + bias;
        const double row_value = static_cast<double>(row_output[element]);
        const double column_value = static_cast<double>(column_output[element]);
        const double row_difference =
            std::abs(row_value - static_cast<double>(expected));
        const double column_difference =
            std::abs(column_value - static_cast<double>(expected));
        const double cross_difference = std::abs(row_value - column_value);
        const double reference_limit =
            kAbsoluteTolerance +
            kRelativeTolerance * std::abs(static_cast<double>(expected));
        const double cross_limit =
            kAbsoluteTolerance + kRelativeTolerance * std::abs(row_value);

        comparison.max_row_reference_difference = std::max(
            comparison.max_row_reference_difference,
            row_difference);
        comparison.max_column_reference_difference = std::max(
            comparison.max_column_reference_difference,
            column_difference);
        comparison.max_cross_difference = std::max(
            comparison.max_cross_difference,
            cross_difference);

        if (!std::isfinite(row_value) || row_difference > reference_limit) {
            ++comparison.row_reference_mismatches;
        }
        if (!std::isfinite(column_value) ||
            column_difference > reference_limit) {
            ++comparison.column_reference_mismatches;
        }
        if (!std::isfinite(row_value) || !std::isfinite(column_value) ||
            cross_difference > cross_limit) {
            ++comparison.cross_mismatches;
        }
    }

    return comparison;
}

[[nodiscard]] double checksum(const float* values, std::size_t elements) {
    // This serial reduction keeps the output observable but is not timed.
    double sum = 0.0;
    for (std::size_t element = 0; element < elements; ++element) {
        sum += static_cast<double>(values[element]);
    }
    return sum;
}

[[nodiscard]] double median(std::vector<double> samples) {
    // Sorting first makes the middle sample easy to identify for a stable
    // summary that is less sensitive to one-off timing noise.
    std::sort(samples.begin(), samples.end());
    const std::size_t middle = samples.size() / 2;

    // Use the usual middle element for odd counts and the average of the two
    // center elements for even counts.
    if (samples.size() % 2 == 1) {
        return samples[middle];
    }
    return (samples[middle - 1] + samples[middle]) / 2.0;
}

template <typename Traversal>
[[nodiscard]] double time_traversal(Traversal&& traversal) {
    // Measure only the traversal body; setup, validation, and reporting stay
    // outside the timed region.
    const double start = omp_get_wtime();
    traversal();
    return omp_get_wtime() - start;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        // main wires together the measurement experiment: parse inputs, check
        // the host assumptions, verify correctness, then time both traversals.
        const Options options = parse_options(argc, argv);
        const std::size_t elements = options.rows * options.columns;
        const std::size_t bytes_per_matrix = elements * sizeof(float);
        const long observed_cache_line = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);

        // The experiment is only meaningful if the host really exposes the
        // 64-byte cache-line assumption used by the matrix layout.
        if (observed_cache_line !=
            static_cast<long>(kExpectedCacheLineBytes)) {
            std::cerr
                << "expected a 64-byte L1 data-cache line, observed "
                << observed_cache_line << "\n";
            return EXIT_FAILURE;
        }

        omp_set_dynamic(0);
        if (observe_team_size() != 1) {
            std::cerr << "OpenMP did not create exactly one worker\n";
            return EXIT_FAILURE;
        }

        AlignedFloatBuffer input = allocate_aligned(elements);
        AlignedFloatBuffer row_output = allocate_aligned(elements);
        AlignedFloatBuffer column_output = allocate_aligned(elements);

        const auto input_alignment =
            reinterpret_cast<std::uintptr_t>(input.get()) %
            kExpectedCacheLineBytes;
        const auto row_output_alignment =
            reinterpret_cast<std::uintptr_t>(row_output.get()) %
            kExpectedCacheLineBytes;
        const auto column_output_alignment =
            reinterpret_cast<std::uintptr_t>(column_output.get()) %
            kExpectedCacheLineBytes;

        if (input_alignment != 0 || row_output_alignment != 0 ||
            column_output_alignment != 0) {
            std::cerr << "one or more matrix buffers are not 64-byte aligned\n";
            return EXIT_FAILURE;
        }

        // Deterministic values make both traversal orders solve the same
        // problem. Initialization is outside every timed region.
        for (std::size_t element = 0; element < elements; ++element) {
            input[element] =
                0.001F + static_cast<float>(element % 1024) * 0.000001F;
        }

        std::cout << std::fixed << std::setprecision(9);
        std::cout << "METADATA,rows," << options.rows << '\n';
        std::cout << "METADATA,columns," << options.columns << '\n';
        std::cout << "METADATA,elements," << elements << '\n';
        std::cout << "METADATA,bytes_per_matrix," << bytes_per_matrix << '\n';
        std::cout
            << "METADATA,observed_cache_line_bytes," << observed_cache_line
            << '\n';
        std::cout << "METADATA,row_bytes," << options.columns * sizeof(float)
                  << '\n';
        std::cout << "METADATA,row_inner_stride_bytes," << sizeof(float)
                  << '\n';
        std::cout
            << "METADATA,column_inner_stride_bytes,"
            << options.columns * sizeof(float) << '\n';
        std::cout << "METADATA,buffer_alignment_remainders,"
                  << input_alignment << ':' << row_output_alignment << ':'
                  << column_output_alignment << '\n';
        std::cout << "METADATA,repetitions," << options.repetitions << '\n';
        std::cout << "METADATA,openmp_threads,1\n";
        std::cout << "METADATA,omp_wtick_seconds," << omp_get_wtick() << '\n';

        // Correctness precedes timing. These two complete runs also warm the
        // code paths and ensure all matrix pages are resident.
        run_row_major(
            input.get(), row_output.get(), options.rows, options.columns);
        run_column_major(
            input.get(), column_output.get(), options.rows, options.columns);

        const Comparison comparison = compare_outputs(
            input.get(),
            row_output.get(),
            column_output.get(),
            elements);

        std::cout
            << "CHECK,row_reference," << comparison.row_reference_mismatches
            << ',' << comparison.max_row_reference_difference << '\n';
        std::cout
            << "CHECK,column_reference,"
            << comparison.column_reference_mismatches << ','
            << comparison.max_column_reference_difference << '\n';
        std::cout
            << "CHECK,row_vs_column," << comparison.cross_mismatches << ','
            << comparison.max_cross_difference << '\n';

        if (comparison.row_reference_mismatches != 0 ||
            comparison.column_reference_mismatches != 0 ||
            comparison.cross_mismatches != 0) {
            std::cerr << "correctness check failed; timing was not started\n";
            return EXIT_FAILURE;
        }

        std::vector<double> row_timings;
        std::vector<double> column_timings;
        row_timings.reserve(static_cast<std::size_t>(options.repetitions));
        column_timings.reserve(static_cast<std::size_t>(options.repetitions));

        // Alternate which traversal runs first so gradual host load or thermal
        // drift is less likely to affect only one pattern. Each traversal still
        // receives the same number of timed repetitions.
        for (int repetition = 1; repetition <= options.repetitions; ++repetition) {
            double row_elapsed = 0.0;
            double column_elapsed = 0.0;

            if (repetition % 2 == 1) {
                row_elapsed = time_traversal([&] {
                    run_row_major(
                        input.get(),
                        row_output.get(),
                        options.rows,
                        options.columns);
                });
                column_elapsed = time_traversal([&] {
                    run_column_major(
                        input.get(),
                        column_output.get(),
                        options.rows,
                        options.columns);
                });
            } else {
                column_elapsed = time_traversal([&] {
                    run_column_major(
                        input.get(),
                        column_output.get(),
                        options.rows,
                        options.columns);
                });
                row_elapsed = time_traversal([&] {
                    run_row_major(
                        input.get(),
                        row_output.get(),
                        options.rows,
                        options.columns);
                });
            }

            row_timings.push_back(row_elapsed);
            column_timings.push_back(column_elapsed);
            std::cout
                << "RUN,row-major," << repetition << ',' << row_elapsed << '\n';
            std::cout
                << "RUN,column-major," << repetition << ',' << column_elapsed
                << '\n';
        }

        const double row_median = median(row_timings);
        const double column_median = median(column_timings);

        std::cout << "SUMMARY,row-major," << row_median << '\n';
        std::cout << "SUMMARY,column-major," << column_median << '\n';
        std::cout
            << "COMPARISON,column_over_row_slowdown,"
            << column_median / row_median << '\n';
        std::cout
            << "CHECKSUM,row-major," << checksum(row_output.get(), elements)
            << '\n';
        std::cout
            << "CHECKSUM,column-major,"
            << checksum(column_output.get(), elements) << '\n';

        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
