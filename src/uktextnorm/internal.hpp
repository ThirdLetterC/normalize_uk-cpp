#pragma once

#include "uktextnorm/uktextnorm.hpp"

#include "../common/utf8.hpp"

#include <ctre.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace uktextnorm::detail {

using Forms = std::array<std::string_view, 3>;
using CaseForms = std::array<std::string_view, 4>;

struct Measurement {
    std::string_view one;
    std::string_view few;
    std::string_view many;
    char gender = 'm';
};

struct CountedNoun {
    std::string_view one;
    std::string_view few;
    std::string_view many;
    char gender = 'm';
};

struct Cp {
    char32_t value = 0;
    std::size_t start = 0;
    std::size_t stop = 0;
};

struct FinanceUnit {
    Forms forms;
    bool feminine = false;
};

using normalize_uk_cpp::detail::append_utf8;
using normalize_uk_cpp::detail::decode_one;

// Character/string helpers.
bool is_uk(char32_t cp);
bool is_upper_uk(char32_t cp);
bool is_latin(char32_t cp);
bool is_word_joiner(char32_t cp);
char32_t lower_cp(char32_t cp);
char32_t upper_cp(char32_t cp);
std::string lower_text(std::string_view text);
std::string capitalize_first_letter(std::string text);
std::vector<Cp> codepoints(std::string_view text);
std::vector<std::size_t> byte_to_char_offsets(std::string_view text);
bool has_ascii_digit(std::string_view text);
bool has_ascii_alpha(std::string_view text);
bool contains_any(std::string_view text, std::string_view chars);
bool contains_any_token(std::string_view text, std::initializer_list<std::string_view> tokens);
bool is_utf8_continuation(char ch);
bool has_roman_candidate(std::string_view text);
bool has_currency_candidate(std::string_view text);
bool has_symbol_candidate(std::string_view text);
bool is_ascii_acronym(std::string_view text);
bool is_uncertain_word_char(char32_t cp);
std::vector<Cp> uncertain_word_spans(std::string_view text);
void replace_all(std::string& text, std::string_view from, std::string_view to);
std::vector<std::string> split_words(std::string_view text);
std::string join(const std::vector<std::string>& words, std::string_view sep = " ");
unsigned long long parse_ull(std::string_view text);
std::optional<unsigned long long> try_parse_ull(std::string_view text);
std::string number_digits_or_words(std::string_view digits);
int parse_int(std::string_view text);
std::string trim_spaces(std::string text);

// Number-word helpers.
std::string plural(unsigned long long n, const Forms& forms);
void feminine_last(std::vector<std::string>& words);
void neuter_last(std::vector<std::string>& words);
std::vector<std::string> under_thousand(unsigned n);
std::string decimal_to_words(std::string_view int_part, std::string_view frac_part);
std::string number_words_for_gender(unsigned long long n, char gender);
bool prefers_many_after_genitive_number(unsigned long long n);
std::string inflect_ordinal(std::string stem, std::string_view form);
std::string hours_words(int hour);
std::string minutes_words(int minute, const Forms& forms = {"хвилина", "хвилини", "хвилин"});
std::string say_fraction(unsigned long long num, unsigned long long den);
std::string read_measurement_quantity(std::string_view num, const Measurement& meas);

// Lexicon tables and derived regex alternations.
std::string regex_alternation(std::vector<std::string> keys);
const std::unordered_map<std::string, std::string>& abbreviation_map();
std::string compact_spaces_lower(std::string_view text);
const std::unordered_map<std::string, std::string>& cyrillic_transliteration_map();
const std::unordered_map<std::string, std::string>& pronunciation_map();
const std::unordered_map<std::string, Measurement>& measurements();
const std::string& unit_alt();
const std::string& counted_noun_alt();
const std::string& month_alt();
const std::regex& date_day_range_re();
const std::regex& date_spelled_re();
const std::regex& range_units_re();
const std::regex& case_prep_re();
const std::regex& counted_ponad_re();
const std::regex& counted_genitive_re();
const std::regex& counted_nouns_re();
const std::regex& measurements_re();
const std::string& currency_token_alt();
const std::regex& symbol_currency_prefix_re();
const std::regex& symbol_currency_suffix_re();
const std::unordered_map<std::string, std::string>& cardinal_to_ordinal();
const std::unordered_map<std::string, CaseForms>& case_forms();
const std::unordered_map<std::string, CountedNoun>& counted_nouns();
const std::unordered_map<std::string, std::string_view>& counted_oblique_cases();
const std::unordered_map<std::string, std::string>& compound_prefix_forms();
const std::unordered_map<std::string, FinanceUnit>& finance_units();
const std::unordered_map<std::string, std::string>& english_words();

// Roman numerals.
int roman_to_int(std::string_view s);
bool valid_roman(std::string_view s);

// Calendar validation.
bool is_valid_date(int day, int month, int year);

