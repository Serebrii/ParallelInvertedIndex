#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
#include <mutex>
#include <algorithm>
#include "types.h"
#include "text_utils.h"

namespace fs = std::filesystem;

std::vector<FileData> LoadCorpus(const std::string& dir_path) {
    std::vector<FileData> corpus;
    for (const auto& entry : fs::directory_iterator(dir_path)) {
        if (entry.is_regular_file()) {
            std::ifstream file(entry.path(), std::ios::binary);
            if (file) {
                std::string content((std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>());
                corpus.push_back({ entry.path().filename().string(), std::move(content) });
            }
        }
    }
    return corpus;
}

// V0
InvertedIndex BuildIndex_V0(const std::vector<FileData>& corpus) {
    InvertedIndex index;
    for (const auto& file : corpus) {
        auto tokens = TextUtils::TokenizeAndNormalize(file.content);
        for (const auto& token : tokens) {
            auto& record = index[token];
            record.total_count++;
            record.doc_counts[file.filename]++;
        }
    }
    return index;
}

// V1
InvertedIndex BuildIndex_V1(const std::vector<FileData>& corpus, int num_threads) {
    InvertedIndex index;
    std::vector<std::thread> threads;

    auto worker = [&](int thread_id) {
        for (size_t i = thread_id; i < corpus.size(); i += num_threads) {
            const auto& file = corpus[i];
            auto tokens = TextUtils::TokenizeAndNormalize(file.content);
            for (const auto& token : tokens) {
                auto& record = index[token];
                record.total_count++;
                record.doc_counts[file.filename]++;
            }
        }
        };

    for (int t = 0; t < num_threads; ++t) threads.emplace_back(worker, t);
    for (auto& th : threads) th.join();
    return index;
}

// V2
InvertedIndex BuildIndex_V2(const std::vector<FileData>& corpus, int num_threads) {
    InvertedIndex index;
    std::mutex global_mtx;
    std::vector<std::thread> threads;

    auto worker = [&](int thread_id) {
        for (size_t i = thread_id; i < corpus.size(); i += num_threads) {
            const auto& file = corpus[i];
            auto tokens = TextUtils::TokenizeAndNormalize(file.content);
            for (const auto& token : tokens) {
                std::lock_guard<std::mutex> lock(global_mtx);
                auto& record = index[token];
                record.total_count++;
                record.doc_counts[file.filename]++;
            }
        }
        };

    for (int t = 0; t < num_threads; ++t) threads.emplace_back(worker, t);
    for (auto& th : threads) th.join();
    return index;
}

// V3
struct alignas(64) IndexSection {
    std::mutex mtx;
    InvertedIndex index;
};

InvertedIndex BuildIndex_V3(const std::vector<FileData>& corpus, int num_threads, int K) {
    std::vector<IndexSection> sections(K);
    std::hash<std::string> hasher;
    std::vector<std::thread> threads;

    auto worker = [&](int thread_id) {
        for (size_t i = thread_id; i < corpus.size(); i += num_threads) {
            const auto& file = corpus[i];
            auto tokens = TextUtils::TokenizeAndNormalize(file.content);

            std::unordered_map<std::string, uint32_t> local_file_counts;
            for (const auto& token : tokens) {
                local_file_counts[token]++;
            }

            for (const auto& [token, count] : local_file_counts) {
                size_t k = hasher(token) % K;
                std::lock_guard<std::mutex> lock(sections[k].mtx);
                auto& record = sections[k].index[token];
                record.total_count += count;
                record.doc_counts[file.filename] = count;
            }
        }
        };

    for (int t = 0; t < num_threads; ++t) threads.emplace_back(worker, t);
    for (auto& th : threads) th.join();

    InvertedIndex final_index;
    for (int k = 0; k < K; ++k) {
        for (auto& [word, record] : sections[k].index) {
            final_index[word] = std::move(record);
        }
    }
    return final_index;
}

void MergeIndices(InvertedIndex& dest, const InvertedIndex& src) {
    for (const auto& [word, record] : src) {
        auto& dest_record = dest[word];
        dest_record.total_count += record.total_count;
        for (const auto& [doc, count] : record.doc_counts) {
            dest_record.doc_counts[doc] += count;
        }
    }
}

// V4
InvertedIndex BuildIndex_V4(const std::vector<FileData>& corpus, int num_threads, const std::string& merge_strategy) {
    std::vector<InvertedIndex> local_indices(num_threads);
    std::vector<std::thread> threads;

    auto worker = [&](int thread_id) {
        for (size_t i = thread_id; i < corpus.size(); i += num_threads) {
            const auto& file = corpus[i];
            auto tokens = TextUtils::TokenizeAndNormalize(file.content);
            for (const auto& token : tokens) {
                auto& record = local_indices[thread_id][token];
                record.total_count++;
                record.doc_counts[file.filename]++;
            }
        }
        };

    for (int t = 0; t < num_threads; ++t) threads.emplace_back(worker, t);
    for (auto& th : threads) th.join();

    // 1. P-1 кроків
    if (merge_strategy == "seq") {
        InvertedIndex final_index = std::move(local_indices[0]);
        for (int t = 1; t < num_threads; ++t) {
            MergeIndices(final_index, local_indices[t]);
        }
        return final_index;
    }
    // 2. log2 P кроків
    else {
        int step = 1;
        while (step < num_threads) {
            std::vector<std::thread> merge_threads;
            for (int i = 0; i < num_threads; i += 2 * step) {
                if (i + step < num_threads) {
                    merge_threads.emplace_back([&, i, step]() {
                        MergeIndices(local_indices[i], local_indices[i + step]);
                        });
                }
            }
            for (auto& th : merge_threads) th.join();
            step *= 2;
        }
        return std::move(local_indices[0]);
    }
}

Config ParseArgs(int argc, char* argv[]) {
    Config cfg;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--version" && i + 1 < argc) cfg.version = argv[++i];
        else if (arg == "--input" && i + 1 < argc) cfg.input_dir = argv[++i];
        else if (arg == "--output" && i + 1 < argc) cfg.output_file = argv[++i];
        else if (arg == "--threads" && i + 1 < argc) cfg.threads = std::stoi(argv[++i]);
        else if (arg == "-K" && i + 1 < argc) cfg.K = std::stoi(argv[++i]);
        else if (arg == "--runs" && i + 1 < argc) cfg.runs = std::stoi(argv[++i]);
    }
    return cfg;
}

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    Config cfg = ParseArgs(argc, argv);

    if (cfg.input_dir.empty()) {
        std::cout << "Використання: IndexBuilder.exe --version <V0|V1|V2|V3|V4_seq|V4_tree> --input <dir> --output <file> [--threads P] [-K K]\n";
        return 1;
    }

    std::cout << "Завантаження корпусу з: " << cfg.input_dir << "...\n";
    auto corpus = LoadCorpus(cfg.input_dir);
    std::cout << "Завантажено файлів: " << corpus.size() << "\n";

    if (corpus.empty()) {
        std::cerr << "Помилка: Корпус порожній!\n";
        return 1;
    }

    if (cfg.version == "V0") {
        auto dummy = BuildIndex_V0(corpus);
    }

    std::cout << "Виконання версії " << cfg.version << " (Потоків: " << cfg.threads << ", K: " << cfg.K << ")...\n";

    std::vector<double> timings;
    InvertedIndex final_index;

    for (int r = 0; r < cfg.runs; ++r) {
        auto start = std::chrono::high_resolution_clock::now();

        if (cfg.version == "V0") {
            final_index = BuildIndex_V0(corpus);
        }
        else if (cfg.version == "V1") {
            final_index = BuildIndex_V1(corpus, cfg.threads);
        }
        else if (cfg.version == "V2") {
            final_index = BuildIndex_V2(corpus, cfg.threads);
        }
        else if (cfg.version == "V3") {
            final_index = BuildIndex_V3(corpus, cfg.threads, cfg.K);
        }
        else if (cfg.version == "V4_seq") {
            final_index = BuildIndex_V4(corpus, cfg.threads, "seq");
        }
        else if (cfg.version == "V4_tree") {
            final_index = BuildIndex_V4(corpus, cfg.threads, "tree");
        }
        else {
            std::cerr << "Невідома версія: " << cfg.version << "\n";
            return 1;
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        timings.push_back(elapsed.count());

        uint64_t total_words = TextUtils::ComputeTotalWordCount(final_index);
        std::cout << "  Запуск " << (r + 1) << ": " << elapsed.count() << " ms | Сумарно слів: " << total_words << "\n";
    }

    std::sort(timings.begin(), timings.end());
    double median_time = timings[timings.size() / 2];
    std::cout << "\n[РЕЗУЛЬТАТ] Медіанний час " << cfg.version << ": " << median_time << " ms\n";

    if (!cfg.output_file.empty()) {
        std::cout << "Збереження результату у " << cfg.output_file << "...\n";
        TextUtils::SaveIndexToFile(final_index, cfg.output_file);
    }

    return 0;
}
