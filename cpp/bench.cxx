#include <cmath>  // `std::sqrt`
#include <thread> // `std::thread`

#include <benchmark/benchmark.h>

#define SIMSIMD_RSQRT(x) (1 / sqrtf(x))
#define SIMSIMD_LOG(x) (logf(x))
#include <simsimd/simsimd.h>

namespace bm = benchmark;

enum class function_kind_t {
    distance_k,
    complex_dot_k,
    haversine_k,
};

template <typename scalar_at, std::size_t dimensions_ak> struct vectors_pair_gt {
    scalar_at a[dimensions_ak]{};
    scalar_at b[dimensions_ak]{};

    std::size_t dimensions() const noexcept { return dimensions_ak; }
    std::size_t size_bytes() const noexcept { return dimensions_ak * sizeof(scalar_at); }

    void set(scalar_at v) noexcept {
        for (std::size_t i = 0; i != dimensions_ak; ++i)
            a[i] = b[i] = v;
    }

    void randomize() noexcept {

        double a2_sum = 0, b2_sum = 0;
        for (std::size_t i = 0; i != dimensions_ak; ++i) {
            if constexpr (std::is_integral_v<scalar_at>)
                a[i] = static_cast<scalar_at>(rand()), b[i] = static_cast<scalar_at>(rand());
            else {
                double ai = double(rand()) / double(RAND_MAX), bi = double(rand()) / double(RAND_MAX);
                a2_sum += ai * ai, b2_sum += bi * bi;
                a[i] = static_cast<scalar_at>(ai), b[i] = static_cast<scalar_at>(bi);
            }
        }

        // Normalize the vectors:
        if constexpr (!std::is_integral_v<scalar_at>) {
            a2_sum = std::sqrt(a2_sum);
            b2_sum = std::sqrt(b2_sum);
            for (std::size_t i = 0; i != dimensions_ak; ++i)
                a[i] = static_cast<scalar_at>(a[i] / a2_sum), b[i] = static_cast<scalar_at>(b[i] / b2_sum);
        }
    }
};

template <typename pair_at, function_kind_t function_kind, typename metric_at = void>
constexpr void measure(bm::State& state, metric_at metric, metric_at baseline) {

    pair_at pair;
    pair.randomize();
    // pair.set(1);

    auto call_baseline = [&](pair_at& pair) -> simsimd_f32_t {
        // Output for real vectors have a single dimensions.
        // Output for complex vectors have two dimensions.
        simsimd_distance_t results[2] = {0, 0};
        baseline(pair.a, pair.b, pair.dimensions(), &results[0]);
        return results[0] + results[1];
    };
    auto call_contender = [&](pair_at& pair) -> simsimd_f32_t {
        // Output for real vectors have a single dimensions.
        // Output for complex vectors have two dimensions.
        simsimd_distance_t results[2] = {0, 0};
        metric(pair.a, pair.b, pair.dimensions(), &results[0]);
        return results[0] + results[1];
    };

    double c_baseline = call_baseline(pair);
    double c = 0;
    std::size_t iterations = 0;
    for (auto _ : state)
        bm::DoNotOptimize((c = call_contender(pair))), iterations++;

    state.counters["bytes"] = bm::Counter(iterations * pair.size_bytes() * 2, bm::Counter::kIsRate);
    state.counters["pairs"] = bm::Counter(iterations, bm::Counter::kIsRate);

    double delta = std::abs(c - c_baseline) > 0.0001 ? std::abs(c - c_baseline) : 0;
    double error = delta != 0 && c_baseline != 0 ? delta / c_baseline : 0;
    state.counters["abs_delta"] = delta;
    state.counters["relative_error"] = error;
}

