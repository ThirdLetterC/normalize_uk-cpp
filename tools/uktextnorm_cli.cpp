#include "uktextnorm/uktextnorm.hpp"

#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef RUACCENT_CPP_VERSION
#define RUACCENT_CPP_VERSION "unknown"
#endif

namespace {

void usage(std::ostream& out)
{
    out << "Usage: uktextnorm [options] <text>\n"
        << "Options:\n"
        << "  --file <path>          Read input text from a UTF-8 file.\n"
        << "  --stdin                Read input text from standard input.\n"
        << "  --line-by-line         Normalize each input line independently.\n"
        << "  --from-to-ranges       Expand ranges as 'from ... to ...'.\n"
        << "  --preset <name>        Use default, tts-friendly, conservative, or search-indexing.\n"
        << "  --phone-digits         Spell phone numbers digit by digit.\n"
        << "  --preserve-symbols     Preserve standalone math symbols.\n"
        << "  --spoken-dates         Use genitive spoken day forms for numeric dates.\n"
        << "  --no-known-acronyms    Preserve known Ukrainian acronyms.\n"
        << "  --no-acronym-spelling  Preserve unknown all-caps acronyms.\n"
        << "  --no-english           Preserve known English brand/product words.\n"
        << "  --no-transliterate     Preserve remaining Latin-script words.\n"
        << "  --uncertain            Print uncertainty spans as TSV.\n"
        << "  --uncertain-json       Print uncertainty spans as JSON.\n"
        << "  --min-severity <level> Only print uncertainty spans at info, warning, or error.\n"
        << "  --fail-on <severity>   Exit 2 if uncertainty reaches info, warning, or error.\n"
        << "  --summary              Print uncertainty counts instead of individual spans.\n"
        << "  --version              Show version information.\n"
        << "  -h, --help             Show this message.\n";
}

std::string read_all(std::istream& in)
{
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

std::string read_file(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("failed to open " + path);
    }
    return read_all(in);
}

std::string_view category_name(uktextnorm::UncertaintyCategory category)
{
    switch (category) {
    case uktextnorm::UncertaintyCategory::AmbiguousAbbreviation:
        return "AmbiguousAbbreviation";
    case uktextnorm::UncertaintyCategory::BareNumber:
        return "BareNumber";
    case uktextnorm::UncertaintyCategory::Currency:
        return "Currency";
    case uktextnorm::UncertaintyCategory::Date:
        return "Date";
    case uktextnorm::UncertaintyCategory::Identifier:
        return "Identifier";
    case uktextnorm::UncertaintyCategory::ForeignWord:
        return "ForeignWord";
    case uktextnorm::UncertaintyCategory::MixedScript:
        return "MixedScript";
    case uktextnorm::UncertaintyCategory::RomanNumeral:
        return "RomanNumeral";
    case uktextnorm::UncertaintyCategory::Unit:
        return "Unit";
    case uktextnorm::UncertaintyCategory::Web:
        return "Web";
    }
    return "Unknown";
}

const std::vector<uktextnorm::UncertaintyCategory>& all_categories()
{
    static const std::vector<uktextnorm::UncertaintyCategory> categories = {
        uktextnorm::UncertaintyCategory::AmbiguousAbbreviation,
        uktextnorm::UncertaintyCategory::BareNumber,
        uktextnorm::UncertaintyCategory::Currency,
        uktextnorm::UncertaintyCategory::Date,
        uktextnorm::UncertaintyCategory::Identifier,
        uktextnorm::UncertaintyCategory::ForeignWord,
        uktextnorm::UncertaintyCategory::MixedScript,
        uktextnorm::UncertaintyCategory::RomanNumeral,
        uktextnorm::UncertaintyCategory::Unit,
        uktextnorm::UncertaintyCategory::Web};
    return categories;
}

std::string_view severity_name(uktextnorm::UncertaintySeverity severity)
{
    switch (severity) {
    case uktextnorm::UncertaintySeverity::Info:
        return "Info";
    case uktextnorm::UncertaintySeverity::Warning:
        return "Warning";
    case uktextnorm::UncertaintySeverity::Error:
        return "Error";
    }
    return "Unknown";
}

const std::vector<uktextnorm::UncertaintySeverity>& all_severities()
{
    static const std::vector<uktextnorm::UncertaintySeverity> severities = {uktextnorm::UncertaintySeverity::Info,
                                                                            uktextnorm::UncertaintySeverity::Warning,
                                                                            uktextnorm::UncertaintySeverity::Error};
    return severities;
}

int severity_rank(uktextnorm::UncertaintySeverity severity)
{
    switch (severity) {
    case uktextnorm::UncertaintySeverity::Info:
        return 0;
    case uktextnorm::UncertaintySeverity::Warning:
        return 1;
    case uktextnorm::UncertaintySeverity::Error:
        return 2;
    }
    return 3;
}

uktextnorm::UncertaintySeverity parse_severity(std::string_view value)
{
    if (value == "info" || value == "Info") {
        return uktextnorm::UncertaintySeverity::Info;
    }
    if (value == "warning" || value == "warn" || value == "Warning") {
        return uktextnorm::UncertaintySeverity::Warning;
    }
    if (value == "error" || value == "Error") {
        return uktextnorm::UncertaintySeverity::Error;
    }
    throw std::runtime_error("unknown severity: " + std::string(value));
}

bool reaches_severity(const std::vector<uktextnorm::UncertainSpan>& spans, uktextnorm::UncertaintySeverity threshold)
{
    for (const auto& span : spans) {
        if (severity_rank(span.severity) >= severity_rank(threshold)) {
            return true;
        }
    }
    return false;
}

std::vector<uktextnorm::UncertainSpan> filter_spans(const std::vector<uktextnorm::UncertainSpan>& spans,
                                                    std::optional<uktextnorm::UncertaintySeverity> min_severity)
{
    if (!min_severity) {
        return spans;
    }
    std::vector<uktextnorm::UncertainSpan> out;
    for (const auto& span : spans) {
        if (severity_rank(span.severity) >= severity_rank(*min_severity)) {
            out.push_back(span);
        }
    }
    return out;
}

std::vector<uktextnorm::UncertainSpan> collect_line_spans(std::string_view text,
                                                          std::optional<uktextnorm::UncertaintySeverity> min_severity,
                                                          std::optional<uktextnorm::UncertaintySeverity> fail_on,
                                                          bool& failed)
{
    std::vector<uktextnorm::UncertainSpan> out;
    std::size_t start = 0;
    while (start < text.size()) {
        const auto end = text.find('\n', start);
        auto line = text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        const auto spans = uktextnorm::flag_uncertain(line);
        if (fail_on && reaches_severity(spans, *fail_on)) {
            failed = true;
        }
        auto filtered = filter_spans(spans, min_severity);
        out.insert(out.end(), filtered.begin(), filtered.end());
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    return out;
}

std::string json_escape(std::string_view text)
{
    std::string out;
    for (const unsigned char ch : text) {
        switch (ch) {
        case '\\':
            out += "\\\\";
            break;
        case '"':
            out += "\\\"";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (ch < 0x20) {
                static constexpr char hex[] = "0123456789abcdef";
                out += "\\u00";
                out.push_back(hex[(ch >> 4) & 0x0F]);
                out.push_back(hex[ch & 0x0F]);
            } else {
                out.push_back(static_cast<char>(ch));
            }
        }
    }
    return out;
}

void print_span_json(const uktextnorm::UncertainSpan& span)
{
    std::cout << "{\"start\":" << span.start << ",\"stop\":" << span.stop << ",\"severity\":\""
              << severity_name(span.severity) << "\",\"category\":\"" << category_name(span.category)
              << "\",\"text\":\"" << json_escape(span.text) << "\",\"reason\":\"" << json_escape(span.reason) << "\"}";
}

void print_uncertain_json(const std::vector<uktextnorm::UncertainSpan>& spans)
{
    std::cout << "[\n";
    for (std::size_t i = 0; i < spans.size(); ++i) {
        std::cout << "  ";
        print_span_json(spans[i]);
        if (i + 1 < spans.size()) {
            std::cout << ',';
        }
        std::cout << '\n';
    }
    std::cout << "]\n";
}

std::size_t count_severity(const std::vector<uktextnorm::UncertainSpan>& spans,
                           uktextnorm::UncertaintySeverity severity)
{
    std::size_t count = 0;
    for (const auto& span : spans) {
        if (span.severity == severity) {
            ++count;
        }
    }
    return count;
}

std::size_t count_category(const std::vector<uktextnorm::UncertainSpan>& spans,
                           uktextnorm::UncertaintyCategory category)
{
    std::size_t count = 0;
    for (const auto& span : spans) {
        if (span.category == category) {
            ++count;
        }
    }
    return count;
}

void print_summary_tsv(const std::vector<uktextnorm::UncertainSpan>& spans)
{
    std::cout << "total\t" << spans.size() << '\n';
    for (const auto severity : all_severities()) {
        const auto count = count_severity(spans, severity);
        if (count) {
            std::cout << "severity\t" << severity_name(severity) << '\t' << count << '\n';
        }
    }
    for (const auto category : all_categories()) {
        const auto count = count_category(spans, category);
        if (count) {
            std::cout << "category\t" << category_name(category) << '\t' << count << '\n';
        }
    }
}

void print_summary_json(const std::vector<uktextnorm::UncertainSpan>& spans)
{
    std::cout << "{\"total\":" << spans.size() << ",\"severity\":{";
    bool first = true;
    for (const auto severity : all_severities()) {
        const auto count = count_severity(spans, severity);
        if (!count) {
            continue;
        }
        if (!first) {
            std::cout << ',';
        }
        first = false;
        std::cout << "\"" << severity_name(severity) << "\":" << count;
    }
    std::cout << "},\"category\":{";
    first = true;
    for (const auto category : all_categories()) {
        const auto count = count_category(spans, category);
        if (!count) {
            continue;
        }
        if (!first) {
            std::cout << ',';
        }
        first = false;
        std::cout << "\"" << category_name(category) << "\":" << count;
    }
    std::cout << "}}\n";
}

uktextnorm::NormalizePreset parse_preset(std::string_view value)
{
    if (value == "default") {
        return uktextnorm::NormalizePreset::Default;
    }
    if (value == "tts" || value == "tts-friendly") {
        return uktextnorm::NormalizePreset::TtsFriendly;
    }
    if (value == "conservative") {
        return uktextnorm::NormalizePreset::Conservative;
    }
    if (value == "search" || value == "search-indexing") {
        return uktextnorm::NormalizePreset::SearchIndexing;
    }
    throw std::runtime_error("unknown preset: " + std::string(value));
}

void print_line_by_line(std::string_view text, const uktextnorm::NormalizeOptions& options)
{
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto end = text.find('\n', start);
        auto line = text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        std::cout << uktextnorm::normalize_ukrainian(line, options) << '\n';
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
}

bool print_uncertain_json_line_by_line(std::string_view text,
                                       std::optional<uktextnorm::UncertaintySeverity> fail_on,
                                       std::optional<uktextnorm::UncertaintySeverity> min_severity)
{
    std::cout << "[\n";
    std::size_t start = 0;
    std::size_t line_no = 1;
    bool first_line = true;
    bool failed = false;
    while (start < text.size()) {
        const auto end = text.find('\n', start);
        auto line = text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        if (!first_line) {
            std::cout << ",\n";
        }
        first_line = false;
        const auto spans = uktextnorm::flag_uncertain(line);
        if (fail_on && reaches_severity(spans, *fail_on)) {
            failed = true;
        }
        const auto printed_spans = filter_spans(spans, min_severity);
        std::cout << "  {\"line\":" << line_no << ",\"text\":\"" << json_escape(line) << "\",\"spans\":[";
        for (std::size_t i = 0; i < printed_spans.size(); ++i) {
            if (i) {
                std::cout << ',';
            }
            print_span_json(printed_spans[i]);
        }
        std::cout << "]}";
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
        ++line_no;
    }
    std::cout << "\n]\n";
    return failed;
}

bool print_uncertain_tsv_line_by_line(std::string_view text,
                                      std::optional<uktextnorm::UncertaintySeverity> fail_on,
                                      std::optional<uktextnorm::UncertaintySeverity> min_severity)
{
    std::size_t start = 0;
    std::size_t line_no = 1;
    bool failed = false;
    while (start < text.size()) {
        const auto end = text.find('\n', start);
        auto line = text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        const auto spans = uktextnorm::flag_uncertain(line);
        if (fail_on && reaches_severity(spans, *fail_on)) {
            failed = true;
        }
        for (const auto& span : filter_spans(spans, min_severity)) {
            std::cout << line_no << '\t' << span.start << '\t' << span.stop << '\t' << severity_name(span.severity)
                      << '\t' << category_name(span.category) << '\t' << span.text << '\t' << span.reason << '\n';
        }
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
        ++line_no;
    }
    return failed;
}

} // namespace

int main(int argc, char** argv)
{
    uktextnorm::NormalizeOptions options;
    bool uncertain = false;
    bool uncertain_json = false;
    bool read_stdin = false;
    bool line_by_line = false;
    bool summary = false;
    std::optional<uktextnorm::UncertaintySeverity> fail_on;
    std::optional<uktextnorm::UncertaintySeverity> min_severity;
    std::string file_path;
    std::string text;

    try {
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            auto next = [&](std::string_view name) -> std::string {
                if (i + 1 >= argc) {
                    throw std::runtime_error("missing value for " + std::string(name));
                }
                return argv[++i];
            };
            if (arg == "-h" || arg == "--help") {
                usage(std::cout);
                return 0;
            }
            if (arg == "--version") {
                std::cout << "uktextnorm " << RUACCENT_CPP_VERSION << '\n';
                return 0;
            }
            if (arg == "--preset") {
                options = uktextnorm::options_for_preset(parse_preset(next(arg)));
            } else if (arg == "--file") {
                if (read_stdin || !file_path.empty() || !text.empty()) {
                    throw std::runtime_error("--file cannot be combined with --stdin or positional text");
                }
                file_path = next(arg);
            } else if (arg == "--stdin") {
                if (read_stdin || !file_path.empty() || !text.empty()) {
                    throw std::runtime_error("--stdin cannot be combined with --file or positional text");
                }
                read_stdin = true;
            } else if (arg == "--line-by-line") {
                line_by_line = true;
            } else if (arg == "--from-to-ranges") {
                options.range_style = uktextnorm::RangeStyle::FromTo;
            } else if (arg == "--phone-digits") {
                options.phone_style = uktextnorm::PhoneStyle::DigitByDigit;
            } else if (arg == "--preserve-symbols") {
                options.symbol_style = uktextnorm::SymbolStyle::Preserve;
            } else if (arg == "--spoken-dates") {
                options.date_style = uktextnorm::DateStyle::Spoken;
            } else if (arg == "--no-known-acronyms") {
                options.expand_known_acronyms = false;
            } else if (arg == "--no-acronym-spelling") {
                options.spell_unknown_acronyms = false;
            } else if (arg == "--no-english") {
                options.normalize_english_words = false;
            } else if (arg == "--no-transliterate") {
                options.transliterate_latin = false;
            } else if (arg == "--uncertain") {
                uncertain = true;
            } else if (arg == "--uncertain-json") {
                uncertain_json = true;
            } else if (arg == "--min-severity") {
                min_severity = parse_severity(next(arg));
            } else if (arg == "--fail-on") {
                fail_on = parse_severity(next(arg));
            } else if (arg == "--summary") {
                summary = true;
            } else if (arg.starts_with('-')) {
                throw std::runtime_error("unknown option: " + arg);
            } else {
                if (read_stdin || !file_path.empty()) {
                    throw std::runtime_error("positional text cannot be combined with --file or --stdin");
                }
                if (!text.empty()) {
                    text.push_back(' ');
                }
                text += arg;
            }
        }

        if (read_stdin) {
            text = read_all(std::cin);
        } else if (!file_path.empty()) {
            text = read_file(file_path);
        }

        if (text.empty()) {
            usage(std::cerr);
            return 1;
        }

        if (uncertain && uncertain_json) {
            throw std::runtime_error("--uncertain and --uncertain-json cannot be combined");
        }
        if (fail_on && !uncertain && !uncertain_json) {
            throw std::runtime_error("--fail-on requires --uncertain or --uncertain-json");
        }
        if (min_severity && !uncertain && !uncertain_json) {
            throw std::runtime_error("--min-severity requires --uncertain or --uncertain-json");
        }
        if (summary && !uncertain && !uncertain_json) {
            throw std::runtime_error("--summary requires --uncertain or --uncertain-json");
        }
        if (line_by_line) {
            if (summary) {
                bool failed = false;
                const auto spans = collect_line_spans(text, min_severity, fail_on, failed);
                if (uncertain_json) {
                    print_summary_json(spans);
                } else if (uncertain) {
                    print_summary_tsv(spans);
                }
                return failed ? 2 : 0;
            }
            if (uncertain_json) {
                return print_uncertain_json_line_by_line(text, fail_on, min_severity) ? 2 : 0;
            }
            if (uncertain) {
                return print_uncertain_tsv_line_by_line(text, fail_on, min_severity) ? 2 : 0;
            }
            print_line_by_line(text, options);
            return 0;
        }

        if (uncertain_json) {
            const auto spans = uktextnorm::flag_uncertain(text);
            const auto printed_spans = filter_spans(spans, min_severity);
            if (summary) {
                print_summary_json(printed_spans);
            } else {
                print_uncertain_json(printed_spans);
            }
            return fail_on && reaches_severity(spans, *fail_on) ? 2 : 0;
        }

        if (uncertain) {
            const auto spans = uktextnorm::flag_uncertain(text);
            const auto printed_spans = filter_spans(spans, min_severity);
            if (summary) {
                print_summary_tsv(printed_spans);
            } else {
                for (const auto& span : printed_spans) {
                    std::cout << span.start << '\t' << span.stop << '\t' << severity_name(span.severity) << '\t'
                              << category_name(span.category) << '\t' << span.text << '\t' << span.reason << '\n';
                }
            }
            return fail_on && reaches_severity(spans, *fail_on) ? 2 : 0;
        }

        std::cout << uktextnorm::normalize_ukrainian(text, options) << '\n';
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << "\n\n";
        usage(std::cerr);
        return 1;
    }
}
