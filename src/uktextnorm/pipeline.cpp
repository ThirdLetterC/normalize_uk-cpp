#include "uktextnorm/uktextnorm.hpp"

#include "internal.hpp"

#include "generated/uktextnorm_lexicons.hpp"

namespace uktextnorm {

using namespace detail;

NormalizeOptions options_for_preset(NormalizePreset preset)
{
    NormalizeOptions options;
    switch (preset) {
    case NormalizePreset::Default:
        return options;
    case NormalizePreset::TtsFriendly:
        options.range_style = RangeStyle::FromTo;
        options.phone_style = PhoneStyle::DigitByDigit;
        options.date_style = DateStyle::Spoken;
        options.quote_style = QuoteStyle::Strip;
        return options;
    case NormalizePreset::Conservative:
        options.repair_homoglyphs = false;
        options.expand_known_acronyms = false;
        options.spell_unknown_acronyms = false;
        options.normalize_english_words = false;
        options.transliterate_latin = false;
        options.symbol_style = SymbolStyle::Preserve;
        return options;
    case NormalizePreset::SearchIndexing:
        options.quote_style = QuoteStyle::Straight;
        options.spell_unknown_acronyms = false;
        options.normalize_english_words = false;
        options.transliterate_latin = false;
        options.symbol_style = SymbolStyle::Preserve;
        return options;
    }
    return options;
}

std::string normalize_ukrainian(std::string_view input)
{
    return normalize_ukrainian(input, NormalizeOptions{});
}

std::string normalize_ukrainian(std::string_view input, NormalizePreset preset)
{
    return normalize_ukrainian_with_preset(input, preset);
}

std::string normalize_ukrainian_with_preset(std::string_view input, NormalizePreset preset)
{
    return normalize_ukrainian(input, options_for_preset(preset));
}

std::string normalize_ukrainian(std::string_view input, const NormalizeOptions& options)
{
    std::string text(input);
    const auto maybe_digits = [&] { return has_ascii_digit(text); };
    const auto maybe_roman = [&] { return has_roman_candidate(text); };
    const auto maybe_currency = [&] { return has_currency_candidate(text); };

    text = normalize_unicode(std::move(text), options.quote_style);
    text = normalize_typography(std::move(text));
    if (options.repair_homoglyphs && has_ascii_alpha(text)) {
        text = normalize_homoglyphs(std::move(text));
    }
    if (contains_any(text, "@#") ||
        contains_any_token(
            text,
            {"http://", "https://", "www.", ".com", ".ua", ".org", ".net", ".info", ".io", ".edu", ".gov", ".укр"})) {
        text = normalize_web(std::move(text));
    }
    if (contains_any_token(text, {"кв.", "квартал"}) || maybe_roman()) {
        text = normalize_quarters(std::move(text));
    }
    if (text.contains('.')) {
        text = normalize_addresses(std::move(text));
    }
    text = normalize_abbreviations(text);
    if (maybe_digits()) {
        text = normalize_number_groups(std::move(text), options.parse_thousand_separators);
        if (options.normalize_network_addresses && contains_any(text, ".:")) {
            text = normalize_ip_addresses(std::move(text));
        }
        text = normalize_identifiers(std::move(text));
        if (contains_any(text, "-–—%") || contains_any_token(text, {" рр", "роки", "стор.", "с."})) {
            text = normalize_ranges(std::move(text), options.range_style);
        }
        text = normalize_dates(std::move(text), options.date_style, options.validate_dates);
        text = normalize_discourse_dates(std::move(text));
        if (contains_any(text, "/°№") || contains_any_token(text, {"мм рт", "раз", "тиск"})) {
            text = normalize_medical(std::move(text));
        }
        text = normalize_counted_noun_context(std::move(text));
        text = normalize_case_context(std::move(text));
        if (text.contains('.') || maybe_roman()) {
            text = normalize_sections(std::move(text));
        }
        if (contains_any(text, ":-–—")) {
            text = normalize_time(std::move(text));
        }
        text = normalize_counted_nouns(std::move(text));
        text = normalize_ordinal_triggers(std::move(text));
        if (contains_any(text, "-–—")) {
            text = normalize_compounds(std::move(text));
        }
        text = normalize_ordinals(std::move(text));
        if (contains_any(text, "/½⅓⅔¼¾⅕⅖⅗⅘⅙⅚⅐⅛⅜⅝⅞⅑⅒")) {
            text = normalize_fractions(std::move(text));
        }
        if (text.contains('%')) {
            text = normalize_percent(std::move(text));
        }
        if (contains_any(text, "°′″")) {
            text = normalize_coordinates(std::move(text));
        }
        if (maybe_currency()) {
            text = normalize_symbol_currency(std::move(text));
        }
        if (contains_any_token(text, {"тис", "млн", "млрд", "трлн"})) {
            text = normalize_multipliers(std::move(text));
        }
        text = normalize_measurements(std::move(text));
    } else if (maybe_roman()) {
        text = normalize_ordinals(std::move(text));
    }
    if (contains_any_token(text, {"BTC", "ETH", "USDT", "BNB", "USD", "EUR", "GBP", "UAH"})) {
        text = normalize_finance(std::move(text));
    }
    if (options.expand_known_acronyms) {
        text = normalize_known_acronyms(std::move(text));
    }
    if (options.spell_unknown_acronyms) {
        text = expand_abbreviations(text);
    }
    if (options.symbol_style == SymbolStyle::Expand) {
        if (text.contains('+') && maybe_digits()) {
            text = normalize_math(std::move(text));
        }
        if (has_symbol_candidate(text)) {
            text = normalize_symbols(std::move(text));
        }
    }
    if (maybe_digits()) {
        if (maybe_currency()) {
            text = normalize_overprecise_currency_decimals(std::move(text));
            text = normalize_currency(std::move(text));
        }
        if (text.contains(',')) {
            text = normalize_decimals(std::move(text));
        }
        text = normalize_text_with_phone_numbers(std::move(text), options.phone_style);
        if (text.contains('.')) {
            text = normalize_versions(std::move(text));
        }
        if (text.contains('-') || text.contains("−")) {
            text = normalize_negatives(std::move(text));
        }
        text = normalize_text_with_numbers(std::move(text));
    }
    if (options.normalize_english_words) {
        if (has_ascii_alpha(text)) {
            text = normalize_english(std::move(text));
        }
    }
    if (options.transliterate_latin) {
        if (has_ascii_alpha(text)) {
            text = transliterate_to_cyrillic(text);
        }
    }
    return trim_spaces(std::move(text));
}

std::vector<UncertainSpan> flag_uncertain(std::string_view text)
{
    std::vector<UncertainSpan> spans;
    const auto char_offsets = byte_to_char_offsets(text);
    std::set<std::pair<std::size_t, std::size_t>> seen;
    auto add = [&](std::size_t s,
                   std::size_t e,
                   std::string reason,
                   UncertaintyCategory category,
                   UncertaintySeverity severity) {
        while (s > 0 && is_utf8_continuation(text[s])) {
            --s;
        }
        while (e < text.size() && is_utf8_continuation(text[e])) {
            ++e;
        }
        if (seen.insert({s, e}).second) {
            spans.push_back({char_offsets[s],
                             char_offsets[e],
                             std::string(text.substr(s, e - s)),
                             std::move(reason),
                             category,
                             severity});
        }
    };
    std::string input(text);
    ctre_each<R"((^|[^\d])(\d{1,2})\.(\d{1,2})\.(\d{3,4})(?![\d]))">(input, [&](const auto& m) {
        const auto day = parse_int(cap<2>(m));
        const auto month = parse_int(cap<3>(m));
        if (day < 1 || day > 31 || month < 1 || month > 12) {
            const auto s = cap_pos<2>(input, m);
            add(s,
                s + cap<2>(m).size() + 1 + cap<3>(m).size() + 1 + cap<4>(m).size(),
                "invalid or ambiguous numeric date",
                UncertaintyCategory::Date,
                UncertaintySeverity::Error);
        } else if (!is_valid_date(day, month, parse_int(cap<4>(m)))) {
            const auto s = cap_pos<2>(input, m);
            add(s,
                s + cap<2>(m).size() + 1 + cap<3>(m).size() + 1 + cap<4>(m).size(),
                "calendar-invalid date (day does not exist in that month)",
                UncertaintyCategory::InvalidDate,
                UncertaintySeverity::Error);
        }
    });
    ctre_each<R"((^|[^\d.,])(\d{1,3},\d{3})(?![\d]))">(input, [&](const auto& m) {
        const auto s = cap_pos<2>(input, m);
        add(s,
            s + cap<2>(m).size(),
            "single comma group (decimal or thousands separator?)",
            UncertaintyCategory::AmbiguousNumberGrouping,
            UncertaintySeverity::Warning);
    });
    static const std::unordered_map<std::string, std::string> multisense = {
        {"р", "рік / рядок / річка"},
        {"м", "метр / місто"},
        {"с", "секунда / село / сторінка"},
        {"в", "вік / вулиця / прийменник"},
        {"кв", "квартира / квартал / квадратний"},
        {"ст", "століття / стаття / станція / сторінка"},
        {"п", "пункт / пан / поверх"},
        {"обл", "область / обліковий"}};
    static const std::regex abbr(R"((^|[^А-Яа-яЄєІіЇїҐґ])(кв|обл|ст|р|м|с|в|п)\.(?![а-яіїєґ]))", std::regex::icase);
    for (std::sregex_iterator it(input.begin(), input.end(), abbr), end; it != end; ++it) {
        const auto s = static_cast<std::size_t>((*it).position(2));
        auto left = input.substr(0, s);
        while (!left.empty() && left.back() == ' ') {
            left.pop_back();
        }
        if (!left.empty() && std::isdigit(static_cast<unsigned char>(left.back()))) {
            continue;
        }
        const auto key = lower_text((*it)[2].str());
        add(s,
            s + (*it)[2].length() + 1,
            "ambiguous abbreviation (" + multisense.at(key) + ")",
            UncertaintyCategory::AmbiguousAbbreviation,
            UncertaintySeverity::Warning);
    }
    for (const auto& word : uncertain_word_spans(input)) {
        const auto token = std::string_view(input).substr(word.start, word.stop - word.start);
        bool has_latin = false;
        bool has_uk = false;
        bool has_non_joiner_uk = false;
        for (std::size_t i = word.start; i < word.stop;) {
            std::size_t next = i + 1;
            const auto cp = decode_one(input, i, next);
            has_latin = has_latin || is_latin(cp);
            has_uk = has_uk || is_uk(cp);
            has_non_joiner_uk = has_non_joiner_uk || (is_uk(cp) && !is_word_joiner(cp));
            i = next;
        }
        if (has_latin && has_non_joiner_uk) {
            add(word.start,
                word.stop,
                "mixed-script word (possible typo or spoofing)",
                UncertaintyCategory::MixedScript,
                UncertaintySeverity::Error);
        }
        std::size_t first_next = word.start + 1;
        if (!has_latin || has_non_joiner_uk || !is_latin(decode_one(input, word.start, first_next))) {
            continue;
        }
        if (is_ascii_acronym(token) || english_words().contains(lower_text(token))) {
            continue;
        }
        if (word.start > 0) {
            std::size_t before_start = 0;
            for (std::size_t i = 0; i < word.start;) {
                before_start = i;
                std::size_t next = i + 1;
                decode_one(input, i, next);
                i = next;
            }
            std::size_t ignored = before_start + 1;
            if (is_uk(decode_one(input, before_start, ignored))) {
                continue;
            }
        }
        if (word.stop < input.size()) {
            std::size_t next = word.stop + 1;
            if (is_uk(decode_one(input, word.stop, next))) {
                continue;
            }
        }
        add(word.start,
            word.stop,
            "foreign word (transliteration is approximate)",
            UncertaintyCategory::ForeignWord,
            UncertaintySeverity::Info);
    }
    static const std::unordered_set<std::string> roman_stop = {
        "CD", "DVD", "MD", "DC", "MC", "MI", "MM", "DI", "DIV", "MIX", "CIV", "LCD"};
    ctre_each<R"(\b[MDCLXVI]{2,}\b)">(input, [&](const auto& m) {
        const auto w = whole_string(m);
        if (!roman_stop.contains(w) && valid_roman(w)) {
            const auto s = cap_pos(input, m);
            add(s,
                s + cap<0>(m).size(),
                "Roman numeral (case defaults to nominative)",
                UncertaintyCategory::RomanNumeral,
                UncertaintySeverity::Info);
        }
    });
    static const std::regex identifier(
        R"((?:№\s*[A-Za-zА-Яа-яЄєІіЇїҐґ0-9]+(?:[-/][A-Za-zА-Яа-яЄєІіЇїҐґ0-9]+)+|(?:ЄДРПОУ|РНОКПП|ІПН|ЄРДР)\.?\s*[:№#]?\s*\d{6,20}|паспорт\s+[A-Za-zА-Яа-яЄєІіЇїҐґ]{2}\s*\d{6,9}|(?:картка|картку|карта|карту)\s*\d{4}[\s-]+(?:(?:\*{4}|xxxx|XXXX)[\s-]+(?:\*{4}|xxxx|XXXX)|\d{4}[\s-]+\d{4})[\s-]+\d{4}))",
        std::regex::icase);
    for (std::sregex_iterator it(input.begin(), input.end(), identifier), end; it != end; ++it) {
        add((*it).position(),
            (*it).position() + (*it).length(),
            "structured identifier (domain-specific reading may vary)",
            UncertaintyCategory::Identifier,
            UncertaintySeverity::Info);
    }
    ctre_each<R"(\b[A-Za-z0-9._%+\-]+@(?:\s|$|[^\s@.]+(?:\s|$)|[^\s@]*\.\s))">(input, [&](const auto& m) {
        const auto s = cap_pos(input, m);
        add(s,
            s + cap<0>(m).size(),
            "malformed email-like contact",
            UncertaintyCategory::Web,
            UncertaintySeverity::Warning);
    });
    static const std::regex malformed_url(R"(\bhttps?://(?:\s|$)|\bwww\.(?:\s|$))", std::regex::icase);
    for (std::sregex_iterator it(input.begin(), input.end(), malformed_url), end; it != end; ++it) {
        add((*it).position(),
            (*it).position() + (*it).length(),
            "malformed URL-like token",
            UncertaintyCategory::Web,
            UncertaintySeverity::Warning);
    }
    static const std::regex unsupported_currency(
        R"((^|[^\dA-Za-zА-Яа-яЄєІіЇїҐґ])(\d+(?:[.,]\d+)?)\s*(RUB|KRW|BRL|ZAR|₽|₩)(?![A-Za-zА-Яа-яЄєІіЇїҐґ]))",
        std::regex::icase);
    for (std::sregex_iterator it(input.begin(), input.end(), unsupported_currency), end; it != end; ++it) {
        const auto s = static_cast<std::size_t>((*it).position(2));
        const auto e = static_cast<std::size_t>((*it).position(0) + (*it).length(0));
        add(s,
            e,
            "unsupported or ambiguous currency token",
            UncertaintyCategory::Currency,
            UncertaintySeverity::Warning);
    }
    static const std::unordered_set<std::string> known_unit_words = [] {
        std::unordered_set<std::string> out = {"грн", "коп", "btc",  "eth",  "usdt", "bnb",  "тис",
                                               "млн", "млрд", "трлн", "рік",  "року", "році", "раз",
                                               "рази", "разів"};
        for (const auto& entry : lexicon::kCurrencies) {
            out.insert(lower_text(entry.code));
            out.insert(lower_text(entry.main_one));
            out.insert(lower_text(entry.main_few));
            out.insert(lower_text(entry.main_many));
            out.insert(lower_text(entry.sub_one));
            out.insert(lower_text(entry.sub_few));
            out.insert(lower_text(entry.sub_many));
        }
        return out;
    }();
    ctre_each<R"((^|[^\d.,])(\d+(?:[.,]\d+)?)\s*([A-Za-zА-Яа-яЄєІіЇїҐґ]{1,6})(?![A-Za-zА-Яа-яЄєІіЇїҐґ]))">(
        input, [&](const auto& m) {
            const auto original_unit = cap_string<3>(m);
            const auto unit = lower_text(original_unit);
            if (measurements().contains(original_unit) || measurements().contains(unit) ||
                known_unit_words.contains(unit) || counted_nouns().contains(unit)) {
                return;
            }
            const auto s = cap_pos<2>(input, m);
            const auto e = cap_pos<3>(input, m) + cap<3>(m).size();
            add(s,
                e,
                "unknown unit or unsupported unit spelling",
                UncertaintyCategory::Unit,
                UncertaintySeverity::Warning);
        });
    ctre_each<R"((^|[^\d.,])(\d{4})(?!\d|[.,]\d))">(input, [&](const auto& m) {
        const auto n = parse_int(cap<2>(m));
        if (n < 1000 || n > 2099) {
            return;
        }
        const auto e = cap_pos<2>(input, m) + cap<2>(m).size();
        const auto after = input.substr(e, 16);
        if (ctre::search<R"(^\s*(?:рік|року|році|р\.|рр\.|ст\.))">(after)) {
            return;
        }
        add(cap_pos<2>(input, m),
            e,
            "four-digit number (year or cardinal?)",
            UncertaintyCategory::BareNumber,
            UncertaintySeverity::Warning);
    });
    static const std::regex cue_after("^\\s*(?:" + unit_alt() + R"(|%|грн|коп|рік|року|році|тис|млн|млрд|[-–—]))",
                                      std::regex::icase);
    static const std::unordered_set<std::string> governors = {
        "близько", "понад", "менше", "більше", "від", "до",  "із", "з",  "без", "після",
        "к",       "у",     "в",     "о",      "об",  "при", "над", "під", "перед", "між"};
    ctre_each<R"((^|[^\d.,:%\-])(\d{1,4})(?![\d.,:%/\-]))">(input, [&](const auto& m) {
        const auto s = cap_pos<2>(input, m);
        const auto e = s + cap<2>(m).size();
        if (seen.contains({s, e})) {
            return;
        }
        const auto digits = cap_string<2>(m);
        if (digits.size() > 1 && digits[0] == '0') {
            return;
        }
        auto left = input.substr(0, s);
        if (auto prev = ctre::search<R"(([А-Яа-яЄєІіЇїҐґ]+)$)">(left);
            prev && governors.contains(lower_text(cap<1>(prev)))) {
            return;
        }
        if (std::regex_search(input.substr(e), cue_after)) {
            return;
        }
        add(s,
            e,
            "bare number (case / cardinal-vs-ordinal undetermined)",
            UncertaintyCategory::BareNumber,
            UncertaintySeverity::Warning);
    });
    static const std::regex agreement_re(
        R"((^|[^А-Яа-яЄєІіЇїҐґ-])(близько|понад|перед|між|над|під|при|після|без|від|до|із|у|в|на|з)\s+(\d{1,6})\s+([^\s\d,.;:!?()]{3,}))",
        std::regex::icase);
    for (std::sregex_iterator it(input.begin(), input.end(), agreement_re), end; it != end; ++it) {
        const auto noun = lower_text((*it)[4].str());
        if (counted_nouns().contains(noun) || measurements().contains((*it)[4].str()) ||
            measurements().contains(noun)) {
            continue;
        }
        if (counted_oblique_cases().contains(noun)) {
            continue;
        }
        const auto s = static_cast<std::size_t>((*it).position(3));
        const auto e = static_cast<std::size_t>((*it).position(4) + (*it).length(4));
        add(s,
            e,
            "number-noun agreement not verified (noun outside lexicon)",
            UncertaintyCategory::Agreement,
            UncertaintySeverity::Info);
    }

    std::sort(spans.begin(), spans.end(), [](const auto& a, const auto& b) {
        return std::tie(a.start, a.stop) < std::tie(b.start, b.stop);
    });
    return spans;
}

} // namespace uktextnorm