// Identifier / finance / phone readers.
std::string spell_identifier_letters(std::string_view letters);
std::string read_identifier_number(std::string_view digits);
std::optional<std::string> read_roman_identifier_segment(std::string_view letters);
std::string read_identifier_segment(std::string_view segment);
std::string read_structured_identifier(std::string_view value);
std::string read_dotted(std::string_view num);
std::string finance_unit_many(std::string_view ticker);
std::string finance_amount_words(std::string amount, const FinanceUnit& unit);
std::string normalize_phone_number(std::string_view phone, PhoneStyle style);

// Normalization passes.
std::string normalize_unicode(std::string text, QuoteStyle quote_style);
std::string normalize_homoglyphs(std::string text);
std::string normalize_typography(std::string text);
std::string normalize_web(std::string text);
std::string normalize_dates(std::string text, DateStyle style, bool validate);
std::string normalize_discourse_dates(std::string text);
std::string normalize_ordinals(std::string text);
std::string normalize_quarters(std::string text);
std::string normalize_ranges(std::string text, RangeStyle style);
std::string normalize_addresses(std::string text);
std::string normalize_number_groups(std::string text, bool parse_thousand_separators);
std::string normalize_sections(std::string text);
std::string normalize_known_acronyms(std::string text);
std::string normalize_case_context(std::string text);
std::string normalize_counted_noun_context(std::string text);
std::string normalize_counted_nouns(std::string text);
std::string normalize_ordinal_triggers(std::string text);
std::string normalize_compounds(std::string text);
std::string normalize_time(std::string text);
std::string normalize_fractions(std::string text);
std::string normalize_percent(std::string text);
std::string normalize_measurements(std::string text);
std::string normalize_medical(std::string text);
std::string normalize_symbols(std::string text);
std::string normalize_math(std::string text);
std::string normalize_decimals(std::string text);
std::string normalize_overprecise_currency_decimals(std::string text);
std::string normalize_symbol_currency(std::string text);
std::string normalize_multipliers(std::string text);
std::string normalize_currency(std::string text);
std::string normalize_finance(std::string text);
std::string normalize_text_with_phone_numbers(std::string text, PhoneStyle style);
std::string normalize_ip_addresses(std::string text);
std::string normalize_coordinates(std::string text);
std::string normalize_identifiers(std::string text);
std::string normalize_versions(std::string text);
std::string normalize_negatives(std::string text);
std::string normalize_text_with_numbers(std::string text);
std::string normalize_english(std::string text);

// Regex/CTRE substitution helpers.
template <class Fn>
std::string regex_sub(std::string input, const std::regex& re, Fn fn)
{
    std::string out;
    std::sregex_iterator it(input.begin(), input.end(), re);
    std::sregex_iterator end;
    if (it == end) {
        return input;
    }
    out.reserve(input.size());
    std::size_t last = 0;
    for (; it != end; ++it) {
        const auto& m = *it;
        out.append(input, last, static_cast<std::size_t>(m.position()) - last);
        out += fn(m);
        last = static_cast<std::size_t>(m.position() + m.length());
    }
    out.append(input, last, std::string::npos);
    return out;
}

template <class Fn>
std::string regex_sub(std::string_view text, const std::regex& re, Fn fn)
{
    return regex_sub(std::string(text), re, std::move(fn));
}

template <std::size_t Index, class Match>
std::string_view cap(const Match& match)
{
    return match.template get<Index>().to_view();
}

template <class Match>
std::string whole_string(const Match& match)
{
    return std::string(cap<0>(match));
}

template <std::size_t Index, class Match>
std::string cap_string(const Match& match)
{
    return std::string(cap<Index>(match));
}

template <std::size_t Index = 0, class Match>
std::size_t cap_pos(std::string_view input, const Match& match)
{
    return static_cast<std::size_t>(cap<Index>(match).data() - input.data());
}

template <ctll::fixed_string Pattern, class Fn>
std::string ctre_sub(std::string input, Fn fn)
{
    const std::string_view view(input);
    auto matches = ctre::search_all<Pattern>(view);
    auto it = matches.begin();
    if (it == matches.end()) {
        return input;
    }

    std::string out;
    out.reserve(input.size());
    std::size_t last = 0;
    for (; it != matches.end(); ++it) {
        const auto match = *it;
        const auto whole = cap<0>(match);
        const auto pos = static_cast<std::size_t>(whole.data() - view.data());
        out.append(input, last, pos - last);
        out += fn(match);
        last = pos + whole.size();
    }
    out.append(input, last, std::string::npos);
    return out;
}

template <ctll::fixed_string Pattern, class Fn>
void ctre_each(std::string_view input, Fn fn)
{
    for (const auto& match : ctre::search_all<Pattern>(input)) {
        fn(match);
    }
}

} // namespace uktextnorm::detail
