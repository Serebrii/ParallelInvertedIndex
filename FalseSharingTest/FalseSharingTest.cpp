#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <iomanip>

constexpr uint64_t ITERATIONS = 10'000'000;

// Реалізація А
void RunTest_ImplementationA(int P) {
    std::vector<long long> counters(P, 0);
    std::vector<std::thread> threads;

    for (int t = 0; t < P; ++t) {
        threads.emplace_back([&counters, t]() {
            for (uint64_t i = 0; i < ITERATIONS; ++i) {
                counters[t]++;
            }
            });
    }

    for (auto& th : threads) {
        th.join();
    }
}

// Реалізація В
struct alignas(64) PaddedCounter {
    long long value = 0;
};

void RunTest_ImplementationB(int P) {
    std::vector<PaddedCounter> counters(P);
    std::vector<std::thread> threads;

    for (int t = 0; t < P; ++t) {
        threads.emplace_back([&counters, t]() {
            for (uint64_t i = 0; i < ITERATIONS; ++i) {
                counters[t].value++;
            }
            });
    }

    for (auto& th : threads) {
        th.join();
    }
}

double MeasureTime(void(*test_func)(int), int P, int runs = 5) {
    std::vector<double> timings;
    for (int r = 0; r < runs; ++r) {
        auto start = std::chrono::high_resolution_clock::now();
        test_func(P);
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        timings.push_back(elapsed.count());
    }
    std::sort(timings.begin(), timings.end());
    return timings[timings.size() / 2];
}

int main() {
    std::cout << "=== Дослідження хибного розділення (False Sharing) ===\n";
    std::cout << "Кількість операцій на потік: " << ITERATIONS << "\n\n";

    std::vector<int> thread_counts = { 1, 2, 4, 8 };

    std::cout << std::setw(10) << "Потоки (P)"
        << std::setw(25) << "Реалізація A (без паддингу)"
        << std::setw(25) << "Реалізація B (з паддингом)"
        << std::setw(15) << "Прискорення" << "\n";
    std::cout << std::string(75, '-') << "\n";

    for (int P : thread_counts) {
        double time_A = MeasureTime(RunTest_ImplementationA, P);
        double time_B = MeasureTime(RunTest_ImplementationB, P);
        double speedup = time_A / time_B;

        std::cout << std::setw(10) << P
            << std::setw(20) << std::fixed << std::setprecision(2) << time_A << " ms"
            << std::setw(20) << std::fixed << std::setprecision(2) << time_B << " ms"
            << std::setw(14) << std::fixed << std::setprecision(2) << speedup << "x\n";
    }

    return 0;
}