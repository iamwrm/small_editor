#include <immintrin.h>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <thread>
#include <vector>

// 使用FMA (Fused Multiply-Add) 指令: a = a * b + c
// 每次FMA操作 = 2 FLOPS (1次乘法 + 1次加法)

constexpr size_t ITERATIONS = 100000000;  // 1亿次迭代
constexpr size_t UNROLL = 10;             // 循环展开次数

// AVX-512: 512位 = 16个float (单精度) 或 8个double (双精度)
// 每次FMA: 16 FLOPS (单精度) 或 8 FLOPS (双精度)

double benchmark_avx512_sp(size_t iterations) {
    // 单精度浮点 (float) - AVX-512
    __m512 v0 = _mm512_set1_ps(1.0f);
    __m512 v1 = _mm512_set1_ps(0.9999f);
    __m512 v2 = _mm512_set1_ps(0.9998f);
    __m512 v3 = _mm512_set1_ps(0.9997f);
    __m512 v4 = _mm512_set1_ps(0.9996f);
    __m512 v5 = _mm512_set1_ps(0.9995f);
    __m512 v6 = _mm512_set1_ps(0.9994f);
    __m512 v7 = _mm512_set1_ps(0.9993f);
    __m512 v8 = _mm512_set1_ps(0.9992f);
    __m512 v9 = _mm512_set1_ps(0.9991f);

    __m512 mul = _mm512_set1_ps(1.0000001f);
    __m512 add = _mm512_set1_ps(0.0000001f);

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < iterations; i++) {
        // 10路并行FMA，充分利用CPU流水线
        v0 = _mm512_fmadd_ps(v0, mul, add);
        v1 = _mm512_fmadd_ps(v1, mul, add);
        v2 = _mm512_fmadd_ps(v2, mul, add);
        v3 = _mm512_fmadd_ps(v3, mul, add);
        v4 = _mm512_fmadd_ps(v4, mul, add);
        v5 = _mm512_fmadd_ps(v5, mul, add);
        v6 = _mm512_fmadd_ps(v6, mul, add);
        v7 = _mm512_fmadd_ps(v7, mul, add);
        v8 = _mm512_fmadd_ps(v8, mul, add);
        v9 = _mm512_fmadd_ps(v9, mul, add);
    }

    auto end = std::chrono::high_resolution_clock::now();

    // 防止编译器优化掉计算
    volatile float sink = _mm512_reduce_add_ps(v0) + _mm512_reduce_add_ps(v1) +
                          _mm512_reduce_add_ps(v2) + _mm512_reduce_add_ps(v3) +
                          _mm512_reduce_add_ps(v4) + _mm512_reduce_add_ps(v5) +
                          _mm512_reduce_add_ps(v6) + _mm512_reduce_add_ps(v7) +
                          _mm512_reduce_add_ps(v8) + _mm512_reduce_add_ps(v9);
    (void)sink;

    std::chrono::duration<double> elapsed = end - start;

    // 计算FLOPS: iterations * UNROLL * 16(floats per vector) * 2(ops per FMA)
    double flops = (double)iterations * UNROLL * 16 * 2;
    return flops / elapsed.count();
}

