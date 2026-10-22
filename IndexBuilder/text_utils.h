#pragma once
#include "types.h"
#include <windows.h>
#include <sstream>
#include <algorithm>
#include <fstream>

namespace TextUtils {
    inline std::wstring Utf8ToWide(const std::string& str) {
        if (str.empty()) return L"";
        int count = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
        std::wstring wstr(count, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], count);
        return wstr;
    }

    inline std::string WideToUtf8(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        int count = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
        std::string str(count, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &str[0], count, NULL, NULL);
        return str;
    }

    inline std::vector<std::string> TokenizeAndNormalize(const std::string& text) {
        std::wstring wtext = Utf8ToWide(text);
        std::vector<std::string> tokens;
        std::wstring current_token;

        for (wchar_t ch : wtext) {
            if (iswalnum(ch)) {
                current_token += towlower(ch);
            }
            else {
                if (current_token.length() >= 3) {
                    tokens.push_back(WideToUtf8(current_token));
                }
                current_token.clear();
            }
        }
        if (current_token.length() >= 3) {
            tokens.push_back(WideToUtf8(current_token));
        }

        return tokens;
    }

    inline void SaveIndexToFile(const InvertedIndex& index, const std::string& filepath) {
        std::vector<std::string> sorted_words;
        sorted_words.reserve(index.size());
        for (const auto& [word, _] : index) {
            sorted_words.push_back(word);
        }
        std::sort(sorted_words.begin(), sorted_words.end());

        std::ofstream out(filepath, std::ios::binary);
        for (const auto& word : sorted_words) {
            const auto& record = index.at(word);

            struct DocEntry {
                std::string name;
                uint32_t count;
            };
            std::vector<DocEntry> docs;
            docs.reserve(record.doc_counts.size());
            for (const auto& [doc, count] : record.doc_counts) {
                docs.push_back({ doc, count });
            }

            std::sort(docs.begin(), docs.end(), [](const DocEntry& a, const DocEntry& b) {
                if (a.count != b.count) return a.count > b.count;
                return a.name < b.name;
                });

            out << word << "\t" << record.total_count << "\t";
            for (size_t i = 0; i < docs.size(); ++i) {
                out << docs[i].name << ":" << docs[i].count;
                if (i + 1 < docs.size()) out << ",";
            }
            out << "\n";
        }
    }

    inline uint64_t ComputeTotalWordCount(const InvertedIndex& index) {
        uint64_t total = 0;
        for (const auto& [_, record] : index) {
            total += record.total_count;
        }
        return total;
    }

}
