#include "uktextnorm/uktextnorm.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <optional>
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

struct Options {
    std::string root = ".";
    std::string preset_filter;
    std::string case_filter;
    std::size_t target_bytes = 256 * 1024;
    std::size_t per_case_target_bytes = 8 * 1024;
    bool per_case = true;
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

std::vector<Preset> presets()
{
    return {{"Default", uktextnorm::NormalizePreset::Default},
            {"TtsFriendly", uktextnorm::NormalizePreset::TtsFriendly},
            {"Conservative", uktextnorm::NormalizePreset::Conservative},
            {"SearchIndexing", uktextnorm::NormalizePreset::SearchIndexing}};
}

bool matches_filter(std::string_view value, std::string_view filter)
{
    return filter.empty() || value.find(filter) != std::string_view::npos;
}

int iterations_for_bytes(std::size_t bytes, std::size_t target_bytes)
{
    if (bytes == 0) {
        return 1;
    }
    const auto iterations = target_bytes / bytes;
    return static_cast<int>(std::clamp<std::size_t>(iterations, 1, 100000));
}

std::size_t run_iterations(const std::vector<Case>& cases, const Preset& preset, int iterations)
{
    std::size_t checksum = 0;
    for (int i = 0; i < iterations; ++i) {
        for (const auto& c : cases) {
            checksum += uktextnorm::normalize_ukrainian(c.text, preset.value).size();
        }
    }
    return checksum;
}

void print_result(std::string_view label,
                  int iterations,
                  double total_docs,
                  double total_processed_bytes,
                  std::chrono::duration<double> elapsed,
                  std::size_t checksum)
{
    const auto ns_per_byte = elapsed.count() * 1'000'000'000.0 / total_processed_bytes;
    const auto docs_per_sec = total_docs / elapsed.count();
    std::cout << std::left << std::setw(30) << label << " iterations=" << std::setw(7) << iterations
              << " bytes=" << std::setw(10) << static_cast<unsigned long long>(total_processed_bytes)
              << " elapsed_ms=" << std::setw(10) << std::fixed << std::setprecision(2) << elapsed.count() * 1000.0
              << " ns_per_byte=" << std::setw(10) << std::setprecision(2) << ns_per_byte
              << " docs_per_sec=" << std::setw(10) << std::setprecision(2) << docs_per_sec << " checksum=" << checksum
              << '\n';
}

void run_case(const Case& c, const Preset& preset, std::size_t target_bytes)
{
    const auto iterations = iterations_for_bytes(c.text.size(), target_bytes);
    const std::vector<Case> cases{c};
    const auto start = std::chrono::steady_clock::now();
    const auto checksum = run_iterations(cases, preset, iterations);
    const auto stop = std::chrono::steady_clock::now();
    print_result(std::string(preset.name) + "/" + c.name,
                 iterations,
                 static_cast<double>(iterations),
                 static_cast<double>(iterations) * static_cast<double>(c.text.size()),
                 stop - start,
                 checksum);
}

void run_preset(const std::vector<Case>& cases, const Preset& preset, const Options& options)
{
    const auto bytes = total_bytes(cases);
    const auto iterations = iterations_for_bytes(bytes, options.target_bytes);

    const auto start = std::chrono::steady_clock::now();
    const auto checksum = run_iterations(cases, preset, iterations);
    const auto stop = std::chrono::steady_clock::now();
    print_result(preset.name,
                 iterations,
                 static_cast<double>(iterations * cases.size()),
                 static_cast<double>(iterations) * static_cast<double>(bytes),
                 stop - start,
                 checksum);

    if (options.per_case) {
        for (const auto& c : cases) {
            run_case(c, preset, options.per_case_target_bytes);
        }
    }
}

void print_usage(std::string_view program)
{
    std::cerr
        << "usage: " << program
        << " [root] [--preset NAME] [--case NAME] [--target-bytes N] [--per-case-target-bytes N] [--no-per-case]\n";
}

std::optional<std::size_t> parse_size(std::string_view value)
{
    try {
        return static_cast<std::size_t>(std::stoull(std::string(value)));
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<Options> parse_options(int argc, char** argv)
{
    Options options;
    bool root_set = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        auto next = [&](std::string_view name) -> std::optional<std::string_view> {
            if (i + 1 >= argc) {
                std::cerr << "missing value for " << name << '\n';
                return std::nullopt;
            }
            return argv[++i];
        };
        if (arg == "--preset") {
            if (const auto value = next(arg)) {
                options.preset_filter = *value;
            } else {
                return std::nullopt;
            }
        } else if (arg == "--case") {
            if (const auto value = next(arg)) {
                options.case_filter = *value;
            } else {
                return std::nullopt;
            }
        } else if (arg == "--target-bytes") {
            const auto value = next(arg);
            const auto parsed = value ? parse_size(*value) : std::nullopt;
            if (!parsed) {
                std::cerr << "invalid --target-bytes value\n";
                return std::nullopt;
            }
            options.target_bytes = *parsed;
        } else if (arg == "--per-case-target-bytes") {
            const auto value = next(arg);
            const auto parsed = value ? parse_size(*value) : std::nullopt;
            if (!parsed) {
                std::cerr << "invalid --per-case-target-bytes value\n";
                return std::nullopt;
            }
            options.per_case_target_bytes = *parsed;
        } else if (arg == "--no-per-case") {
            options.per_case = false;
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return std::nullopt;
        } else if (!root_set) {
            options.root = std::string(arg);
            root_set = true;
        } else {
            std::cerr << "unknown argument: " << arg << '\n';
            return std::nullopt;
        }
    }
    return options;
}

} // namespace

int main(int argc, char** argv)
{
    const auto options = parse_options(argc, argv);
    if (!options) {
        return 1;
    }

    auto cases = benchmark_cases(options->root);
    std::erase_if(cases, [&](const Case& c) { return !matches_filter(c.name, options->case_filter); });
    if (cases.empty()) {
        std::cerr << "No benchmark cases found under " << options->root << '\n';
        return 1;
    }

    std::cout << "cases=" << cases.size() << " corpus_bytes=" << total_bytes(cases)
              << " target_bytes=" << options->target_bytes
              << " per_case_target_bytes=" << options->per_case_target_bytes << '\n';
    bool ran_preset = false;
    for (const auto& preset : presets()) {
        if (!matches_filter(preset.name, options->preset_filter)) {
            continue;
        }
        ran_preset = true;
        run_preset(cases, preset, *options);
    }
    if (!ran_preset) {
        std::cerr << "No preset matched filter: " << options->preset_filter << '\n';
        return 1;
    }
}
