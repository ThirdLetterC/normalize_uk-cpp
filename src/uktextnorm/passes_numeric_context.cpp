#include "uktextnorm/uktextnorm.hpp"

#include "generated/uktextnorm_lexicons.hpp"
#include "numeric_internal.hpp"

namespace uktextnorm::detail {

std::string normalize_case_context(std::string text)
{
    static const std::unordered_map<std::string, std::string_view> prep_case = {{"близько", "gen"},
                                                                                {"менше", "gen"},
                                                                                {"більше", "gen"},
                                                                                {"серед", "gen"},
                                                                                {"від", "gen"},
                                                                                {"до", "gen"},
                                                                                {"із", "gen"},
                                                                                {"з", "instr"},
                                                                                {"без", "gen"},
                                                                                {"після", "gen"},
                                                                                {"протягом", "gen"},
                                                                                {"впродовж", "gen"},
                                                                                {"упродовж", "gen"},
                                                                                {"перед", "instr"},
                                                                                {"між", "instr"},
                                                                                {"над", "instr"},
                                                                                {"під", "instr"},
                                                                                {"при", "prep"},
                                                                                {"к", "dat"},
                                                                                {"о", "prep"},
                                                                                {"об", "prep"}};
    static const std::regex quantified_genitive(R"((^|[\s(\[{:;,.!?])((?:З|з))\s+(\d+)\s+([^\s,.;:!?]+))");
    static const std::regex comparative_genitive(
        R"((^|[^А-Яа-яЄєІіЇїҐґ])(Після|після|До|до|Від|від|Без|без)\s+(більш|менш)\s+ніж\s+(\d+)(?!\d))");
    static const std::regex instr(
        R"((^|[^А-Яа-яЄєІіЇїҐґ])([Зз])\s+(\d+)\s+([а-яєіїґ']{3,}(?:ами|ями|ма))(?![А-Яа-яЄєІіЇїҐґ]))");
    static const std::string oblique_nouns = [] {
        std::vector<std::string> keys;
        keys.reserve(counted_oblique_cases().size());
        for (const auto& [key, _] : counted_oblique_cases()) {
            keys.push_back(key);
        }
        return regex_alternation(std::move(keys));
    }();
    static const std::regex oblique("(^|[^А-Яа-яЄєІіЇїҐґ])(У|у|В|в|На|на)\\s+(\\d+)\\s+(" + oblique_nouns +
                                        R"()(?![А-Яа-яЄєІіЇїҐґ]))",
                                    std::regex::icase);
    static const std::regex before_locative_adjective(R"((^|[\s(\[{:;,.!?])(У|у|В|в|На|на)\s+(\d+)\s+([^\s,.;:!?]+))");
    static const std::regex ponad_quantity(R"((^|[^А-Яа-яЄєІіЇїҐґ])(Понад|понад)\s+(\d+)(?!\d))");
    text = regex_sub(text, ponad_quantity, [](const std::smatch& m) {
        auto words = number_to_words(parse_ull(m[3].str()));
        if (words.starts_with("тисяча ")) {
            words.replace(0, std::string("тисяча").size(), "тисячу");
        }
        return m[1].str() + m[2].str() + " " + words;
    });
    text = regex_sub(text, quantified_genitive, [](const std::smatch& m) {
        const auto noun = lower_text(m[4].str());
        if (!noun.ends_with("ів") && !noun.ends_with("їв")) {
            return m.str();
        }
        return m[1].str() + m[2].str() + " " + number_to_words_case(parse_ull(m[3].str()), "gen") + " " + m[4].str();
    });
    text = regex_sub(text, comparative_genitive, [](const std::smatch& m) {
        return m[1].str() + m[2].str() + " " + m[3].str() + " ніж " +
               number_to_words_case(parse_ull(m[4].str()), "gen");
    });
    text = regex_sub(text, instr, [](const std::smatch& m) {
        return m[1].str() + m[2].str() + " " + number_to_words_case(parse_ull(m[3].str()), "instr") + " " + m[4].str();
    });
    text = regex_sub(text, before_locative_adjective, [](const std::smatch& m) {
        const auto count = parse_ull(m[3].str());
        const auto adjective = lower_text(m[4].str());
        if (count == 1 || (!adjective.ends_with("ому") && !adjective.ends_with("ьому") && !adjective.ends_with("ій") &&
                           !adjective.ends_with("их"))) {
            return m.str();
        }
        return m[1].str() + m[2].str() + " " + number_to_words_case(count, "prep") + " " + m[4].str();
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
            out += m[5].str();
        }
        return out;
    });
    return regex_sub(text, oblique, [](const std::smatch& m) {
        const auto noun = m[4].str();
        const auto it = counted_oblique_cases().find(lower_text(noun));
        if (it == counted_oblique_cases().end()) {
            return m.str();
        }
        return m[1].str() + m[2].str() + " " + number_to_words_case(parse_ull(m[3].str()), it->second) + " " + noun;
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
        if (noun.one == "стаття") {
            const auto previous = preceding_word(m.prefix().str());
            if (previous == "частина" || previous == "пункт" || previous == "розділ" || previous == "параграф" ||
                previous == "глава") {
                return m.str();
            }
        }
        return m[1].str() + number_words_for_gender(n, noun.gender) + " " + plural(n, {noun.one, noun.few, noun.many});
    });
}

std::string normalize_ordinal_triggers(std::string text)
{
    static const std::regex genitive_class(R"((^|[^\dА-Яа-яЄєІіЇїҐґ])(\d{1,2})\s+(класу)(?![А-Яа-яЄєІіЇїҐґ]))",
                                           std::regex::icase);
    text = regex_sub(text, genitive_class, [](const std::smatch& m) {
        const auto number = parse_ull(m[2].str());
        return number == 0 ? m.str() : m[1].str() + number_to_ordinal_words(number, "gen") + " " + m[3].str();
    });
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
        R"((^|[^\d])(\d+)-(?!(?:ший|ими|им|ім|ою|ій|ше|ша|ге|га|тє|тя|го|му|й|м|а|у|е|х)(?:[^А-Яа-яЄєІіЇїҐґ]|$))([^0-9A-Za-z\s,.;:!?()]+))");
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

} // namespace uktextnorm::detail