double benchmark_avx512_dp(size_t iterations) {
    // 双精度浮点 (double) - AVX-512
    __m512d v0 = _mm512_set1_pd(1.0);
    __m512d v1 = _mm512_set1_pd(0.9999);
    __m512d v2 = _mm512_set1_pd(0.9998);
    __m512d v3 = _mm512_set1_pd(0.9997);
    __m512d v4 = _mm512_set1_pd(0.9996);
    __m512d v5 = _mm512_set1_pd(0.9995);
    __m512d v6 = _mm512_set1_pd(0.9994);
    __m512d v7 = _mm512_set1_pd(0.9993);
    __m512d v8 = _mm512_set1_pd(0.9992);
    __m512d v9 = _mm512_set1_pd(0.9991);

    __m512d mul = _mm512_set1_pd(1.0000001);
    __m512d add = _mm512_set1_pd(0.0000001);

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < iterations; i++) {
        v0 = _mm512_fmadd_pd(v0, mul, add);
        v1 = _mm512_fmadd_pd(v1, mul, add);
        v2 = _mm512_fmadd_pd(v2, mul, add);
        v3 = _mm512_fmadd_pd(v3, mul, add);
        v4 = _mm512_fmadd_pd(v4, mul, add);
        v5 = _mm512_fmadd_pd(v5, mul, add);
        v6 = _mm512_fmadd_pd(v6, mul, add);
        v7 = _mm512_fmadd_pd(v7, mul, add);
        v8 = _mm512_fmadd_pd(v8, mul, add);
        v9 = _mm512_fmadd_pd(v9, mul, add);
    }

    auto end = std::chrono::high_resolution_clock::now();

    volatile double sink = _mm512_reduce_add_pd(v0) + _mm512_reduce_add_pd(v1) +
                           _mm512_reduce_add_pd(v2) + _mm512_reduce_add_pd(v3) +
                           _mm512_reduce_add_pd(v4) + _mm512_reduce_add_pd(v5) +
                           _mm512_reduce_add_pd(v6) + _mm512_reduce_add_pd(v7) +
                           _mm512_reduce_add_pd(v8) + _mm512_reduce_add_pd(v9);
    (void)sink;

    std::chrono::duration<double> elapsed = end - start;

    // 计算FLOPS: iterations * UNROLL * 8(doubles per vector) * 2(ops per FMA)
    double flops = (double)iterations * UNROLL * 8 * 2;
    return flops / elapsed.count();
}

void run_multithread_benchmark(int num_threads) {
    std::vector<std::thread> threads;
    std::vector<double> results_sp(num_threads);
    std::vector<double> results_dp(num_threads);

    size_t iterations_per_thread = ITERATIONS / num_threads;

    // 单精度多线程测试
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([&, i]() {
            results_sp[i] = benchmark_avx512_sp(iterations_per_thread);
        });
    }
    for (auto& t : threads) t.join();
    auto end = std::chrono::high_resolution_clock::now();

    double total_sp = 0;
    for (double r : results_sp) total_sp += r;

    threads.clear();

    // 双精度多线程测试
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([&, i]() {
            results_dp[i] = benchmark_avx512_dp(iterations_per_thread);
        });
    }
    for (auto& t : threads) t.join();

    double total_dp = 0;
    for (double r : results_dp) total_dp += r;

    std::cout << "\n多线程测试 (" << num_threads << " 线程):\n";
    std::cout << "  单精度 (FP32): " << std::fixed << std::setprecision(2)
              << total_sp / 1e9 << " GFLOPS\n";
    std::cout << "  双精度 (FP64): " << std::fixed << std::setprecision(2)
              << total_dp / 1e9 << " GFLOPS\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "   CPU FLOPS 性能测试 (AVX-512 FMA)\n";
    std::cout << "========================================\n\n";

    std::cout << "测试参数:\n";
    std::cout << "  迭代次数: " << ITERATIONS << "\n";
    std::cout << "  循环展开: " << UNROLL << "x\n";
    std::cout << "  AVX-512: 16 x float 或 8 x double\n";
    std::cout << "  FMA: 2 FLOPS/操作 (乘+加)\n\n";

    // 单线程测试
    std::cout << "单线程测试:\n";

    double sp_flops = benchmark_avx512_sp(ITERATIONS);
    std::cout << "  单精度 (FP32): " << std::fixed << std::setprecision(2)
              << sp_flops / 1e9 << " GFLOPS\n";

    double dp_flops = benchmark_avx512_dp(ITERATIONS);
    std::cout << "  双精度 (FP64): " << std::fixed << std::setprecision(2)
              << dp_flops / 1e9 << " GFLOPS\n";

    // 多线程测试
    int hw_threads = std::thread::hardware_concurrency();
    std::cout << "\n硬件线程数: " << hw_threads << "\n";

    run_multithread_benchmark(4);
    run_multithread_benchmark(8);
    run_multithread_benchmark(16);

    std::cout << "\n========================================\n";

    return 0;
}
