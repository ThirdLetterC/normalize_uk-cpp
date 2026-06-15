#include "uktextnorm/uktextnorm.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct Case {
    std::string name;
    std::string text;
};

struct Preset {
    std::string_view name;
    uktextnorm::NormalizePreset value = uktextnorm::NormalizePreset::Default;
};

std::string read_file(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void add_golden_cases(std::vector<Case>& cases, const std::string& path, std::size_t max_cases)
{
    std::ifstream in(path);
    if (!in) {
        return;
    }
    std::string line;
    std::size_t row = 0;
    while (std::getline(in, line)) {
        ++row;
        if (line.empty() || line.front() == '#') {
            continue;
        }
        const auto tab = line.find('\t');
        const auto input = tab == std::string::npos ? line : line.substr(0, tab);
        cases.push_back({"golden:" + std::to_string(row), input});
        if (cases.size() >= max_cases) {
            return;
        }
    }
}

std::vector<Case> benchmark_cases(const std::string& root)
{
    std::vector<Case> cases;
    add_golden_cases(cases, root + "/tests/data/uktextnorm_golden.tsv", 16);
    add_golden_cases(cases, root + "/tests/data/uktextnorm_domain_golden.tsv", 24);

    if (auto cli_input = read_file(root + "/tests/data/uktextnorm_cli_input.txt"); !cli_input.empty()) {
        cases.push_back({"cli-input-prefix", cli_input.substr(0, 2048)});
    }

    const std::string legal = "Постанова КМУ № 123/2026-р від 15.06.2026 у справі № 910/1234/24. "
                              "Стягнути 1 234,56 грн, 12.5% річних, 5-7 кг матеріалів і 120/80 мм рт. ст. ";
    const std::string mixed = "OpenAI, ChatGPT, GitHub, Kubernetes, BTC/UAH, https://example.com/a?x=1&y=2, "
                              "+380 67 123-45-67, ФОП, ПДВ, XXI ст. ";

    std::string long_mixed;
    for (int i = 0; i < 2; ++i) {
        long_mixed += (i % 2 == 0) ? legal : mixed;
    }
    cases.push_back({"synthetic-long-mixed", std::move(long_mixed)});
    cases.push_back({"synthetic-legal", legal + legal});

    return cases;
}

std::size_t total_bytes(const std::vector<Case>& cases)
{
    return std::accumulate(cases.begin(), cases.end(), std::size_t{0}, [](std::size_t total, const Case& c) {
        return total + c.text.size();
    });
}

int iterations_for_bytes(std::size_t bytes)
{
    if (bytes == 0) {
        return 1;
    }
    const auto target_bytes = std::size_t{64} * 1024;
    const auto iterations = target_bytes / bytes;
    return static_cast<int>(std::clamp<std::size_t>(iterations, 1, 5));
}

void run_preset(const std::vector<Case>& cases, const Preset& preset)
{
    const auto bytes = total_bytes(cases);
    const auto iterations = iterations_for_bytes(bytes);
    std::size_t checksum = 0;

    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        for (const auto& c : cases) {
            checksum += uktextnorm::normalize_ukrainian(c.text, preset.value).size();
        }
    }
    const auto stop = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration<double>(stop - start);
    const auto total_docs = static_cast<double>(iterations * cases.size());
    const auto total_processed_bytes = static_cast<double>(iterations) * static_cast<double>(bytes);
    const auto ns_per_byte = elapsed.count() * 1'000'000'000.0 / total_processed_bytes;
    const auto docs_per_sec = total_docs / elapsed.count();

    std::cout << std::left << std::setw(15) << preset.name << " iterations=" << std::setw(4) << iterations
              << " bytes=" << std::setw(9) << static_cast<unsigned long long>(total_processed_bytes)
              << " elapsed_ms=" << std::setw(10) << std::fixed << std::setprecision(2) << elapsed.count() * 1000.0
              << " ns_per_byte=" << std::setw(10) << std::setprecision(2) << ns_per_byte
              << " docs_per_sec=" << std::setw(10) << std::setprecision(2) << docs_per_sec << " checksum=" << checksum
              << '\n';
}

} // namespace

int main(int argc, char** argv)
{
    const std::string root = argc > 1 ? argv[1] : ".";
    const auto cases = benchmark_cases(root);
    if (cases.empty()) {
        std::cerr << "No benchmark cases found under " << root << '\n';
        return 1;
    }

    std::cout << "cases=" << cases.size() << " corpus_bytes=" << total_bytes(cases) << '\n';
    for (const auto& preset : std::vector<Preset>{{"Default", uktextnorm::NormalizePreset::Default},
                                                  {"TtsFriendly", uktextnorm::NormalizePreset::TtsFriendly},
                                                  {"Conservative", uktextnorm::NormalizePreset::Conservative},
                                                  {"SearchIndexing", uktextnorm::NormalizePreset::SearchIndexing}}) {
        run_preset(cases, preset);
    }
}