template <typename scalar_at, function_kind_t function_kind = function_kind_t::distance_k, typename metric_at = void>
void register_(std::string name, metric_at* distance_func, metric_at* baseline_func) {

    std::size_t seconds = 10;
    std::size_t threads = std::thread::hardware_concurrency(); // 1;

    using pair_dims_t = vectors_pair_gt<scalar_at, 1536>;
    std::string name_dims = name + "_" + std::to_string(pair_dims_t{}.dimensions()) + "d";
    bm::RegisterBenchmark(name_dims.c_str(), measure<pair_dims_t, function_kind, metric_at*>, distance_func,
                          baseline_func)
        ->MinTime(seconds)
        ->Threads(threads);

    return;
    using pair_bytes_t = vectors_pair_gt<scalar_at, 1536 / sizeof(scalar_at)>;
    std::string name_bytes = name + "_" + std::to_string(pair_bytes_t{}.size_bytes()) + "b";
    bm::RegisterBenchmark(name_bytes.c_str(), measure<pair_bytes_t, function_kind, metric_at*>, distance_func,
                          baseline_func)
        ->MinTime(seconds)
        ->Threads(threads);
}

int main(int argc, char** argv) {

    // Log supported functionality
    char const* flags[2] = {"false", "true"};
    std::printf("Benchmarking Similarity Measures\n");
    std::printf("\n");
    std::printf("- Arm NEON support enabled: %s\n", flags[SIMSIMD_TARGET_NEON]);
    std::printf("- Arm SVE support enabled: %s\n", flags[SIMSIMD_TARGET_SVE]);
    std::printf("- x86 HASWELL support enabled: %s\n", flags[SIMSIMD_TARGET_HASWELL]);
    std::printf("- x86 SKYLAKE support enabled: %s\n", flags[SIMSIMD_TARGET_SKYLAKE]);
    std::printf("- x86 ICE support enabled: %s\n", flags[SIMSIMD_TARGET_ICE]);
    std::printf("- x86 SAPPHIRE support enabled: %s\n", flags[SIMSIMD_TARGET_SAPPHIRE]);
    std::printf("- Compiler supports F16: %s\n", flags[SIMSIMD_NATIVE_F16]);
    std::printf("\n");

    // Run the benchmarks
    bm::Initialize(&argc, argv);
    if (bm::ReportUnrecognizedArguments(argc, argv))
        return 1;

#if SIMSIMD_TARGET_NEON

    register_<simsimd_f16_t>("neon_f16_dot", simsimd_dot_f16_neon, simsimd_dot_f16_accurate);
    register_<simsimd_f16_t>("neon_f16_cos", simsimd_cos_f16_neon, simsimd_cos_f16_accurate);
    register_<simsimd_f16_t>("neon_f16_l2sq", simsimd_l2sq_f16_neon, simsimd_l2sq_f16_accurate);
    register_<simsimd_f16_t>("neon_f16_kl", simsimd_kl_f16_neon, simsimd_kl_f16_accurate);
    register_<simsimd_f16_t>("neon_f16_js", simsimd_js_f16_neon, simsimd_js_f16_accurate);

    register_<simsimd_f32_t>("neon_f32_dot", simsimd_dot_f32_neon, simsimd_dot_f32_accurate);
    register_<simsimd_f32_t>("neon_f32_cos", simsimd_cos_f32_neon, simsimd_cos_f32_accurate);
    register_<simsimd_f32_t>("neon_f32_l2sq", simsimd_l2sq_f32_neon, simsimd_l2sq_f32_accurate);
    register_<simsimd_f32_t>("neon_f32_kl", simsimd_kl_f32_neon, simsimd_kl_f32_accurate);
    register_<simsimd_f32_t>("neon_f32_js", simsimd_js_f32_neon, simsimd_js_f32_accurate);

    register_<simsimd_i8_t>("neon_i8_cos", simsimd_cos_i8_neon, simsimd_cos_i8_accurate);
    register_<simsimd_i8_t>("neon_i8_l2sq", simsimd_l2sq_i8_neon, simsimd_l2sq_i8_accurate);

    register_<simsimd_b8_t>("neon_b8_hamming", simsimd_hamming_b8_neon, simsimd_hamming_b8_serial);
    register_<simsimd_b8_t>("neon_b8_jaccard", simsimd_jaccard_b8_neon, simsimd_jaccard_b8_serial);

    register_<simsimd_f32_t, function_kind_t::complex_dot_k>("neon_f32c_dot", simsimd_dot_f32c_neon,
                                                             simsimd_dot_f32c_accurate);
#endif

#if SIMSIMD_TARGET_SVE
    register_<simsimd_f16_t>("sve_f16_dot", simsimd_dot_f16_sve, simsimd_dot_f16_accurate);
    register_<simsimd_f16_t>("sve_f16_cos", simsimd_cos_f16_sve, simsimd_cos_f16_accurate);
    register_<simsimd_f16_t>("sve_f16_l2sq", simsimd_l2sq_f16_sve, simsimd_l2sq_f16_accurate);

    register_<simsimd_f32_t>("sve_f32_dot", simsimd_dot_f32_sve, simsimd_dot_f32_accurate);
    register_<simsimd_f32_t>("sve_f32_cos", simsimd_cos_f32_sve, simsimd_cos_f32_accurate);
    register_<simsimd_f32_t>("sve_f32_l2sq", simsimd_l2sq_f32_sve, simsimd_l2sq_f32_accurate);

    register_<simsimd_f64_t>("sve_f64_dot", simsimd_dot_f64_sve, simsimd_dot_f64_serial);
    register_<simsimd_f64_t>("sve_f64_cos", simsimd_cos_f64_sve, simsimd_cos_f64_serial);
    register_<simsimd_f64_t>("sve_f64_l2sq", simsimd_l2sq_f64_sve, simsimd_l2sq_f64_serial);

    register_<simsimd_b8_t>("sve_b8_hamming", simsimd_hamming_b8_sve, simsimd_hamming_b8_serial);
    register_<simsimd_b8_t>("sve_b8_jaccard", simsimd_jaccard_b8_sve, simsimd_jaccard_b8_serial);
#endif

#if SIMSIMD_TARGET_HASWELL
    register_<simsimd_f16_t>("avx2_f16_dot", simsimd_dot_f16_haswell, simsimd_dot_f16_accurate);
    register_<simsimd_f16_t>("avx2_f16_cos", simsimd_cos_f16_haswell, simsimd_cos_f16_accurate);
    register_<simsimd_f16_t>("avx2_f16_l2sq", simsimd_l2sq_f16_haswell, simsimd_l2sq_f16_accurate);
    register_<simsimd_f16_t>("avx2_f16_kl", simsimd_kl_f16_haswell, simsimd_kl_f16_accurate);
    register_<simsimd_f16_t>("avx2_f16_js", simsimd_js_f16_haswell, simsimd_js_f16_accurate);

    register_<simsimd_i8_t>("avx2_i8_cos", simsimd_cos_i8_haswell, simsimd_cos_i8_accurate);
    register_<simsimd_i8_t>("avx2_i8_l2sq", simsimd_l2sq_i8_haswell, simsimd_l2sq_i8_accurate);

    register_<simsimd_b8_t>("avx2_b8_hamming", simsimd_hamming_b8_haswell, simsimd_hamming_b8_serial);
    register_<simsimd_b8_t>("avx2_b8_jaccard", simsimd_jaccard_b8_haswell, simsimd_jaccard_b8_serial);
#endif

#if SIMSIMD_TARGET_SAPPHIRE
    register_<simsimd_f16_t>("avx512_f16_dot", simsimd_dot_f16_sapphire, simsimd_dot_f16_accurate);
    register_<simsimd_f16_t>("avx512_f16_cos", simsimd_cos_f16_sapphire, simsimd_cos_f16_accurate);
    register_<simsimd_f16_t>("avx512_f16_l2sq", simsimd_l2sq_f16_sapphire, simsimd_l2sq_f16_accurate);
    register_<simsimd_f16_t>("avx512_f16_kl", simsimd_kl_f16_sapphire, simsimd_kl_f16_accurate);
    register_<simsimd_f16_t>("avx512_f16_js", simsimd_js_f16_sapphire, simsimd_js_f16_accurate);
#endif

#if SIMSIMD_TARGET_ICE
    register_<simsimd_i8_t>("avx512_i8_cos", simsimd_cos_i8_ice, simsimd_cos_i8_accurate);
    register_<simsimd_i8_t>("avx512_i8_l2sq", simsimd_l2sq_i8_ice, simsimd_l2sq_i8_accurate);

    register_<simsimd_f64_t>("avx512_f64_dot", simsimd_dot_f64_skylake, simsimd_dot_f64_serial);
    register_<simsimd_f64_t>("avx512_f64_cos", simsimd_cos_f64_skylake, simsimd_cos_f64_serial);
    register_<simsimd_f64_t>("avx512_f64_l2sq", simsimd_l2sq_f64_skylake, simsimd_l2sq_f64_serial);

    register_<simsimd_b8_t>("avx512_b8_hamming", simsimd_hamming_b8_avx512, simsimd_hamming_b8_serial);
    register_<simsimd_b8_t>("avx512_b8_jaccard", simsimd_jaccard_b8_avx512, simsimd_jaccard_b8_serial);
#endif

#if SIMSIMD_TARGET_SKYLAKE
    register_<simsimd_f32_t>("avx512_f32_dot", simsimd_dot_f32_skylake, simsimd_dot_f32_accurate);
    register_<simsimd_f32_t>("avx512_f32_cos", simsimd_cos_f32_skylake, simsimd_cos_f32_accurate);
    register_<simsimd_f32_t>("avx512_f32_l2sq", simsimd_l2sq_f32_skylake, simsimd_l2sq_f32_accurate);
    register_<simsimd_f32_t>("avx512_f32_kl", simsimd_kl_f32_skylake, simsimd_kl_f32_accurate);
    register_<simsimd_f32_t>("avx512_f32_js", simsimd_js_f32_skylake, simsimd_js_f32_accurate);
#endif

    register_<simsimd_f16_t>("serial_f16_dot", simsimd_dot_f16_serial, simsimd_dot_f16_accurate);
    register_<simsimd_f16_t>("serial_f16_cos", simsimd_cos_f16_serial, simsimd_cos_f16_accurate);
    register_<simsimd_f16_t>("serial_f16_l2sq", simsimd_l2sq_f16_serial, simsimd_l2sq_f16_accurate);
    register_<simsimd_f16_t>("serial_f16_kl", simsimd_kl_f16_serial, simsimd_kl_f16_accurate);
    register_<simsimd_f16_t>("serial_f16_js", simsimd_js_f16_serial, simsimd_js_f16_accurate);

    register_<simsimd_f32_t>("serial_f32_dot", simsimd_dot_f32_serial, simsimd_dot_f32_accurate);
    register_<simsimd_f32_t>("serial_f32_cos", simsimd_cos_f32_serial, simsimd_cos_f32_accurate);
    register_<simsimd_f32_t>("serial_f32_l2sq", simsimd_l2sq_f32_serial, simsimd_l2sq_f32_accurate);
    register_<simsimd_f32_t>("serial_f32_kl", simsimd_kl_f32_serial, simsimd_kl_f32_accurate);
    register_<simsimd_f32_t>("serial_f32_js", simsimd_js_f32_serial, simsimd_js_f32_accurate);

    register_<simsimd_f64_t>("serial_f64_dot", simsimd_dot_f64_serial, simsimd_dot_f64_serial);
    register_<simsimd_f64_t>("serial_f64_cos", simsimd_cos_f64_serial, simsimd_cos_f64_serial);
    register_<simsimd_f64_t>("serial_f64_l2sq", simsimd_l2sq_f64_serial, simsimd_l2sq_f64_serial);

    register_<simsimd_i8_t>("serial_i8_cos", simsimd_cos_i8_serial, simsimd_cos_i8_accurate);
    register_<simsimd_i8_t>("serial_i8_l2sq", simsimd_l2sq_i8_serial, simsimd_l2sq_i8_accurate);

    register_<simsimd_f32_t, function_kind_t::complex_dot_k>("serial_f32c_dot", simsimd_dot_f32c_serial,
                                                             simsimd_dot_f32c_accurate);
    register_<simsimd_f16_t, function_kind_t::complex_dot_k>("serial_f16c_dot", simsimd_dot_f16c_serial,
                                                             simsimd_dot_f16c_accurate);

    register_<simsimd_b8_t>("serial_b8_hamming", simsimd_hamming_b8_serial, simsimd_hamming_b8_serial);
    register_<simsimd_b8_t>("serial_b8_jaccard", simsimd_jaccard_b8_serial, simsimd_jaccard_b8_serial);

    bm::RunSpecifiedBenchmarks();
    bm::Shutdown();
    return 0;
}