#include "uktextnorm/uktextnorm.hpp"

#include "generated/uktextnorm_lexicons.hpp"
#include "numeric_internal.hpp"

namespace uktextnorm::detail {

std::string normalize_ordinals(std::string text)
{
    static const std::unordered_map<std::string, std::string_view> suffix_form = {{"й", "nom_m"},
                                                                                  {"ший", "nom_m"},
                                                                                  {"го", "gen"},
                                                                                  {"му", "dat"},
                                                                                  {"м", "prep"},
                                                                                  {"а", "nom_f"},
                                                                                  {"ша", "nom_f"},
                                                                                  {"га", "nom_f"},
                                                                                  {"тя", "nom_f"},
                                                                                  {"у", "acc_f"},
                                                                                  {"е", "nom_n"},
                                                                                  {"ше", "nom_n"},
                                                                                  {"ге", "nom_n"},
                                                                                  {"тє", "nom_n"},
                                                                                  {"х", "pl"},
                                                                                  {"им", "ins"},
                                                                                  {"ім", "ins"},
                                                                                  {"ою", "ins_f"},
                                                                                  {"ій", "loc_f"},
                                                                                  {"ими", "ins_pl"}};
    static const std::unordered_set<std::string> stop = {
        "CD", "DVD", "MD", "DC", "MC", "MI", "MM", "DI", "DIV", "DVI", "DL", "CLI", "MIX", "CIV", "LCD"};
    // Ukrainian Wikipedia commonly writes Roman centuries with Cyrillic
    // homoglyphs (ХХІ). Repair only in a century context, never in identifiers.
    auto cyrillic_roman_value = [](std::string token) -> std::optional<int> {
        if (token.find("Х") == std::string::npos && token.find("І") == std::string::npos) {
            return std::nullopt;
        }
        replace_all(token, "Х", "X");
        replace_all(token, "І", "I");
        return valid_roman(token) ? std::optional<int>{roman_to_int(token)} : std::nullopt;
    };
    static const std::regex cyrillic_century_range(
        R"((^|[^A-Za-zА-Яа-яЄєІіЇїҐґ])((?:Х|І|X|I|V|M|C|D|L){1,8})\s*(?:-|–|—)\s*((?:Х|І|X|I|V|M|C|D|L){1,8})\s*(ст\.|століття|столітті|сторіччя|сторіччі)(?![А-Яа-яЄєІіЇїҐґ]))");
    text = regex_sub(text, cyrillic_century_range, [&](const std::smatch& m) {
        const auto first = cyrillic_roman_value(m[2].str());
        const auto second = cyrillic_roman_value(m[3].str());
        if (!first || !second) {
            return m.str();
        }
        auto left = lower_text(m.prefix().str() + m[1].str());
        while (!left.empty() && std::isspace(static_cast<unsigned char>(left.back()))) {
            left.pop_back();
        }
        const auto boundary = left.find_last_of(" \t\n");
        const auto word = left.substr(boundary == std::string::npos ? 0 : boundary + 1);
        const auto noun = m[4].str().starts_with("сторіч") ? "сторіччя" : "століття";
        if (word == "у" || word == "в" || m[4].str() == "столітті" || m[4].str() == "сторіччі") {
            return m[1].str() + number_to_ordinal_words(*first, "prep") + "–" +
                   number_to_ordinal_words(*second, "prep") +
                   (std::string_view(noun) == "сторіччя" ? " сторіччях" : " століттях");
        }
        return m[1].str() + "від " + number_to_ordinal_words(*first, "gen") + " до " +
               number_to_ordinal_words(*second, "gen") + " " + noun;
    });
    static const std::regex cyrillic_century(
        R"((^|[^A-Za-zА-Яа-яЄєІіЇїҐґ])((?:Х|І|X|I|V|M|C|D|L){1,8})\s*(ст\.|століття|столітті|сторіччя|сторіччі)(?![А-Яа-яЄєІіЇїҐґ]))");
    text = regex_sub(text, cyrillic_century, [&](const std::smatch& m) {
        const auto value = cyrillic_roman_value(m[2].str());
        if (!value) {
            return m.str();
        }
        auto left = lower_text(m.prefix().str() + m[1].str());
        while (!left.empty() && std::isspace(static_cast<unsigned char>(left.back()))) {
            left.pop_back();
        }
        const auto boundary = left.find_last_of(" \t\n");
        const auto word = left.substr(boundary == std::string::npos ? 0 : boundary + 1);
        const bool locative = word == "у" || word == "в" || m[3].str() == "столітті" || m[3].str() == "сторіччі";
        const bool genitive = word == "початку" || word == "кінця" || word == "середини" || word == "половини";
        const auto form = locative ? "prep" : genitive ? "gen" : "nom_n";
        const auto noun = m[3].str().starts_with("сторіч") ? (locative ? " сторіччі" : " сторіччя")
                                                           : (locative ? " столітті" : " століття");
        return m[1].str() + number_to_ordinal_words(*value, form) + noun;
    });
    static const std::regex bare_cyrillic_century_before_start(
        R"((Протягом|протягом|Впродовж|впродовж|Упродовж|упродовж)\s+((?:Х|І|X|I|V|M|C|D|L){1,8})\s+та\s+початку)");
    text = regex_sub(text, bare_cyrillic_century_before_start, [&](const std::smatch& m) {
        const auto value = cyrillic_roman_value(m[2].str());
        return value ? m[1].str() + " " + number_to_ordinal_words(*value, "gen") + " століття та початку" : m.str();
    });
    // Keep these patterns in std::regex rather than CTRE. The CTRE expansion
    // for these UTF-8 lookahead/alternation expressions has high runtime stack
    // usage; on the default 1 MiB Windows executable stack even a short input
    // can terminate with STATUS_STACK_OVERFLOW.
    static const std::regex ordinal_suffix(R"((\d+)(?:-|–|—)(ший|ими|им|ім|ою|ій|ше|ша|ге|га|тє|тя|го|му|й|м|а|у|е|х)(?![А-Яа-яЄєІіЇїҐґ]))");
    text = regex_sub(text, ordinal_suffix, [&](const std::smatch& m) {
        return number_to_ordinal_words(parse_ull(m[1].str()), suffix_form.at(m[2].str()));
    });
    static const std::regex roman_century_range(
        R"((^|[^A-Za-z])([MDCLXVI]{1,6})\s*(?:-|–|—)\s*([MDCLXVI]{1,6})\s*(?:ст\.|століття)(?![А-Яа-яЄєІіЇїҐґ]))");
    text = regex_sub(text, roman_century_range, [](const std::smatch& m) {
        const auto start = m[2].str();
        const auto stop = m[3].str();
        if (!valid_roman(start) || !valid_roman(stop)) {
            return m.str();
        }
        return m[1].str() + number_to_ordinal_words(roman_to_int(start), "nom_n") + " " +
               number_to_ordinal_words(roman_to_int(stop), "nom_n") + " століття";
    });
    static const std::regex roman_section_range(
        R"((^|[^A-Za-z])([MDCLXVI]{1,6})\s*(?:-|–|—)\s*([MDCLXVI]{1,6})\s*(розд\.|розділ)(?![А-Яа-яЄєІіЇїҐґ]))");
    text = regex_sub(text, roman_section_range, [](const std::smatch& m) {
        const auto start = m[2].str();
        const auto stop = m[3].str();
        if (!valid_roman(start) || !valid_roman(stop)) {
            return m.str();
        }
        return m[1].str() + number_to_ordinal_words(roman_to_int(start), "nom_m") + " " +
               number_to_ordinal_words(roman_to_int(stop), "nom_m") + " розділ";
    });
    static const std::regex roman_century(R"((^|[^A-Za-z])([MDCLXVI]{1,6})\s*(?:ст\.|століття)(?![А-Яа-яЄєІіЇїҐґ]))");
    text = regex_sub(text, roman_century, [](const std::smatch& m) {
        const auto tok = m[2].str();
        if (!valid_roman(tok)) {
            return m.str();
        }
        return m[1].str() + number_to_ordinal_words(roman_to_int(tok), "nom_n") + " століття";
    });
    static const std::regex roman_group(R"((^|[^A-Za-z])([MDCLXVI]{1,6})\s+(група|групи)(?![А-Яа-яЄєІіЇїҐґ]))",
                                        std::regex::icase);
    text = regex_sub(text, roman_group, [](const std::smatch& m) {
        const auto token = m[2].str();
        if (!valid_roman(token)) {
            return m.str();
        }
        const auto form = lower_text(m[3].str()) == "групи" ? "gen_f" : "nom_f";
        return m[1].str() + number_to_ordinal_words(roman_to_int(token), form) + " " + m[3].str();
    });
    static const std::regex bare_roman(R"(\b[MDCLXVI]{2,}\b)");
    return regex_sub(text, bare_roman, [&](const std::smatch& m) {
        const auto tok = m.str();
        if (stop.contains(tok) || !valid_roman(tok)) {
            return tok;
        }
        return number_to_ordinal_words(roman_to_int(tok), "nom_m");
    });
}

std::string normalize_quarters(std::string text)
{
    static const std::regex roman_q(
        R"((^|[^A-Za-z])([MDCLXVI]{1,6})\s*(?:кв\.|квартал)(?:\s+(\d{3,4})(?:\s*р\.|\s+року)?)?(?![А-Яа-яЄєІіЇїҐґ]))");
    static const std::regex num_q(
        R"((^|[^\d])(\d{1,2})(?:[-–—]?(?:й|ій))?\s*(?:кв\.|квартал)(?:\s+(\d{3,4})(?:\s*р\.|\s+року)?)?(?![А-Яа-яЄєІіЇїҐґ]))");
    auto say = [](unsigned long long q, const std::ssub_match& year) {
        if (q < 1 || q > 4) {
            return std::string{};
        }
        std::string out = number_to_ordinal_words(q, "nom_m") + " квартал";
        if (year.matched) {
            out += " " + number_to_ordinal_words(parse_ull(year.str()), "gen") + " року";
        }
        return out;
    };
    text = regex_sub(text, roman_q, [&](const std::smatch& m) {
        const auto tok = m[2].str();
        if (!valid_roman(tok)) {
            return m.str();
        }
        const auto out = say(static_cast<unsigned long long>(roman_to_int(tok)), m[3]);
        return out.empty() ? m.str() : m[1].str() + out;
    });
    return regex_sub(text, num_q, [&](const std::smatch& m) {
        const auto out = say(parse_ull(m[2].str()), m[3]);
        return out.empty() ? m.str() : m[1].str() + out;
    });
}

std::string normalize_page_ranges(std::string text, RangeStyle style)
{
    static const std::regex bibliographic_volumes(R"((^|[\s,:;])(У|у|В|в)\s+(\d+)\s+(?:т|Т)(?:т|Т)?\.?([ \t]*)(?=/))");
    text = regex_sub(text, bibliographic_volumes, [](const std::smatch& m) {
        const auto count = parse_ull(m[3].str());
        const bool singular = count % 10 == 1 && count % 100 != 11;
        return m[1].str() + m[2].str() + " " + number_to_words_case(count, "prep") + (singular ? " томі" : " томах") +
               m[4].str();
    });
    static const std::regex page_count(R"((^|(?:-|—)\s+)(\d+)\s+(?:с|С)\.(?=\s*(?::|;|-|—|ISBN|$)))",
                                       std::regex::icase);
    text = regex_sub(text, page_count, [](const std::smatch& m) {
        const auto count = parse_ull(m[2].str());
        return m[1].str() + number_words_for_gender(count, 'f') + " " +
               plural(count, {"сторінка", "сторінки", "сторінок"}) + (m.suffix().str().empty() ? "." : "");
    });
    static const std::regex page_range(
        R"((^|[^А-Яа-яЄєІіЇїҐґA-Za-z])(?:стор\.|Стор\.|СТОР\.|с\.|С\.|pp?\.)\s*(\d+)\s*(?:-|−|–|—)\s*(\d+)(?!\d))",
        std::regex::icase);
    text = regex_sub(text, page_range, [&](const std::smatch& m) {
        const auto low = try_parse_ull(m[2].str());
        const auto high = try_parse_ull(m[3].str());
        if (!low || !high) {
            return m.str();
        }
        if (style == RangeStyle::FromTo) {
            return m[1].str() + "від " + number_to_ordinal_words(*low, "gen_f") + " до " +
                   number_to_ordinal_words(*high, "gen_f") + " сторінки";
        }
        return m[1].str() + "сторінки " + number_to_words(*low) + " " + number_to_words(*high);
    });
    static const std::regex single_page(R"((^|[^А-Яа-яЄєІіЇїҐґA-Za-z])(?:стор|Стор|СТОР|с|С|pp?)\.\s*(\d+)(?!\d))",
                                        std::regex::icase);
    return regex_sub(text, single_page, [](const std::smatch& m) {
        return m[1].str() + "сторінка " + number_to_words(parse_ull(m[2].str()));
    });
}

std::string normalize_section_ranges(std::string text, RangeStyle style)
{
    struct SectionRange {
        std::string_view compact;
        std::string_view genitive;
        std::string_view ordinal_form;
    };
    static const std::unordered_map<std::string, SectionRange> sections = {
        {"ст", {"статті", "статті", "gen_f"}},
        {"статті", {"статті", "статті", "gen_f"}},
        {"ч", {"частини", "частини", "gen_f"}},
        {"частини", {"частини", "частини", "gen_f"}},
        {"п", {"пункти", "пункту", "gen"}},
        {"пункти", {"пункти", "пункту", "gen"}},
        {"пп", {"підпункти", "підпункту", "gen"}},
        {"підпункти", {"підпункти", "підпункту", "gen"}},
        {"абз", {"абзаци", "абзацу", "gen"}},
        {"розд", {"розділи", "розділу", "gen"}},
        {"гл", {"глави", "глави", "gen_f"}},
        {"табл", {"таблиці", "таблиці", "gen_f"}},
        {"рис", {"рисунки", "рисунка", "gen"}},
    };
    static const std::regex dotted_range(
        R"((^|[^А-Яа-яЄєІіЇїҐґA-Za-z])(пп|підпункти)\.?\s*(\d+(?:\.\d+)+)\s*(?:-|−|–|—)\s*(\d+(?:\.\d+)+)(?![\d.]))",
        std::regex::icase);
    text = regex_sub(text, dotted_range, [&](const std::smatch& m) {
        if (style == RangeStyle::FromTo) {
            return m[1].str() + "від підпункту " + read_dotted(m[3].str()) + " до підпункту " + read_dotted(m[4].str());
        }
        return m[1].str() + "підпункти " + read_dotted(m[3].str()) + " " + read_dotted(m[4].str());
    });
    static const std::regex range(
        R"((^|[^А-Яа-яЄєІіЇїҐґA-Za-z])(ст|статті|ч|частини|пп|підпункти|п|пункти|абз|розд|гл|табл|рис)\.?\s*(\d+)\s*(?:-|−|–|—)\s*(\d+)(?!\d))",
        std::regex::icase);
    return regex_sub(text, range, [&](const std::smatch& m) {
        const auto low = try_parse_ull(m[3].str());
        const auto high = try_parse_ull(m[4].str());
        if (!low || !high) {
            return m.str();
        }
        const auto& section = sections.at(lower_text(m[2].str()));
        if (style == RangeStyle::FromTo) {
            return m[1].str() + "від " + number_to_ordinal_words(*low, section.ordinal_form) + " до " +
                   number_to_ordinal_words(*high, section.ordinal_form) + " " + std::string(section.genitive);
        }
        return m[1].str() + std::string(section.compact) + " " + number_to_words(*low) + " " + number_to_words(*high);
    });
}

std::string normalize_ranges(std::string text, RangeStyle style)
{
    const auto& number = signed_number_pattern();
    const auto& separator = range_separator_pattern();
    const auto& prefix = range_prefix_pattern();
    const auto& temperature_unit = temperature_unit_pattern();
    static const std::string number_boundary = R"((?![\d:/+\-−–—])(?![.,]\d))";
    static const std::string temperature_boundary = number_boundary + R"((?![A-Za-zА-Яа-яЄєІіЇїҐґ]))";

    enum class GovernedRange {
        None,
        From,
        To,
        Near,
        On,
        In
    };
    auto governed_range = [&](const std::smatch& m, bool explicitly_from_to) {
        if (style != RangeStyle::FromTo || explicitly_from_to) {
            return GovernedRange::None;
        }
        const auto word = preceding_word(m.prefix().str() + m[1].str());
        if (word == "від") {
            return GovernedRange::From;
        }
        if (word == "до") {
            return GovernedRange::To;
        }
        if (word == "близько") {
            return GovernedRange::Near;
        }
        if (word == "на") {
            return GovernedRange::On;
        }
        if (word == "в" || word == "у") {
            return GovernedRange::In;
        }
        return GovernedRange::None;
    };
    auto governed_connector =
        [&](const std::smatch& m, const std::string& low, const std::string& high, bool explicitly_from_to) {
            return governed_range(m, explicitly_from_to) == GovernedRange::None
                       ? range_connector(style, low, high, explicitly_from_to)
                       : low + "–" + high;
        };
    auto governed_case = [&](const std::smatch& m, bool explicitly_from_to) {
        const auto context = governed_range(m, explicitly_from_to);
        return style == RangeStyle::FromTo && context != GovernedRange::On && context != GovernedRange::In ? "gen"
               : explicitly_from_to                                                                        ? "gen"
                                                                                                           : "nom";
    };

    auto temperature_bound = [](std::string_view token, const TemperatureScale& scale, bool genitive) {
        if (!genitive) {
            return temperature_quantity_words(token, scale);
        }
        const auto words = signed_number_words(token, "gen");
        if (!words) {
            return std::optional<std::string>{};
        }
        return std::optional<std::string>{*words + " " + temperature_range_unit(token, scale) +
                                          (scale.genitive_name.empty() ? "" : " " + std::string(scale.genitive_name))};
    };

    auto say_temperature = [&](const std::smatch& m,
                               std::size_t low_index,
                               std::size_t high_index,
                               std::size_t scale_index,
                               bool explicitly_from_to) {
        const auto grammatical_case = governed_case(m, explicitly_from_to);
        const auto low = signed_number_words(m[low_index].str(), grammatical_case);
        const auto high = signed_number_words(m[high_index].str(), grammatical_case);
        if (!low || !high) {
            return m.str();
        }
        const auto scale = temperature_scale(m[scale_index].str());
        if (!scale) {
            return m.str();
        }
        return m[1].str() + governed_connector(m, *low, *high, explicitly_from_to) + " " +
               temperature_range_unit(m[high_index].str(), *scale) +
               (scale->genitive_name.empty() ? "" : " " + std::string(scale->genitive_name));
    };

    static const std::regex explicit_repeated_temperature(prefix + "від\\s+(" + number + ")\\s*(" + temperature_unit +
                                                          ")\\s+до\\s+(" + number + ")\\s*(" + temperature_unit + ")" +
                                                          temperature_boundary);
    text = regex_sub(text, explicit_repeated_temperature, [&](const std::smatch& m) {
        const auto low_scale = temperature_scale(m[3].str());
        const auto high_scale = temperature_scale(m[5].str());
        if (!low_scale || !high_scale) {
            return m.str();
        }
        if (low_scale->kind != high_scale->kind) {
            const auto low = temperature_bound(m[2].str(), *low_scale, true);
            const auto high = temperature_bound(m[4].str(), *high_scale, true);
            return low && high ? m[1].str() + "від " + *low + " до " + *high : m.str();
        }
        return say_temperature(m, 2, 4, 5, true);
    });
    static const std::regex repeated_temperature(prefix + "(" + number + ")\\s*(" + temperature_unit + ")\\s*" +
                                                 separator + "\\s*(" + number + ")\\s*(" + temperature_unit + ")" +
                                                 temperature_boundary);
    text = regex_sub(text, repeated_temperature, [&](const std::smatch& m) {
        const auto low_scale = temperature_scale(m[3].str());
        const auto high_scale = temperature_scale(m[5].str());
        if (!low_scale || !high_scale) {
            return m.str();
        }
        if (low_scale->kind != high_scale->kind) {
            const bool genitive = style == RangeStyle::FromTo;
            const auto low = temperature_bound(m[2].str(), *low_scale, genitive);
            const auto high = temperature_bound(m[4].str(), *high_scale, genitive);
            if (!low || !high) {
                return m.str();
            }
            return m[1].str() + governed_connector(m, *low, *high, false);
        }
        return say_temperature(m, 2, 4, 5, false);
    });
    static const std::regex explicit_temperature(prefix + "від\\s+(" + number + ")\\s+до\\s+(" + number + ")\\s*(" +
                                                 temperature_unit + ")" + temperature_boundary);
    text =
        regex_sub(text, explicit_temperature, [&](const std::smatch& m) { return say_temperature(m, 2, 3, 4, true); });
    static const std::regex temperature_range(prefix + "(" + number + ")\\s*" + separator + "\\s*(" + number +
                                              ")\\s*(" + temperature_unit + ")" + temperature_boundary);
    text = regex_sub(text, temperature_range, [&](const std::smatch& m) { return say_temperature(m, 2, 3, 4, false); });

    static const std::regex prepositional_year_range(
        R"((^|[^А-Яа-яЄєІіЇїҐґ])(У|у|В|в)\s+(\d{3,4})\s*(?:-|−|–|—)\s*(\d{3,4})\s*(?:рр\.?|роки|роках|року|років)(?![\dа-яіїєґ]))");
    const auto say_prepositional_year_range = [](const std::smatch& m) {
        return m[1].str() + m[2].str() + " період від " + number_to_ordinal_words(parse_ull(m[3].str()), "gen") + " до " +
               number_to_ordinal_words(parse_ull(m[4].str()), "gen") + " року";
    };
    text = regex_sub(text, prepositional_year_range, say_prepositional_year_range);
    // Without an explicit year word, only treat a four-digit span as years.
    static const std::regex bare_prepositional_year_range(
        R"((^|[^А-Яа-яЄєІіЇїҐґ])(У|у|В|в)\s+(\d{4})\s*(?:-|−|–|—)\s*(\d{4})(?![\dа-яіїєґ]))");
    text = regex_sub(text, bare_prepositional_year_range, say_prepositional_year_range);
    auto expanded_short_year = [](unsigned long long first, unsigned long long short_second) {
        auto second = (first / 100) * 100 + short_second;
        if (second < first) {
            second += 100;
        }
        return second;
    };
    static const std::regex abbreviated_decade_range(
        R"(\b((?:19|20)\d{2})\s*(?:-|–|—)\s*(\d{2})(?:-|–|—)?(х|их|і|ї)(?:\s+(роках|років|роки))?(?![\dА-Яа-яЄєІіЇїҐґ]))");
    text = regex_sub(text, abbreviated_decade_range, [&](const std::smatch& m) {
        const auto first = parse_ull(m[1].str());
        const auto second = expanded_short_year(first, parse_ull(m[2].str()));
        if (first / 100 != second / 100 || first % 10 != 0 || second % 10 != 0) {
            return m.str();
        }
        const auto form = m[3].str() == "і" || m[3].str() == "ї" ? "nom_pl" : "pl";
        const auto year_word = m[4].matched ? m[4].str() : (form == std::string_view("nom_pl") ? "роки" : "роках");
        return number_to_ordinal_words(first % 100, form) + "–" + number_to_ordinal_words(second % 100, form) + " " +
               year_word + " " + number_to_ordinal_words(first / 100 + 1, "gen") + " століття";
    });
    static const std::regex abbreviated_prepositional_year_range(
        R"((^|[^А-Яа-яЄєІіЇїҐґ])(У|у|В|в)\s+((?:19|20)\d{2})\s*(?:-|–|—)\s*(\d{2})\s*(?:рр?\.?|роки|роках|року|років)(?![\dа-яіїєґ]))");
    text = regex_sub(text, abbreviated_prepositional_year_range, [&](const std::smatch& m) {
        const auto first = parse_ull(m[3].str());
        const auto second = expanded_short_year(first, parse_ull(m[4].str()));
        return m[1].str() + m[2].str() + " період від " + number_to_ordinal_words(first, "gen") + " до " +
               number_to_ordinal_words(second, "gen") + " року";
    });
    static const std::regex abbreviated_year_range(
        R"(\b((?:19|20)\d{2})\s*(?:-|–|—)\s*(\d{2})\s*(?:рр?\.?|роки|роках|року|років)(?![\dа-яіїєґ]))");
    text = regex_sub(text, abbreviated_year_range, [&](const std::smatch& m) {
        const auto first = parse_ull(m[1].str());
        const auto second = expanded_short_year(first, parse_ull(m[2].str()));
        return style == RangeStyle::FromTo
                   ? "від " + number_to_ordinal_words(first, "gen") + " до " + number_to_ordinal_words(second, "gen") +
                         " року"
                   : number_to_ordinal_words(first, "nom_m") + "–" + number_to_ordinal_words(second, "nom_m") + " роки";
    });
    static const std::regex year_range(R"(\b(\d{3,4})\s*(?:-|−|–|—)\s*(\d{3,4})\s*(?:рр\.?|роки)(?![а-яіїєґ]))");
    text = regex_sub(text, year_range, [&](const std::smatch& m) {
        const auto low = parse_ull(m[1].str());
        const auto high = parse_ull(m[2].str());
        if (style == RangeStyle::FromTo) {
            const auto word = preceding_word(m.prefix().str());
            if (word == "на") {
                return "період від " + number_to_ordinal_words(low, "gen") + " до " +
                       number_to_ordinal_words(high, "gen") + " року";
            }
            if (word == "близько" || word == "до" || word == "від") {
                return number_to_ordinal_words(low, "gen") + "–" + number_to_ordinal_words(high, "gen") + " року";
            }
            return "від " + number_to_ordinal_words(low, "gen") + " до " + number_to_ordinal_words(high, "gen") +
                   " року";
        }
        return number_to_ordinal_words(low, "nom_m") + " " + number_to_ordinal_words(high, "nom_m") + " роки";
    });

    static const std::regex time_range(prefix + R"((\d{1,2}):([0-5]\d)(?::([0-5]\d))?\s*)" + separator +
                                       R"(\s*(\d{1,2}):([0-5]\d)(?::([0-5]\d))?(?![\d:]))");
    text = regex_sub(text, time_range, [&](const std::smatch& m) {
        if (parse_ull(m[2].str()) > 23 || parse_ull(m[5].str()) > 23) {
            return m.str();
        }
        const auto low = clock_time_words(m[2].str(), m[3].str(), m[4], style);
        const auto high = clock_time_words(m[5].str(), m[6].str(), m[7], style);
        return m[1].str() + range_connector(style, low, high);
    });

    static const std::regex fraction_range(prefix + R"((\d+)/(\d+)\s*)" + separator + R"(\s*(\d+)/(\d+)(?![\d/]))");
    text = regex_sub(text, fraction_range, [&](const std::smatch& m) {
        const auto n1 = try_parse_ull(m[2].str());
        const auto d1 = try_parse_ull(m[3].str());
        const auto n2 = try_parse_ull(m[4].str());
        const auto d2 = try_parse_ull(m[5].str());
        if (!n1 || !d1 || !n2 || !d2 || !*d1 || !*d2) {
            return m.str();
        }
        auto say = [&](unsigned long long numerator, unsigned long long denominator) {
            if (style != RangeStyle::FromTo) {
                return say_fraction(numerator, denominator);
            }
            return join(number_words_for_case(numerator, "gen", 'f')) + " " +
                   number_to_ordinal_words(denominator, numerator % 10 == 1 && numerator % 100 != 11 ? "gen_f" : "pl");
        };
        return m[1].str() + range_connector(style, say(*n1, *d1), say(*n2, *d2));
    });

    auto say_measurement = [&](const std::smatch& m,
                               std::size_t low_index,
                               std::size_t high_index,
                               std::size_t unit_index,
                               bool explicitly_from_to) {
        const auto& measurement = measurements().at(m[unit_index].str());
        const auto grammatical_case = governed_case(m, explicitly_from_to);
        const auto low = signed_number_words(m[low_index].str(), grammatical_case, measurement.gender);
        const auto high = signed_number_words(m[high_index].str(), grammatical_case, measurement.gender);
        if (!low || !high) {
            return m.str();
        }
        std::string unit(measurement.many);
        const auto context = governed_range(m, explicitly_from_to);
        if ((style == RangeStyle::Compact || context == GovernedRange::On || context == GovernedRange::In) &&
            !explicitly_from_to) {
            const auto upper_text = m[high_index].str();
            auto upper = std::string_view(upper_text);
            take_spoken_sign(upper);
            if (upper.find_first_of(".,") != std::string_view::npos) {
                unit = measurement.few;
            } else if (const auto value = try_parse_ull(upper)) {
                unit = plural(*value, {measurement.one, measurement.few, measurement.many});
            }
        }
        return m[1].str() + governed_connector(m, *low, *high, explicitly_from_to) + " " + unit;
    };
    static const std::regex repeated_unit_range(prefix + "(" + number + ")\\s*(" + unit_alt() + ")\\s*" + separator +
                                                "\\s*(" + number + ")\\s*(" + unit_alt() +
                                                R"()(?![A-Za-zА-Яа-яЄєІіЇїҐґ]))");
    text = regex_sub(text, repeated_unit_range, [&](const std::smatch& m) {
        return m[3].str() == m[5].str() ? say_measurement(m, 2, 4, 5, false) : m.str();
    });
    static const std::regex explicit_repeated_unit_range(prefix + "від\\s+(" + number + ")\\s*(" + unit_alt() +
                                                         ")\\s+до\\s+(" + number + ")\\s*(" + unit_alt() +
                                                         R"()(?![A-Za-zА-Яа-яЄєІіЇїҐґ]))");
    text = regex_sub(text, explicit_repeated_unit_range, [&](const std::smatch& m) {
        return m[3].str() == m[5].str() ? say_measurement(m, 2, 4, 5, true) : m.str();
    });
    static const std::regex explicit_unit_range(prefix + "від\\s+(" + number + ")\\s+до\\s+(" + number + ")\\s*(" +
                                                unit_alt() + R"()(?![A-Za-zА-Яа-яЄєІіЇїҐґ]))");
    text =
        regex_sub(text, explicit_unit_range, [&](const std::smatch& m) { return say_measurement(m, 2, 3, 4, true); });
    static const std::regex explicit_separator_unit_range(prefix + "від\\s+(" + number + ")\\s*" + separator + "\\s*(" +
                                                          number + ")\\s*(" + unit_alt() +
                                                          R"()(?![A-Za-zА-Яа-яЄєІіЇїҐґ]))");
    text = regex_sub(
        text, explicit_separator_unit_range, [&](const std::smatch& m) { return say_measurement(m, 2, 3, 4, true); });
    static const std::regex unit_range(prefix + "(" + number + ")\\s*" + separator + "\\s*(" + number + ")\\s*(" +
                                       unit_alt() + R"()(?![A-Za-zА-Яа-яЄєІіЇїҐґ]))");
    text = regex_sub(text, unit_range, [&](const std::smatch& m) { return say_measurement(m, 2, 3, 4, false); });

    auto say_currency = [&](const std::smatch& m,
                            std::size_t low_index,
                            std::size_t high_index,
                            std::size_t currency_index,
                            bool explicitly_from_to) {
        const auto currency = range_currency(m[currency_index].str());
        if (!currency) {
            return m.str();
        }
        const auto grammatical_case = governed_case(m, explicitly_from_to);
        const auto low = signed_number_words(m[low_index].str(), grammatical_case, currency->gender);
        const auto high = signed_number_words(m[high_index].str(), grammatical_case, currency->gender);
        if (!low || !high) {
            return m.str();
        }
        return m[1].str() + governed_connector(m, *low, *high, explicitly_from_to) + " " + std::string(currency->many);
    };
    static const std::regex repeated_prefix_currency(prefix + "(" + currency_token_alt() + ")\\s*(" + number + ")\\s*" +
                                                     separator + "\\s*(" + currency_token_alt() + ")\\s*(" + number +
                                                     ")" + number_boundary);
    text = regex_sub(text, repeated_prefix_currency, [&](const std::smatch& m) {
        const auto first = range_currency(m[2].str());
        const auto second = range_currency(m[4].str());
        return first && second && first->many == second->many ? say_currency(m, 3, 5, 4, false) : m.str();
    });
    static const std::regex prefix_currency_range(prefix + "(" + currency_token_alt() + ")\\s*(" + number + ")\\s*" +
                                                  separator + "\\s*(" + number + ")" + number_boundary);
    text =
        regex_sub(text, prefix_currency_range, [&](const std::smatch& m) { return say_currency(m, 3, 4, 2, false); });
    static const std::regex repeated_suffix_currency(prefix + "(" + number + ")\\s*(" + currency_token_alt() + ")\\s*" +
                                                     separator + "\\s*(" + number + ")\\s*(" + currency_token_alt() +
                                                     R"()(?![A-Za-zА-Яа-яЄєІіЇїҐґ]))");
    text = regex_sub(text, repeated_suffix_currency, [&](const std::smatch& m) {
        const auto first = range_currency(m[3].str());
        const auto second = range_currency(m[5].str());
        return first && second && first->many == second->many ? say_currency(m, 2, 4, 5, false) : m.str();
    });
    static const std::regex explicit_repeated_suffix_currency(
        prefix + "від\\s+(" + number + ")\\s*(" + currency_token_alt() + ")\\s+до\\s+(" + number + ")\\s*(" +
        currency_token_alt() + R"()(?![A-Za-zА-Яа-яЄєІіЇїҐґ]))");
    text = regex_sub(text, explicit_repeated_suffix_currency, [&](const std::smatch& m) {
        const auto first = range_currency(m[3].str());
        const auto second = range_currency(m[5].str());
        return first && second && first->many == second->many ? say_currency(m, 2, 4, 5, true) : m.str();
    });
    static const std::regex explicit_currency_range(prefix + "від\\s+(" + number + ")\\s+до\\s+(" + number + ")\\s*(" +
                                                    currency_token_alt() + R"()(?![A-Za-zА-Яа-яЄєІіЇїҐґ]))");
    text =
        regex_sub(text, explicit_currency_range, [&](const std::smatch& m) { return say_currency(m, 2, 3, 4, true); });
    static const std::regex suffix_currency_range(prefix + "(" + number + ")\\s*" + separator + "\\s*(" + number +
                                                  ")\\s*(" + currency_token_alt() + R"()(?![A-Za-zА-Яа-яЄєІіЇїҐґ]))");
    text =
        regex_sub(text, suffix_currency_range, [&](const std::smatch& m) { return say_currency(m, 2, 3, 4, false); });

    auto say_percent =
        [&](const std::smatch& m, std::size_t low_index, std::size_t high_index, bool explicitly_from_to) {
            const auto grammatical_case = governed_case(m, explicitly_from_to);
            const auto low = signed_number_words(m[low_index].str(), grammatical_case);
            const auto high = signed_number_words(m[high_index].str(), grammatical_case);
            if (!low || !high) {
                return m.str();
            }
            std::string unit = "відсотків";
            const auto context = governed_range(m, explicitly_from_to);
            if (context == GovernedRange::On || context == GovernedRange::In) {
                const auto high_token_storage = m[high_index].str();
                auto high_token = std::string_view(high_token_storage);
                take_spoken_sign(high_token);
                if (const auto value = try_parse_ull(high_token)) {
                    unit = plural(*value, {"відсоток", "відсотки", "відсотків"});
                }
            }
            return m[1].str() + governed_connector(m, *low, *high, explicitly_from_to) + " " + unit;
        };
    static const std::regex repeated_percent_range(prefix + "(" + number + ")\\s*%\\s*" + separator + "\\s*(" + number +
                                                   R"()\s*%(?!\w))");
    text = regex_sub(text, repeated_percent_range, [&](const std::smatch& m) { return say_percent(m, 2, 3, false); });
    static const std::regex explicit_repeated_percent_range(prefix + "від\\s+(" + number + ")\\s*%\\s+до\\s*(" +
                                                            number + R"()\s*%(?!\w))");
    text = regex_sub(
        text, explicit_repeated_percent_range, [&](const std::smatch& m) { return say_percent(m, 2, 3, true); });
    static const std::regex explicit_percent_range(prefix + "від\\s+(" + number + ")\\s+до\\s+(" + number +
                                                   R"()\s*%(?!\w))");
    text = regex_sub(text, explicit_percent_range, [&](const std::smatch& m) { return say_percent(m, 2, 3, true); });
    static const std::regex explicit_separator_percent_range(prefix + "від\\s+(" + number + ")\\s*" + separator +
                                                             "\\s*(" + number + R"()\s*%(?!\w))");
    text = regex_sub(
        text, explicit_separator_percent_range, [&](const std::smatch& m) { return say_percent(m, 2, 3, true); });
    static const std::regex percent_range(prefix + "(" + number + ")\\s*" + separator + "\\s*(" + number +
                                          R"()\s*%(?!\w))");
    text = regex_sub(text, percent_range, [&](const std::smatch& m) { return say_percent(m, 2, 3, false); });

    static const std::regex paragraph_range(prefix + R"((?:§§|§)\s*(\d+)\s*)" + separator + R"(\s*(\d+)(?!\d))");
    text = regex_sub(text, paragraph_range, [&](const std::smatch& m) {
        const auto low = parse_ull(m[2].str());
        const auto high = parse_ull(m[3].str());
        if (style == RangeStyle::FromTo) {
            return m[1].str() + "від " + number_to_ordinal_words(low, "gen") + " до " +
                   number_to_ordinal_words(high, "gen") + " параграфа";
        }
        return m[1].str() + "параграфи " + number_to_words(low) + " " + number_to_words(high);
    });

    static const std::regex school_grade_range(prefix + R"((\d{1,2})\s*)" + separator +
                                               R"(\s*(\d{1,2})\s+(класах|класів)(?![А-Яа-яЄєІіЇїҐґ]))");
    text = regex_sub(text, school_grade_range, [&](const std::smatch& m) {
        const auto word = preceding_word(m.prefix().str() + m[1].str());
        const auto noun = m[4].str();
        if (!((noun == "класах" && (word == "у" || word == "в")) || (noun == "класів" && word == "учнів"))) {
            return m.str();
        }
        const auto first = parse_ull(m[2].str());
        const auto second = parse_ull(m[3].str());
        if (!first || !second) {
            return m.str();
        }
        return m[1].str() + number_to_ordinal_words(first, "pl") + "–" + number_to_ordinal_words(second, "pl") + " " +
               noun;
    });

    auto say_bare = [&](const std::smatch& m, bool explicitly_from_to) {
        if (!explicitly_from_to && m[2].str().size() == 4 && m[3].str().size() <= 2) {
            const auto possible_year = try_parse_ull(m[2].str());
            if (possible_year && *possible_year >= 1000 && *possible_year <= 2999) {
                return m.str();
            }
        }
        if (governed_range(m, explicitly_from_to) == GovernedRange::In && m.suffix().str().starts_with(" класах")) {
            const auto low = try_parse_ull(m[2].str());
            const auto high = try_parse_ull(m[3].str());
            if (low && high) {
                return m[1].str() + number_to_words_case(*low, "prep") + "–" + number_to_words_case(*high, "prep");
            }
        }
        const auto grammatical_case = governed_case(m, explicitly_from_to);
        const auto gender =
            governed_range(m, explicitly_from_to) == GovernedRange::In && m.suffix().str().starts_with(" лінії") ? 'f'
                                                                                                                 : 'm';
        const auto low = signed_number_words(m[2].str(), grammatical_case, gender);
        const auto high = signed_number_words(m[3].str(), grammatical_case, gender);
        return low && high ? m[1].str() + governed_connector(m, *low, *high, explicitly_from_to) : m.str();
    };
    static const std::regex explicit_bare_range(prefix + "від\\s+(" + number + ")\\s+до\\s+(" + number + ")" +
                                                number_boundary + "(?!\\s+(?:" + month_alt() + R"()(?:\s|$)))");
    text = regex_sub(text, explicit_bare_range, [&](const std::smatch& m) { return say_bare(m, true); });
    static const std::regex explicit_separator_bare_range(prefix + "від\\s+(" + number + ")\\s*" + separator + "\\s*(" +
                                                          number + ")" + number_boundary);
    text = regex_sub(text, explicit_separator_bare_range, [&](const std::smatch& m) { return say_bare(m, true); });
    static const std::regex approximate_bare_range(prefix + "(Понад|понад)\\s+(" + number + ")\\s*" + separator +
                                                   "\\s*(" + number + ")" + number_boundary);
    text = regex_sub(text, approximate_bare_range, [](const std::smatch& m) {
        const auto low = signed_number_words(m[3].str(), "nom");
        const auto high = signed_number_words(m[4].str(), "nom");
        return low && high ? m[1].str() + m[2].str() + " " + *low + " чи " + *high : m.str();
    });
    static const std::regex bare_range(prefix + "(" + number + ")\\s*" + separator + "\\s*(" + number + ")" +
                                       number_boundary + "(?!\\s+(?:" + month_alt() + R"()(?:\s|$)))");
    return regex_sub(text, bare_range, [&](const std::smatch& m) { return say_bare(m, false); });
}

} // namespace uktextnorm::detail
