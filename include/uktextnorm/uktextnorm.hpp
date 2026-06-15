#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace uktextnorm {

enum class UncertaintyCategory {
    AmbiguousAbbreviation,
    BareNumber,
    Currency,
    Date,
    Identifier,
    ForeignWord,
    MixedScript,
    RomanNumeral,
    Unit,
    Web,
};

enum class UncertaintySeverity {
    Info,
    Warning,
    Error,
};

enum class RangeStyle {
    Compact,
    FromTo,
};

enum class PhoneStyle {
    Grouped,
    DigitByDigit,
};

enum class SymbolStyle {
    Expand,
    Preserve,
};

enum class DateStyle {
    Formal,
    Spoken,
};

enum class NormalizePreset {
    Default,
    TtsFriendly,
    Conservative,
    SearchIndexing,
};

struct UncertainSpan {
    std::size_t start = 0;
    std::size_t stop = 0;
    std::string text;
    std::string reason;
    UncertaintyCategory category = UncertaintyCategory::BareNumber;
    UncertaintySeverity severity = UncertaintySeverity::Warning;

    friend bool operator==(const UncertainSpan&, const UncertainSpan&) = default;
};

struct NormalizeOptions {
    bool expand_known_acronyms = true;
    bool spell_unknown_acronyms = true;
    bool normalize_english_words = true;
    bool transliterate_latin = true;
    RangeStyle range_style = RangeStyle::Compact;
    PhoneStyle phone_style = PhoneStyle::Grouped;
    SymbolStyle symbol_style = SymbolStyle::Expand;
    DateStyle date_style = DateStyle::Formal;
};

[[nodiscard]] NormalizeOptions options_for_preset(NormalizePreset preset);

[[nodiscard]] std::string number_to_words(unsigned long long n);
[[nodiscard]] std::string number_to_words_digit_by_digit(std::string_view digits);
[[nodiscard]] std::string number_to_ordinal_words(unsigned long long n, std::string_view form = "nom_m");
[[nodiscard]] std::string number_to_words_case(unsigned long long n, std::string_view grammatical_case);

[[nodiscard]] std::string normalize_abbreviations(std::string_view text);
[[nodiscard]] std::string expand_abbreviations(std::string_view text);
[[nodiscard]] std::string cyrilize(std::string_view text);
[[nodiscard]] std::string cyrrilize(std::string_view text);
[[nodiscard]] std::string normalize_ukrainian(std::string_view text);
[[nodiscard]] std::string normalize_ukrainian(std::string_view text, const NormalizeOptions& options);
[[nodiscard]] std::string normalize_ukrainian(std::string_view text, NormalizePreset preset);
[[nodiscard]] std::vector<UncertainSpan> flag_uncertain(std::string_view text);

} // namespace uktextnorm
