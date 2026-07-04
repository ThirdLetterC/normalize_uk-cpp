#include "uktextnorm/uktextnorm.hpp"

#include "internal.hpp"

namespace uktextnorm::detail {

std::string normalize_dates(std::string text, DateStyle style, bool validate)
{
    static const std::array<std::string_view, 12> months = {"січня",
                                                            "лютого",
                                                            "березня",
                                                            "квітня",
                                                            "травня",
                                                            "червня",
                                                            "липня",
                                                            "серпня",
                                                            "вересня",
                                                            "жовтня",
                                                            "листопада",
                                                            "грудня"};
    auto month_name = [](std::string token) {
        replace_all(token, ".", "");
        token = lower_text(token);
        static const std::unordered_map<std::string, std::string_view> names = {
            {"січ", "січня"},      {"січня", "січня"},         {"лют", "лютого"},  {"лютого", "лютого"},
            {"бер", "березня"},    {"березня", "березня"},     {"квіт", "квітня"}, {"квітня", "квітня"},
            {"трав", "травня"},    {"травня", "травня"},       {"черв", "червня"}, {"червня", "червня"},
            {"лип", "липня"},      {"липня", "липня"},         {"серп", "серпня"}, {"серпня", "серпня"},
            {"вер", "вересня"},    {"вересня", "вересня"},     {"жовт", "жовтня"}, {"жовтня", "жовтня"},
            {"лист", "листопада"}, {"листопада", "листопада"}, {"груд", "грудня"}, {"грудня", "грудня"}};
        if (const auto it = names.find(token); it != names.end()) {
            return std::string(it->second);
        }
        return token;
    };
    auto day_words = [style](std::string_view day, std::string_view formal_form = "nom_n") {
        return number_to_ordinal_words(parse_ull(day), style == DateStyle::Spoken ? "gen" : formal_form);
    };
    text = regex_sub(text, date_day_range_re(), [&](const std::smatch& m) {
        return number_to_ordinal_words(parse_ull(m[1].str()), "gen") + " " +
               number_to_ordinal_words(parse_ull(m[2].str()), "gen") + " " + month_name(m[3].str()) + " " +
               number_to_ordinal_words(parse_ull(m[4].str()), "gen") + " року";
    });
    static const std::regex numeric_range(
        R"(\b(\d{1,2})\.(\d{1,2})\.(\d{4})\s*[-–—]\s*(\d{1,2})\.(\d{1,2})\.(\d{4})\b)");
    text = regex_sub(text, numeric_range, [&](const std::smatch& m) {
        const int m1 = parse_int(m[2].str());
        const int m2 = parse_int(m[5].str());
        if (m1 < 1 || m1 > 12 || m2 < 1 || m2 > 12) {
            return m.str();
        }
        if (validate && (!is_valid_date(parse_int(m[1].str()), m1, parse_int(m[3].str())) ||
                         !is_valid_date(parse_int(m[4].str()), m2, parse_int(m[6].str())))) {
            return m.str();
        }
        return day_words(m[1].str()) + " " + std::string(months[m1 - 1]) + " " +
               number_to_ordinal_words(parse_ull(m[3].str()), "gen") + " року " + day_words(m[4].str()) + " " +
               std::string(months[m2 - 1]) + " " + number_to_ordinal_words(parse_ull(m[6].str()), "gen") + " року";
    });
    static const std::regex iso_range(R"(\b(\d{4})-(\d{2})-(\d{2})\s*[-–—]\s*(\d{4})-(\d{2})-(\d{2})\b)");
    text = regex_sub(text, iso_range, [&](const std::smatch& m) {
        const int m1 = parse_int(m[2].str());
        const int m2 = parse_int(m[5].str());
        if (m1 < 1 || m1 > 12 || m2 < 1 || m2 > 12) {
            return m.str();
        }
        if (validate && (!is_valid_date(parse_int(m[3].str()), m1, parse_int(m[1].str())) ||
                         !is_valid_date(parse_int(m[6].str()), m2, parse_int(m[4].str())))) {
            return m.str();
        }
        return day_words(m[3].str()) + " " + std::string(months[m1 - 1]) + " " +
               number_to_ordinal_words(parse_ull(m[1].str()), "gen") + " року " + day_words(m[6].str()) + " " +
               std::string(months[m2 - 1]) + " " + number_to_ordinal_words(parse_ull(m[4].str()), "gen") + " року";
    });
    text = ctre_sub<R"(\b(\d{1,2})\.(\d{1,2})\.(\d{4})\b)">(text, [&](const auto& m) {
        const int month = parse_int(cap<2>(m));
        if (month < 1 || month > 12) {
            return whole_string(m);
        }
        if (validate && !is_valid_date(parse_int(cap<1>(m)), month, parse_int(cap<3>(m)))) {
            return whole_string(m);
        }
        return day_words(cap<1>(m)) + " " + std::string(months[month - 1]) + " " +
               number_to_ordinal_words(parse_ull(cap<3>(m)), "gen") + " року";
    });
    text = ctre_sub<R"(\b(\d{1,2})/(\d{1,2})/(\d{4})\b)">(text, [&](const auto& m) {
        const int month = parse_int(cap<2>(m));
        if (month < 1 || month > 12) {
            return whole_string(m);
        }
        if (validate && !is_valid_date(parse_int(cap<1>(m)), month, parse_int(cap<3>(m)))) {
            return whole_string(m);
        }
        return day_words(cap<1>(m)) + " " + std::string(months[month - 1]) + " " +
               number_to_ordinal_words(parse_ull(cap<3>(m)), "gen") + " року";
    });
    text = ctre_sub<R"(\b(\d{4})-(\d{2})-(\d{2})\b)">(text, [&](const auto& m) {
        const int month = parse_int(cap<2>(m));
        if (month < 1 || month > 12) {
            return whole_string(m);
        }
        if (validate && !is_valid_date(parse_int(cap<3>(m)), month, parse_int(cap<1>(m)))) {
            return whole_string(m);
        }
        return day_words(cap<3>(m)) + " " + std::string(months[month - 1]) + " " +
               number_to_ordinal_words(parse_ull(cap<1>(m)), "gen") + " року";
    });
    text = regex_sub(text, date_spelled_re(), [](const std::smatch& m) {
        auto token = m[2].str();
        replace_all(token, ".", "");
        token = lower_text(token);
        static const std::unordered_map<std::string, std::string_view> names = {
            {"січ", "січня"},      {"січня", "січня"},         {"лют", "лютого"},  {"лютого", "лютого"},
            {"бер", "березня"},    {"березня", "березня"},     {"квіт", "квітня"}, {"квітня", "квітня"},
            {"трав", "травня"},    {"травня", "травня"},       {"черв", "червня"}, {"червня", "червня"},
            {"лип", "липня"},      {"липня", "липня"},         {"серп", "серпня"}, {"серпня", "серпня"},
            {"вер", "вересня"},    {"вересня", "вересня"},     {"жовт", "жовтня"}, {"жовтня", "жовтня"},
            {"лист", "листопада"}, {"листопада", "листопада"}, {"груд", "грудня"}, {"грудня", "грудня"}};
        const auto it = names.find(token);
        const auto month = it == names.end() ? token : std::string(it->second);
        return number_to_ordinal_words(parse_ull(m[1].str()), "gen") + " " + month + " " +
               number_to_ordinal_words(parse_ull(m[3].str()), "gen") + " року";
    });
    text = ctre_sub<R"((^|[^\d])(\d{3,4})\s+(році|року|рік)(?![А-Яа-яЄєІіЇїҐґ]))">(text, [](const auto& m) {
        static const std::unordered_map<std::string, std::string_view> forms = {
            {"рік", "nom_m"}, {"року", "gen"}, {"році", "prep"}};
        const auto form = cap_string<3>(m);
        return cap_string<1>(m) + number_to_ordinal_words(parse_ull(cap<2>(m)), forms.at(form)) + " " + form;
    });
    static const std::regex ordinal_year_suffix(R"((^|[^\d])(\d{3,4})[-–—](го|му|й|м)(?![А-Яа-яЄєІіЇїҐґ]))");
    text = regex_sub(text, ordinal_year_suffix, [](const std::smatch& m) {
        static const std::unordered_map<std::string, std::string_view> forms = {
            {"го", "gen"}, {"му", "dat"}, {"й", "nom_m"}, {"м", "prep"}};
        return m[1].str() + number_to_ordinal_words(parse_ull(m[2].str()), forms.at(m[3].str()));
    });
    return ctre_sub<R"(\b(\d{3,4})\s*р\.(?![а-яіїєґ]))">(
        text, [](const auto& m) { return number_to_ordinal_words(parse_ull(cap<1>(m)), "nom_m") + " рік"; });
}

std::string normalize_discourse_dates(std::string text)
{
    static const std::regex season_year(
        R"((^|[^А-Яа-яЄєІіЇїҐґ])((?:весна|літо|осінь|зима))\s+(\d{3,4})(?![\dА-Яа-яЄєІіЇїҐґ]))",
        std::regex::icase);
    text = regex_sub(text, season_year, [](const std::smatch& m) {
        return m[1].str() + m[2].str() + " " + number_to_ordinal_words(parse_ull(m[3].str()), "gen") + " року";
    });

    static const std::regex early_decade(
        R"((^|[^А-Яа-яЄєІіЇїҐґ])((?:на\s+початку|у\s+середині|в\s+середині|наприкінці|у\s+кінці|в\s+кінці))\s+(\d{4})-х(?![А-Яа-яЄєІіЇїҐґ]))",
        std::regex::icase);
    return regex_sub(text, early_decade, [](const std::smatch& m) {
        const auto year = parse_ull(m[3].str());
        if (year == 2000) {
            return m[1].str() + m[2].str() + " двотисячних";
        }
        return m[1].str() + m[2].str() + " " + number_to_ordinal_words(year, "pl");
    });
}
std::string normalize_ordinals(std::string text)
{
    static const std::unordered_map<std::string, std::string_view> suffix_form = {{"й", "nom_m"},
                                                                                  {"го", "gen"},
                                                                                  {"му", "dat"},
                                                                                  {"м", "prep"},
                                                                                  {"а", "nom_f"},
                                                                                  {"у", "acc_f"},
                                                                                  {"е", "nom_n"},
                                                                                  {"х", "pl"},
                                                                                  {"им", "ins"},
                                                                                  {"ім", "ins"},
                                                                                  {"ою", "ins_f"},
                                                                                  {"ій", "loc_f"},
                                                                                  {"ими", "ins_pl"}};
    static const std::unordered_set<std::string> stop = {
        "CD", "DVD", "MD", "DC", "MC", "MI", "MM", "DI", "DIV", "MIX", "CIV", "LCD"};
    text = ctre_sub<R"((\d+)(?:-|–|—)(ими|им|ім|ою|ій|го|му|й|м|а|у|е|х)(?![А-Яа-яЄєІіЇїҐґ]))">(text, [&](const auto& m) {
        return number_to_ordinal_words(parse_ull(cap<1>(m)), suffix_form.at(cap_string<2>(m)));
    });
    text = ctre_sub<
        R"((^|[^A-Za-z])([MDCLXVI]{1,6})\s*(?:-|–|—)\s*([MDCLXVI]{1,6})\s*(?:ст\.|століття)(?![А-Яа-яЄєІіЇїҐґ]))">(
        text, [](const auto& m) {
            const auto start = cap_string<2>(m);
            const auto stop = cap_string<3>(m);
            if (!valid_roman(start) || !valid_roman(stop)) {
                return whole_string(m);
            }
            return cap_string<1>(m) + number_to_ordinal_words(roman_to_int(start), "nom_n") + " " +
                   number_to_ordinal_words(roman_to_int(stop), "nom_n") + " століття";
        });
    text = ctre_sub<
        R"((^|[^A-Za-z])([MDCLXVI]{1,6})\s*(?:-|–|—)\s*([MDCLXVI]{1,6})\s*(розд\.|розділ)(?![А-Яа-яЄєІіЇїҐґ]))">(
        text, [](const auto& m) {
            const auto start = cap_string<2>(m);
            const auto stop = cap_string<3>(m);
            if (!valid_roman(start) || !valid_roman(stop)) {
                return whole_string(m);
            }
            return cap_string<1>(m) + number_to_ordinal_words(roman_to_int(start), "nom_m") + " " +
                   number_to_ordinal_words(roman_to_int(stop), "nom_m") + " розділ";
        });
    text =
        ctre_sub<R"((^|[^A-Za-z])([MDCLXVI]{1,6})\s*(?:ст\.|століття)(?![А-Яа-яЄєІіЇїҐґ]))">(text, [](const auto& m) {
            const auto tok = cap_string<2>(m);
            if (!valid_roman(tok)) {
                return whole_string(m);
            }
            return cap_string<1>(m) + number_to_ordinal_words(roman_to_int(tok), "nom_n") + " століття";
        });
    return ctre_sub<R"(\b[MDCLXVI]{2,}\b)">(text, [&](const auto& m) {
        const auto tok = whole_string(m);
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

std::string normalize_ranges(std::string text, RangeStyle style)
{
    text = ctre_sub<R"(\b(\d{3,4})\s*(?:-|–|—)\s*(\d{3,4})\s*(?:рр\.?|роки)(?![а-яіїєґ]))">(text, [&](const auto& m) {
        if (style == RangeStyle::FromTo) {
            return "від " + number_to_ordinal_words(parse_ull(cap<1>(m)), "gen") + " до " +
                   number_to_ordinal_words(parse_ull(cap<2>(m)), "gen") + " років";
        }
        return number_to_ordinal_words(parse_ull(cap<1>(m)), "nom_m") + " " +
               number_to_ordinal_words(parse_ull(cap<2>(m)), "nom_m") + " роки";
    });
    text = regex_sub(text, range_units_re(), [&](const std::smatch& m) {
        const auto& meas = measurements().at(m[4].str());
        if (style == RangeStyle::FromTo) {
            return m[1].str() + "від " + number_to_words_case(parse_ull(m[2].str()), "gen") + " до " +
                   number_to_words_case(parse_ull(m[3].str()), "gen") + " " + std::string(meas.many);
        }
        return m[1].str() + number_to_words(parse_ull(m[2].str())) + " " + number_to_words(parse_ull(m[3].str())) +
               " " + plural(parse_ull(m[3].str()), {meas.one, meas.few, meas.many});
    });
    text = ctre_sub<R"(\b(\d+)\s*(?:-|–|—)\s*(\d+)\s*%)">(text, [&](const auto& m) {
        const auto hi = parse_ull(cap<2>(m));
        if (style == RangeStyle::FromTo) {
            return "від " + number_to_words_case(parse_ull(cap<1>(m)), "gen") + " до " +
                   number_to_words_case(hi, "gen") + " відсотків";
        }
        return number_to_words(parse_ull(cap<1>(m))) + " " + number_to_words(hi) + " " +
               plural(hi, {"відсоток", "відсотки", "відсотків"});
    });
    text = ctre_sub<R"(\b(?:с\.|стор\.)\s*(\d+)\s*(?:-|–|—)\s*(\d+)\b)">(text, [](const auto& m) {
        return "сторінки " + number_to_words(parse_ull(cap<1>(m))) + " " + number_to_words(parse_ull(cap<2>(m)));
    });
    return ctre_sub<R"((\d)\s*[–—]\s*(?=\d))">(text, [](const auto& m) { return cap_string<1>(m) + " "; });
}
std::string normalize_case_context(std::string text)
{
    static const std::unordered_map<std::string, std::string_view> prep_case = {{"близько", "gen"},
                                                                                {"понад", "gen"},
                                                                                {"менше", "gen"},
                                                                                {"більше", "gen"},
                                                                                {"від", "gen"},
                                                                                {"до", "gen"},
                                                                                {"із", "gen"},
                                                                                {"з", "instr"},
                                                                                {"без", "gen"},
                                                                                {"після", "gen"},
                                                                                {"перед", "instr"},
                                                                                {"між", "instr"},
                                                                                {"над", "instr"},
                                                                                {"під", "instr"},
                                                                                {"при", "prep"},
                                                                                {"к", "dat"},
                                                                                {"о", "prep"},
                                                                                {"об", "prep"},
                                                                                {"у", "prep"},
                                                                                {"в", "prep"},
                                                                                {"на", "prep"}};
    static const std::regex instr(R"((^|[^А-Яа-яЄєІіЇїҐґ])([Зз])\s+(\d+)\s+([а-яєіїґ']{3,}(?:ами|ями|ма))\b)");
    static const std::regex oblique(R"(\b(\d+)\s+([а-яєіїґ']{3,}(?:ами|ями|ах|ях))\b)");
    text = regex_sub(text, instr, [](const std::smatch& m) {
        return m[1].str() + m[2].str() + " " + number_to_words_case(parse_ull(m[3].str()), "instr") + " " + m[4].str();
    });
    text = regex_sub(text, case_prep_re(), [&](const std::smatch& m) {
        const auto p = lower_text(m[2].str());
        const auto c = prep_case.at(p);
        if (m[4].matched && c != "gen") {
            return m.str();
        }
        std::string out = m[1].str() + m[2].str() + " " + number_to_words_case(parse_ull(m[3].str()), c);
        if (m[4].matched) {
            out += " " + std::string(measurements().at(m[4].str()).many);
        }
        return out;
    });
    return regex_sub(text, oblique, [](const std::smatch& m) {
        const auto noun = m[2].str();
        const auto it = counted_oblique_cases().find(lower_text(noun));
        if (it == counted_oblique_cases().end()) {
            return m.str();
        }
        return number_to_words_case(parse_ull(m[1].str()), it->second) + " " + noun;
    });
}
std::string normalize_counted_noun_context(std::string text)
{
    text = regex_sub(text, counted_ponad_re(), [](const std::smatch& m) {
        const auto n = parse_ull(m[3].str());
        const auto& noun = counted_nouns().at(lower_text(m[4].str()));
        return m[1].str() + m[2].str() + " " + number_words_for_gender(n, noun.gender) + " " +
               plural(n, {noun.one, noun.few, noun.many});
    });
    return regex_sub(text, counted_genitive_re(), [](const std::smatch& m) {
        const auto n = parse_ull(m[3].str());
        const auto& noun = counted_nouns().at(lower_text(m[4].str()));
        if (!prefers_many_after_genitive_number(n)) {
            return m.str();
        }
        return m[1].str() + m[2].str() + " " + number_to_words_case(n, "gen") + " " + std::string(noun.many);
    });
}

std::string normalize_counted_nouns(std::string text)
{
    return regex_sub(text, counted_nouns_re(), [](const std::smatch& m) {
        const auto n = parse_ull(m[2].str());
        const auto key = lower_text(m[3].str());
        const auto& noun = counted_nouns().at(key);
        return m[1].str() + number_words_for_gender(n, noun.gender) + " " + plural(n, {noun.one, noun.few, noun.many});
    });
}

std::string normalize_ordinal_triggers(std::string text)
{
    static const std::unordered_map<std::string, std::string_view> triggers = {{"місце", "nom_n"},
                                                                               {"село", "nom_n"},
                                                                               {"місто", "nom_n"},
                                                                               {"століття", "nom_n"},
                                                                               {"ст", "nom_n"},
                                                                               {"клас", "nom_m"},
                                                                               {"курс", "nom_m"},
                                                                               {"раунд", "nom_m"},
                                                                               {"сезон", "nom_m"},
                                                                               {"етап", "nom_m"},
                                                                               {"тур", "nom_m"},
                                                                               {"том", "nom_m"},
                                                                               {"під'їзд", "nom_m"},
                                                                               {"поверх", "nom_m"},
                                                                               {"група", "nom_f"},
                                                                               {"квартира", "nom_f"},
                                                                               {"сторінка", "nom_f"}};
    static const std::regex re(
        R"((^|[^\d])(\d{1,4})\s+(місце|село|місто|століття|ст|клас|курс|раунд|сезон|етап|тур|том|під'їзд|поверх|група|квартира|сторінка)(?![А-Яа-яЄєІіЇїҐґ]))",
        std::regex::icase);
    return regex_sub(text, re, [&](const std::smatch& m) {
        const auto noun = lower_text(m[3].str());
        return m[1].str() + number_to_ordinal_words(parse_ull(m[2].str()), triggers.at(noun)) + " " + m[3].str();
    });
}
std::string normalize_compounds(std::string text)
{
    static const std::regex re(
        R"((^|[^\d])(\d+)-(?!(?:ими|им|ім|ою|ій|го|му|й|м|а|у|е|х)(?:[^А-Яа-яЄєІіЇїҐґ]|$))([^0-9A-Za-z\s,.;:!?()]+))");
    return regex_sub(text, re, [](const std::smatch& m) {
        std::string prefix;
        for (const auto& w : split_words(number_to_words(parse_ull(m[2].str())))) {
            if (const auto it = compound_prefix_forms().find(w); it != compound_prefix_forms().end()) {
                prefix += it->second;
            } else if (const auto cf = case_forms().find(w); cf != case_forms().end()) {
                prefix += cf->second[0];
            } else {
                prefix += w;
            }
        }
        return m[1].str() + prefix + m[3].str();
    });
}
std::string normalize_time(std::string text)
{
    text = ctre_sub<R"((^|[^\d:])(\d{1,2}):([0-5]\d):([0-5]\d)(?![\d:]))">(text, [](const auto& m) {
        return cap_string<1>(m) + hours_words(parse_int(cap<2>(m))) + " " + minutes_words(parse_int(cap<3>(m))) + " " +
               minutes_words(parse_int(cap<4>(m)), {"секунда", "секунди", "секунд"});
    });
    text = ctre_sub<R"((^|[^А-Яа-яЄєІіЇїҐґ\d])((?:О|о)(?:б)?) (\d{1,2})(?:-|–|—)?(?:й|ій|а|ої)(?![А-Яа-яЄєІіЇїҐґ]))">(
        text, [](const auto& m) {
            return cap_string<1>(m) + cap_string<2>(m) + " " +
                   number_to_ordinal_words(parse_ull(cap<3>(m)), "nom_f") + " година";
        });
    text = ctre_sub<R"((^|[^\d:])(\d{1,2}):([0-5]\d)\s+(ранку|дня|вечора|ночі)(?![А-Яа-яЄєІіЇїҐґ\d:]))">(
        text, [](const auto& m) {
            const int hour = parse_int(cap<2>(m));
            const int minute = parse_int(cap<3>(m));
            std::string out = cap_string<1>(m) + hours_words(hour);
            if (minute) {
                out += " " + minutes_words(minute);
            }
            return out + " " + cap_string<4>(m);
        });
    return ctre_sub<R"((^|[^\d:])(\d{1,2}):([0-5]\d)(?![\d:]))">(text, [](const auto& m) {
        const int hour = parse_int(cap<2>(m));
        const int minute = parse_int(cap<3>(m));
        std::string out = cap_string<1>(m) + hours_words(hour);
        if (minute) {
            out += " " + minutes_words(minute);
        }
        return out;
    });
}
std::string normalize_fractions(std::string text)
{
    static const std::unordered_map<std::string, std::pair<int, int>> vulgar = {{"½", {1, 2}},
                                                                                {"⅓", {1, 3}},
                                                                                {"⅔", {2, 3}},
                                                                                {"¼", {1, 4}},
                                                                                {"¾", {3, 4}},
                                                                                {"⅕", {1, 5}},
                                                                                {"⅖", {2, 5}},
                                                                                {"⅗", {3, 5}},
                                                                                {"⅘", {4, 5}},
                                                                                {"⅙", {1, 6}},
                                                                                {"⅚", {5, 6}},
                                                                                {"⅐", {1, 7}},
                                                                                {"⅛", {1, 8}},
                                                                                {"⅜", {3, 8}},
                                                                                {"⅝", {5, 8}},
                                                                                {"⅞", {7, 8}},
                                                                                {"⅑", {1, 9}},
                                                                                {"⅒", {1, 10}}};
    for (const auto& [sym, nd] : vulgar) {
        replace_all(text, sym, say_fraction(nd.first, nd.second));
    }
    text = ctre_sub<R"((^|[^\d.,/])(\d+) (\d+)/(\d+)\b)">(text, [](const auto& m) {
        const auto whole = try_parse_ull(cap<2>(m));
        const auto numerator = try_parse_ull(cap<3>(m));
        const auto denominator = try_parse_ull(cap<4>(m));
        if (!whole || !numerator || !denominator || !*denominator) {
            return whole_string(m);
        }
        return cap_string<1>(m) + number_words_for_gender(*whole, 'f') + " і " +
               say_fraction(*numerator, *denominator);
    });
    return ctre_sub<R"(\b(\d+)/(\d+)\b)">(text, [](const auto& m) {
        const auto numerator = try_parse_ull(cap<1>(m));
        const auto denominator = try_parse_ull(cap<2>(m));
        if (!numerator || !denominator) {
            return whole_string(m);
        }
        return say_fraction(*numerator, *denominator);
    });
}

std::string normalize_percent(std::string text)
{
    return ctre_sub<R"((\d+(?:[.,]\d+)?)\s*%)">(text, [](const auto& m) {
        auto num = cap_string<1>(m);
        const auto pos = num.find_first_of(".,");
        if (pos != std::string::npos) {
            auto words = decimal_to_words(std::string_view(num).substr(0, pos), std::string_view(num).substr(pos + 1));
            return words.empty() ? whole_string(m) : words + " відсотка";
        }
        const auto n = try_parse_ull(num);
        if (!n) {
            return number_to_words_digit_by_digit(num) + " відсотків";
        }
        return number_to_words(*n) + " " + plural(*n, {"відсоток", "відсотки", "відсотків"});
    });
}
std::string normalize_measurements(std::string text)
{
    return regex_sub(text, measurements_re(), [](const std::smatch& m) {
        const auto& meas = measurements().at(m[3].str());
        return m[1].str() + read_measurement_quantity(m[2].str(), meas);
    });
}

std::string normalize_medical(std::string text)
{
    static const std::regex concentration(
        R"((^|[^\d.,])(\d+(?:[.,]\d+)?)\s*(мг|мл|г)\s*/\s*(мл|л)(?![A-Za-zА-Яа-яЄєІіЇїҐґ]))", std::regex::icase);
    static const std::regex pressure(R"((^|[^\d.,])(\d{2,3})\s*/\s*(\d{2,3})\s*мм\s*рт\.?\s*ст\.?)", std::regex::icase);
    static const std::regex labelled_pressure(
        R"((^|[^А-Яа-яЄєІіЇїҐґ\d])(тиск\s+)(\d{2,3})\s*/\s*(\d{2,3})\s*мм\s*рт\.?\s*ст\.?)", std::regex::icase);
    static const std::regex frequency(
        R"((^|[^А-Яа-яЄєІіЇїҐґ\d])(\d+)\s*(?:р\.|раз(?:и|ів)?)(\s+на\s+(?:день|добу|тиждень|місяць))(?![А-Яа-яЄєІіЇїҐґ]))",
        std::regex::icase);
    text = regex_sub(text, concentration, [](const std::smatch& m) {
        const auto from = lower_text(m[3].str());
        const auto to = lower_text(m[4].str());
        return m[1].str() + read_measurement_quantity(m[2].str(), measurements().at(from)) + " на " +
               std::string(measurements().at(to).one);
    });
    text = regex_sub(text, labelled_pressure, [](const std::smatch& m) {
        return m[1].str() + m[2].str() + number_to_words(parse_ull(m[3].str())) + " на " +
               number_to_words(parse_ull(m[4].str())) + " міліметрів ртутного стовпа";
    });
    text = regex_sub(text, pressure, [](const std::smatch& m) {
        return m[1].str() + number_to_words(parse_ull(m[2].str())) + " на " + number_to_words(parse_ull(m[3].str())) +
               " міліметрів ртутного стовпа";
    });
    text = ctre_sub<R"((^|[^\d.,])(\d+(?:[.,]\d+)?)\s*°\s*([CСF])\b)">(text, [](const auto& m) {
        const auto num = cap_string<2>(m);
        std::string words;
        if (const auto pos = num.find_first_of(".,"); pos != std::string::npos) {
            words = decimal_to_words(std::string_view(num).substr(0, pos), std::string_view(num).substr(pos + 1)) +
                    " градуса";
        } else {
            const auto n = parse_ull(num);
            words = number_to_words(n) + " " + plural(n, {"градус", "градуси", "градусів"});
        }
        const auto scale = cap_string<3>(m);
        return cap_string<1>(m) + words + (scale == "F" ? " фаренгейта" : " цельсія");
    });
    text = regex_sub(text, frequency, [](const std::smatch& m) {
        const auto n = parse_ull(m[2].str());
        return m[1].str() + number_to_words(n) + " " + plural(n, {"раз", "рази", "разів"}) + m[3].str();
    });
    return ctre_sub<R"((^|[^А-Яа-яЄєІіЇїҐґ\d])№\s*(\d{1,4})(?![\d/]))">(
        text, [](const auto& m) { return cap_string<1>(m) + "номер " + number_to_words(parse_ull(cap<2>(m))); });
}
std::string normalize_math(std::string text)
{
    return ctre_sub<R"((\d)\s*\+\s*(?=\d))">(text, [](const auto& m) { return cap_string<1>(m) + " плюс "; });
}

std::string normalize_decimals(std::string text)
{
    return ctre_sub<R"(\b(\d+),(\d+)\b)">(text, [](const auto& m) {
        const auto integer_part = cap_string<1>(m);
        const auto fractional_part = cap_string<2>(m);
        if (fractional_part.find_first_not_of('0') == std::string::npos) {
            return number_digits_or_words(integer_part);
        }
        auto words = decimal_to_words(integer_part, fractional_part);
        return words.empty()
                   ? number_digits_or_words(integer_part) + " кома " + number_to_words_digit_by_digit(fractional_part)
                   : words;
    });
}

std::string normalize_overprecise_currency_decimals(std::string text)
{
    static const std::regex re(R"(\b(\d+),(\d{3,})(?=\s*(?:грн|UAH|USD|EUR|GBP|[$€£₴]|долар|євро|фунт)))",
                               std::regex::icase);
    return regex_sub(text, re, [](const std::smatch& m) {
        return number_digits_or_words(m[1].str()) + " кома " + number_to_words_digit_by_digit(m[2].str());
    });
}
std::string normalize_multipliers(std::string text)
{
    static const std::unordered_map<std::string, std::pair<Forms, bool>> mult = {
        {"тис", {{"тисяча", "тисячі", "тисяч"}, true}},
        {"млн", {{"мільйон", "мільйони", "мільйонів"}, false}},
        {"млрд", {{"мільярд", "мільярди", "мільярдів"}, false}},
        {"трлн", {{"трильйон", "трильйони", "трильйонів"}, false}}};
    static const std::regex re(R"(\b(\d+(?:[.,]\d+)?)\s*(тис|млн|млрд|трлн)\.?(?![а-яіїєґ]))", std::regex::icase);
    return regex_sub(text, re, [&](const std::smatch& m) {
        const auto key = lower_text(m[2].str());
        const auto [forms, feminine] = mult.at(key);
        const auto num = m[1].str();
        const auto pos = num.find_first_of(".,");
        if (pos != std::string::npos) {
            auto words = decimal_to_words(std::string_view(num).substr(0, pos), std::string_view(num).substr(pos + 1));
            return words.empty() ? m.str() : words + " " + std::string(forms[1]);
        }
        const auto n = try_parse_ull(num);
        if (!n) {
            return number_to_words_digit_by_digit(num) + " " + std::string(forms[2]);
        }
        auto words = split_words(number_to_words(*n));
        if (feminine) {
            feminine_last(words);
        }
        return join(words) + " " + plural(*n, forms);
    });
}
std::string normalize_versions(std::string text)
{
    text = ctre_sub<R"(\b([A-Za-z][A-Za-z0-9_\-]*\s+)(\d+(?:\.\d+)+)\b)">(
        text, [](const auto& m) { return cap_string<1>(m) + read_dotted(cap<2>(m)); });
    return ctre_sub<R"(\b\d+(?:\.\d+){2,}\b)">(text, [](const auto& m) { return read_dotted(cap<0>(m)); });
}

std::string normalize_negatives(std::string text)
{
    return ctre_sub<R"((^|[\s(\[])[\-−](\d))">(
        text, [](const auto& m) { return cap_string<1>(m) + "мінус " + cap_string<2>(m); });
}

std::string normalize_text_with_numbers(std::string text)
{
    return ctre_sub<R"(\b\d+\b)">(text, [](const auto& m) {
        const auto digits = cap_string<0>(m);
        if (digits.size() > 1 && digits[0] == '0') {
            return number_to_words_digit_by_digit(digits);
        }
        return number_digits_or_words(digits);
    });
}

} // namespace uktextnorm::detail
