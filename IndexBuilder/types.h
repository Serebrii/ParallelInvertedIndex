#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

struct Config {
    std::string version = "V0";
    std::string input_dir;
    std::string output_file;
    int threads = 1;
    int K = 256;
    int merge_mode = 1;
    int runs = 5;
};

struct FileData {
    std::string filename;
    std::string content;
};

using DocFreqMap = std::unordered_map<std::string, uint32_t>;

struct TermRecord {
    uint32_t total_count = 0;
    DocFreqMap doc_counts;
};

using InvertedIndex = std::unordered_map<std::string, TermRecord>;
