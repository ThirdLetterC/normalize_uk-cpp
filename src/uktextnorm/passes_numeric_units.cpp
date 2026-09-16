#include "uktextnorm/uktextnorm.hpp"

#include "generated/uktextnorm_lexicons.hpp"
#include "numeric_internal.hpp"

namespace uktextnorm::detail {

bool governed_by_o(const std::smatch& m)
{
    const auto word = preceding_word(m.prefix().str() + m[1].str());
    return word == "о" || word == "об";
}

std::string normalize_time(std::string text, ColonStyle colon_style)
{
    auto clock_words = [](int hour, int minute, std::optional<int> second = std::nullopt, bool locative = false) {
        if ((hour == 0 || hour == 24) && minute == 0 && (!second || *second == 0)) {
            return std::string("опівночі");
        }
        std::string out = locative ? number_to_ordinal_words(static_cast<unsigned long long>(hour), "loc_f") +
                                         " годині"
                                   : hours_words(hour);
        if (minute) {
            out += " " + minutes_words(minute);
        }
        if (second && *second) {
            out += " " + minutes_words(*second, {"секунда", "секунди", "секунд"});
        }
        return out;
    };
    static const std::regex variable_ratio(R"((^|[^A-Za-z\d:])(\d+)\s*:\s*([A-Za-z])(?![A-Za-z\d]))");
    text = regex_sub(text, variable_ratio, [](const std::smatch& m) {
        return m[1].str() + number_to_words(parse_ull(m[2].str())) + " до " + spell_identifier_letters(m[3].str());
    });
    static const std::regex am_pm(R"((^|[^\d:])(\d{1,2}):([0-5]\d)(?::([0-5]\d))?\s*(a\.?m\.?|p\.?m\.?)(?![A-Za-z]))",
                                  std::regex::icase);
    text = regex_sub(text, am_pm, [&](const std::smatch& m) {
        const auto hour = parse_int(m[2].str());
        if (hour < 1 || hour > 12) {
            return m.str();
        }
        const auto period = lower_text(m[5].str());
        const bool pm = period.starts_with("p");
        const auto clock_hour = hour == 12 ? (pm ? 12 : 0) : hour;
        const auto suffix = !pm && hour == 12                ? ""
                            : pm && (hour < 6 || hour == 12) ? " дня"
                            : pm                             ? " вечора"
                            : hour < 5                       ? " ночі"
                                                             : " ранку";
        return m[1].str() +
               clock_words(clock_hour,
                           parse_int(m[3].str()),
                           m[4].matched ? std::optional<int>(parse_int(m[4].str())) : std::nullopt,
                           governed_by_o(m)) +
               suffix;
    });
    static const std::regex zoned(
        R"((^|[^\d:])(\d{1,2}):([0-5]\d)(?::([0-5]\d))?\s*(UTC|GMT)(?:\s*([+-])\s*(\d{1,2})(?::?([0-5]\d))?)?(?![A-Za-z\d]))",
        std::regex::icase);
    text = regex_sub(text, zoned, [&](const std::smatch& m) {
        const auto hour = parse_int(m[2].str());
        const auto offset = m[7].matched ? parse_int(m[7].str()) : 0;
        const auto offset_minutes = m[8].matched ? parse_int(m[8].str()) : 0;
        if (hour > 23 || offset > 14 || (offset == 14 && offset_minutes != 0)) {
            return m.str();
        }
        std::string out = m[1].str() +
                          clock_words(hour,
                                      parse_int(m[3].str()),
                                      m[4].matched ? std::optional<int>(parse_int(m[4].str())) : std::nullopt) +
                          " за всесвітнім координованим часом";
        if (m[6].matched) {
            out += " " + std::string(m[6].str() == "+" ? "плюс " : "мінус ") + hours_words(offset);
            if (m[8].matched && parse_int(m[8].str())) {
                out += " " + minutes_words(parse_int(m[8].str()));
            }
        }
        return out;
    });
    static const std::regex offset_zoned(
        R"((^|[^\d:])(\d{1,2}):([0-5]\d)(?::([0-5]\d))?\s+([+-])(\d{2}):([0-5]\d)(?!\d))");
    text = regex_sub(text, offset_zoned, [&](const std::smatch& m) {
        const auto hour = parse_int(m[2].str());
        const auto offset = parse_int(m[6].str());
        const auto offset_minutes = parse_int(m[7].str());
        if (hour > 23 || offset > 14 || (offset == 14 && offset_minutes != 0)) {
            return m.str();
        }
        std::string out = m[1].str() +
                          clock_words(hour,
                                      parse_int(m[3].str()),
                                      m[4].matched ? std::optional<int>(parse_int(m[4].str())) : std::nullopt) +
                          " за часовим поясом " + (m[5].str() == "+" ? "плюс " : "мінус ") + hours_words(offset);
        if (offset_minutes) {
            out += " " + minutes_words(offset_minutes);
        }
        return out;
    });
    static const std::regex iana_zoned(
        R"((^|[^\d:])(\d{1,2}):([0-5]\d)(?::([0-5]\d))?\s+([A-Za-z_+-]+/[A-Za-z0-9_+/-]+)(?![A-Za-z0-9_+/-]))",
        std::regex::icase);
    text = regex_sub(text, iana_zoned, [&](const std::smatch& m) {
        static const std::unordered_map<std::string, std::string_view> zone_names = {
            {"europe/kyiv", "за київським часом"},
            {"europe/london", "за лондонським часом"},
            {"europe/warsaw", "за варшавським часом"},
            {"america/new_york", "за нью-йоркським часом"},
            {"america/los_angeles", "за лос-анджелеським часом"},
            {"asia/tokyo", "за токійським часом"}};
        const auto hour = parse_int(m[2].str());
        const auto zone = zone_names.find(lower_text(m[5].str()));
        if (hour > 23 || zone == zone_names.end()) {
            return m.str();
        }
        return m[1].str() +
               clock_words(hour,
                           parse_int(m[3].str()),
                           m[4].matched ? std::optional<int>(parse_int(m[4].str())) : std::nullopt) +
               " " + std::string(zone->second);
    });
    static const std::regex hms(R"((^|[^\d:])(\d{1,2}):([0-5]\d):([0-5]\d)(?![\d:]))");
    text = regex_sub(text, hms, [&](const std::smatch& m) {
        const auto hour = parse_int(m[2].str());
        if (hour > 23) {
            return m.str();
        }
        return m[1].str() +
               clock_words(hour, parse_int(m[3].str()), parse_int(m[4].str()), governed_by_o(m));
    });
    text = ctre_sub<R"((^|[^А-Яа-яЄєІіЇїҐґ\d])((?:О|о)(?:б)?) (\d{1,2})(?:-|–|—)?(?:й|ій|а|ої)(?![А-Яа-яЄєІіЇїҐґ]))">(
        text, [](const auto& m) {
            return cap_string<1>(m) + cap_string<2>(m) + " " + number_to_ordinal_words(parse_ull(cap<3>(m)), "nom_f") +
                   " година";
        });
    static const std::regex day_period(
        R"((^|[^\d:])(\d{1,2}):([0-5]\d)\s+(ранку|дня|вечора|ночі)(?![А-Яа-яЄєІіЇїҐґ\d:]))", std::regex::icase);
    text = regex_sub(text, day_period, [&](const std::smatch& m) {
        const auto hour = parse_int(m[2].str());
        return hour <= 23
                   ? m[1].str() + clock_words(hour, parse_int(m[3].str()), std::nullopt, governed_by_o(m)) + " " +
                         m[4].str()
                   : m.str();
    });
    static const std::regex hm(R"((^|[^\d:])(\d{1,2}):([0-5]\d)(?![\d:]))");
    text = regex_sub(text, hm, [&](const std::smatch& m) {
        if (colon_style == ColonStyle::Ratio) {
            return m.str();
        }
        const auto hour = parse_int(m[2].str());
        if (hour == 24 && parse_int(m[3].str()) == 0) {
            return m[1].str() + clock_words(hour, 0);
        }
        return hour <= 23 ? m[1].str() + clock_words(hour, parse_int(m[3].str()), std::nullopt, governed_by_o(m))
                          : m.str();
    });
    static const std::regex ratio(R"((^|[^\d:])(\d+):(\d+)(?![\d:]))");
    return regex_sub(text, ratio, [&](const std::smatch& m) {
        if (colon_style == ColonStyle::Clock) {
            const auto hour = try_parse_ull(m[2].str());
            const auto minute = try_parse_ull(m[3].str());
            if (hour && minute && *hour <= 23 && *minute <= 59) {
                return m[1].str() + clock_words(static_cast<int>(*hour), static_cast<int>(*minute));
            }
            return m.str();
        }
        const auto left = try_parse_ull(m[2].str());
        const auto right = try_parse_ull(m[3].str());
        return left && right ? m[1].str() + number_to_words(*left) + " до " + number_to_words_case(*right, "gen")
                             : m.str();
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
        const std::regex measured_mixed("(\\d+)\\s*" + sym + "\\s*(" + unit_alt() + R"()(?![A-Za-zА-Яа-яЄєІіЇїҐґ]))");
        text = regex_sub(text, measured_mixed, [&](const std::smatch& m) {
            return number_words_for_gender(parse_ull(m[1].str()), 'f') + " цілих і " +
                   say_fraction(nd.first, nd.second) + " " + std::string(measurements().at(m[2].str()).decimal);
        });
        const std::regex measured_vulgar("(^|[^\\d])" + sym + "\\s*(" + unit_alt() + R"()(?![A-Za-zА-Яа-яЄєІіЇїҐґ]))");
        text = regex_sub(text, measured_vulgar, [&](const std::smatch& m) {
            return m[1].str() + say_fraction(nd.first, nd.second) + " " +
                   std::string(measurements().at(m[2].str()).decimal);
        });
        const std::regex mixed("(\\d+)\\s*" + sym);
        text = regex_sub(text, mixed, [&](const std::smatch& m) {
            return number_words_for_gender(parse_ull(m[1].str()), 'f') + " цілих і " +
                   say_fraction(nd.first, nd.second);
        });
        replace_all(text, sym, " " + say_fraction(nd.first, nd.second));
    }
    static const std::regex measured_fraction("(^|[^\\d.,/])([+\\-]?)(\\d+)/(\\d+)\\s*(" + unit_alt() +
                                              R"()(?![A-Za-zА-Яа-яЄєІіЇїҐґ]))");
    text = regex_sub(text, measured_fraction, [](const std::smatch& m) {
        const auto numerator = try_parse_ull(m[3].str());
        const auto denominator = try_parse_ull(m[4].str());
        if (!numerator || !denominator || !*denominator) {
            return m.str();
        }
        const auto sign = m[2].str() == "-" ? "мінус " : m[2].str() == "+" ? "плюс " : "";
        return m[1].str() + sign + say_fraction(*numerator, *denominator) + " " +
               std::string(measurements().at(m[5].str()).decimal);
    });
    text = ctre_sub<R"((^|[^\d.,/])([+\-−]?)(\d+) (\d+)/(\d+)\b)">(text, [](const auto& m) {
        const auto whole = try_parse_ull(cap<3>(m));
        const auto numerator = try_parse_ull(cap<4>(m));
        const auto denominator = try_parse_ull(cap<5>(m));
        if (!whole || !numerator || !denominator || !*denominator) {
            return whole_string(m);
        }
        const auto sign = cap<2>(m) == "-" || cap<2>(m) == "−" ? "мінус " : cap<2>(m) == "+" ? "плюс " : "";
        return cap_string<1>(m) + sign + number_words_for_gender(*whole, 'f') + " і " +
               say_fraction(*numerator, *denominator);
    });
    return ctre_sub<R"((^|[^\d.,/])([+\-−]?)(\d+)/(\d+)\b)">(text, [](const auto& m) {
        const auto numerator = try_parse_ull(cap<3>(m));
        const auto denominator = try_parse_ull(cap<4>(m));
        if (!numerator || !denominator || !*denominator) {
            return whole_string(m);
        }
        const auto sign = cap<2>(m) == "-" || cap<2>(m) == "−" ? "мінус " : cap<2>(m) == "+" ? "плюс " : "";
        return cap_string<1>(m) + sign + say_fraction(*numerator, *denominator);
    });
}

std::string normalize_percent(std::string text)
{
    static const std::regex governed(
        R"((^|[^А-Яа-яЄєІіЇїҐґ-])(Близько|близько|Після|після|Менше|менше|Більше|більше|Без|без|Від|від|До|до|Із|із)\s+([+\-]?\d+(?:[.,]\d+)?)\s*%)");
    text = regex_sub(text, governed, [](const std::smatch& m) {
        auto token = m[3].str();
        const auto words = signed_number_words(token, "gen");
        if (!words) {
            return m.str();
        }
        const auto unit = token.find_first_of(".,") == std::string::npos ? "відсотків" : "відсотка";
        return m[1].str() + m[2].str() + " " + *words + " " + unit;
    });
    static const std::regex adjacent_signed(R"(([+\-])(\d+(?:[.,]\d+)?)\s*%)");
    text = regex_sub(text, adjacent_signed, [](const std::smatch& m) {
        const auto number = signed_number_words(m[1].str() + m[2].str(), "nom");
        if (!number) {
            return m.str();
        }
        const auto token = m[2].str();
        const auto unsigned_number = std::string_view(token);
        const auto decimal = unsigned_number.find_first_of(".,");
        if (decimal != std::string_view::npos) {
            return " " + *number + " відсотка";
        }
        const auto value = try_parse_ull(unsigned_number);
        return value ? " " + *number + " " + plural(*value, {"відсоток", "відсотки", "відсотків"}) : m.str();
    });
    static const std::regex percent(R"((^|[^\d.,+\-])([+\-]?\d+(?:[.,]\d+)?)\s*%)");
    return regex_sub(text, percent, [](const std::smatch& m) {
        auto num = m[2].str();
        auto unsigned_num = std::string_view(num);
        const auto words = signed_number_words(unsigned_num, "nom");
        if (!words) {
            const auto sign = take_spoken_sign(unsigned_num);
            if (unsigned_num.find_first_of(".,") == std::string_view::npos) {
                return m[1].str() + sign + number_to_words_digit_by_digit(unsigned_num) + " відсотків";
            }
            return m.str();
        }
        take_spoken_sign(unsigned_num);
        const auto pos = num.find_first_of(".,");
        if (pos != std::string::npos) {
            return m[1].str() + *words + " відсотка";
        }
        const auto n = try_parse_ull(unsigned_num);
        if (!n) {
            return m.str();
        }
        return m[1].str() + *words + " " + plural(*n, {"відсоток", "відсотки", "відсотків"});
    });
}
std::string normalize_measurements(std::string text)
{
    static const std::string atomic_unit_alt = [] {
        std::vector<std::string> keys;
        for (const auto& [key, _] : measurements()) {
            if (key.find('/') == std::string_view::npos && key.find("·") == std::string_view::npos &&
                key.find('-') == std::string_view::npos && key.find(' ') == std::string_view::npos) {
                keys.emplace_back(key);
            }
        }
        return regex_alternation(std::move(keys));
    }();
    static const std::string atom_pattern = "(?:" + atomic_unit_alt + ")(?:²|³|2|3)?";
    static const std::regex tolerance("(^|[^\\d.,])(" + signed_number_pattern() + ")\\s*±\\s*(" +
                                      signed_number_pattern() + ")\\s*(" + unit_alt() +
                                      R"()(?![A-Za-zА-Яа-яЄєІіЇїҐґ]))");
    text = regex_sub(text, tolerance, [](const std::smatch& m) {
        const auto& measurement = measurements().at(m[4].str());
        const auto base = signed_number_words(m[2].str(), "nom", measurement.gender);
        return base ? m[1].str() + *base + " плюс мінус " + read_measurement_quantity(m[3].str(), measurement)
                    : m.str();
    });
    static const std::regex parenthesized_denominator(
        "(^|[^\\d.,])(" + signed_number_pattern() + ")\\s*(" + atom_pattern + ")\\s*/\\s*\\(\\s*(" + atom_pattern +
        ")\\s*(?:·|\\*)\\s*(" + atom_pattern + R"()\s*\)(?![A-Za-zА-Яа-яЄєІіЇїҐґ]))");
    text = regex_sub(text, parenthesized_denominator, [](const std::smatch& m) {
        const auto first = measurements().find(m[3].str());
        const auto second = measurements().find(m[4].str());
        const auto third = measurements().find(m[5].str());
        if (first == measurements().end() || second == measurements().end() || third == measurements().end()) {
            return m.str();
        }
        return m[1].str() + read_measurement_quantity(m[2].str(), first->second) + " поділити на " +
               std::string(second->second.one) + " помножити на " + std::string(third->second.one);
    });
    static const std::regex formula("(^|[^\\d.,])([+\\-−]?\\d+(?:[.,]\\d+)?)\\s*(" + atom_pattern +
                                    "\\s*(?:·|\\*|/)\\s*" + atom_pattern + "(?:\\s*(?:·|\\*|/)\\s*" + atom_pattern +
                                    ")*)(?!\\s*/)(?![A-Za-zА-Яа-яЄєІіЇїҐґ])");
    static const std::regex atom(atom_pattern);
    auto factor_words = [](std::string factor, const std::string& quantity, bool first, bool denominator) {
        const Measurement* measurement = nullptr;
        std::string exponent;
        if (const auto it = measurements().find(factor); it != measurements().end()) {
            measurement = &it->second;
        } else {
            for (const auto& [suffix, words] : {std::pair{std::string_view("²"), std::string_view(" у квадраті")},
                                                std::pair{std::string_view("³"), std::string_view(" у кубі")},
                                                std::pair{std::string_view("2"), std::string_view(" у квадраті")},
                                                std::pair{std::string_view("3"), std::string_view(" у кубі")}}) {
                if (!factor.ends_with(suffix)) {
                    continue;
                }
                factor.erase(factor.size() - suffix.size());
                if (const auto it = measurements().find(factor); it != measurements().end()) {
                    measurement = &it->second;
                    exponent = words;
                }
                break;
            }
        }
        if (!measurement) {
            return std::optional<std::string>{};
        }
        if (first) {
            return std::optional<std::string>{read_measurement_quantity(quantity, *measurement) + exponent};
        }
        std::string unit(measurement->one);
        if (denominator) {
            static const std::unordered_map<std::string, std::string_view> accusative = {{"секунда", "секунду"},
                                                                                         {"хвилина", "хвилину"},
                                                                                         {"година", "годину"},
                                                                                         {"миля", "милю"},
                                                                                         {"тонна", "тонну"},
                                                                                         {"унція", "унцію"},
                                                                                         {"атмосфера", "атмосферу"}};
            if (const auto it = accusative.find(unit); it != accusative.end()) {
                unit = it->second;
            }
        }
        return std::optional<std::string>{unit + exponent};
    };
    text = regex_sub(text, formula, [&](const std::smatch& m) {
        const auto expression = m[3].str();
        if (const auto direct = measurements().find(expression); direct != measurements().end()) {
            return m[1].str() + read_measurement_quantity(m[2].str(), direct->second);
        }
        std::string out = m[1].str();
        std::size_t previous_end = 0;
        bool denominator = false;
        bool first = true;
        for (std::sregex_iterator it(expression.begin(), expression.end(), atom), end; it != end; ++it) {
            const auto position = static_cast<std::size_t>((*it).position());
            const auto separator = expression.substr(previous_end, position - previous_end);
            if (separator.find('/') != std::string::npos) {
                denominator = true;
            }
            const auto words = factor_words(it->str(), m[2].str(), first, denominator);
            if (!words) {
                return m.str();
            }
            if (!first) {
                out += denominator && separator.find('/') != std::string::npos ? " поділити на " : " помножити на ";
            }
            out += *words;
            first = false;
            previous_end = position + static_cast<std::size_t>((*it).length());
        }
        return first ? m.str() : out;
    });
    return regex_sub(text, measurements_re(), [](const std::smatch& m) {
        const auto& meas = measurements().at(m[3].str());
        return m[1].str() + read_measurement_quantity(m[2].str(), meas) + m[4].str();
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
    static const std::regex temperature_tolerance(range_prefix_pattern() + "(" + signed_number_pattern() +
                                                  ")\\s*±\\s*(" + signed_number_pattern() + ")\\s*(" +
                                                  temperature_unit_pattern() + R"()(?![A-Za-zА-Яа-яЄєІіЇїҐґ]))");
    text = regex_sub(text, temperature_tolerance, [](const std::smatch& m) {
        const auto scale = temperature_scale(m[4].str());
        const auto base = signed_number_words(m[2].str(), "nom");
        const auto tolerance = scale ? temperature_quantity_words(m[3].str(), *scale) : std::nullopt;
        return base && tolerance ? m[1].str() + *base + " плюс мінус " + *tolerance : m.str();
    });
    static const std::regex governed_temperature("(^|[^А-Яа-яЄєІіЇїҐґ-])(Близько|близько|Після|після|Менше|менше|"
                                                 "Більше|більше|Без|без|Від|від|До|до|Із|із)\\s+(" +
                                                 signed_number_pattern() + ")\\s*(" + temperature_unit_pattern() +
                                                 R"()(?![A-Za-zА-Яа-яЄєІіЇїҐґ]))");
    text = regex_sub(text, governed_temperature, [](const std::smatch& m) {
        const auto scale = temperature_scale(m[4].str());
        const auto words = scale ? temperature_quantity_words(m[3].str(), *scale, "gen") : std::nullopt;
        return words ? m[1].str() + m[2].str() + " " + *words : m.str();
    });
    static const std::regex temperature(range_prefix_pattern() + "(" + signed_number_pattern() + ")\\s*(" +
                                        temperature_unit_pattern() + R"()(?![A-Za-zА-Яа-яЄєІіЇїҐґ]))");
    text = regex_sub(text, temperature, [](const std::smatch& m) {
        const auto scale = temperature_scale(m[3].str());
        const auto words = scale ? temperature_quantity_words(m[2].str(), *scale) : std::nullopt;
        return words ? m[1].str() + *words : m.str();
    });
    text = regex_sub(text, frequency, [](const std::smatch& m) {
        const auto n = parse_ull(m[2].str());
        return m[1].str() + number_to_words(n) + " " + plural(n, {"раз", "рази", "разів"}) + m[3].str();
    });
    return ctre_sub<R"((^|[^А-Яа-яЄєІіЇїҐґ\d])№\s*(\d{1,4})(?![\d/]))">(
        text, [](const auto& m) { return cap_string<1>(m) + "номер " + number_to_words(parse_ull(cap<2>(m))); });
}
std::string normalize_scientific(std::string text, RangeStyle range_style)
{
    text = [&] {
        static const std::unordered_map<char32_t, char> superscript = {{U'⁰', '0'},
                                                                       {U'¹', '1'},
                                                                       {U'²', '2'},
                                                                       {U'³', '3'},
                                                                       {U'⁴', '4'},
                                                                       {U'⁵', '5'},
                                                                       {U'⁶', '6'},
                                                                       {U'⁷', '7'},
                                                                       {U'⁸', '8'},
                                                                       {U'⁹', '9'},
                                                                       {U'⁻', '-'},
                                                                       {U'⁺', '+'}};
        std::string out;
        bool in_exponent = false;
        for (std::size_t i = 0; i < text.size();) {
            std::size_t next = i + 1;
            const auto cp = decode_one(text, i, next);
            if (const auto it = superscript.find(cp); it != superscript.end()) {
                if (!in_exponent) {
                    if (out.empty() || !std::isdigit(static_cast<unsigned char>(out.back()))) {
                        out.append(text, i, next - i);
                        i = next;
                        continue;
                    }
                    out.push_back('^');
                    in_exponent = true;
                }
                out.push_back(it->second);
            } else {
                in_exponent = false;
                out.append(text, i, next - i);
            }
            i = next;
        }
        return out;
    }();
    auto exponent_words = [](std::string token) {
        if (token.starts_with("−")) {
            token.replace(0, std::string_view("−").size(), "-");
        }
        std::string sign;
        if (!token.empty() && (token.front() == '-' || token.front() == '+')) {
            sign = token.front() == '-' ? "мінус " : "плюс ";
            token.erase(token.begin());
        }
        const auto value = try_parse_ull(token);
        return value ? std::optional<std::string>(sign + number_to_words(*value)) : std::nullopt;
    };
    auto scientific_words = [&](std::string_view base, std::string exponent) {
        const auto base_words = signed_number_words(base, "nom");
        const auto exponent_text = exponent_words(std::move(exponent));
        return base_words && exponent_text
                   ? std::optional<std::string>(*base_words + " помножити на десять у степені " + *exponent_text)
                   : std::nullopt;
    };
    auto scientific_unit = [](std::string_view quantity, const std::ssub_match& unit) {
        if (!unit.matched) {
            return std::string{};
        }
        const auto measurement = measurements().find(unit.str());
        if (measurement == measurements().end()) {
            return std::string{};
        }
        if (!quantity.empty() && (quantity.front() == '+' || quantity.front() == '-')) {
            quantity.remove_prefix(1);
        }
        if (quantity.find_first_of(".,") != std::string_view::npos) {
            return " " + std::string(measurement->second.decimal);
        }
        const auto value = try_parse_ull(quantity);
        return value
                   ? " " + plural(*value, {measurement->second.one, measurement->second.few, measurement->second.many})
                   : std::string{};
    };
    static const std::string unit_suffix = "(?:\\s*(" + unit_alt() + "))?(?![A-Za-zА-Яа-яЄєІіЇїҐґ])";
    static const std::string optional_unit = "(?:\\s*(" + unit_alt() + "))?";
    static const std::regex inverse_celsius_power(
        R"((^|[^\d])([+-]?\d+(?:[.,]\d+)?)\s*(?:×|·|x|X|\*)\s*10−(\d+)\s*°[CС](?:−|-)(\d+)(?!\d))");
    text = regex_sub(text, inverse_celsius_power, [&](const std::smatch& m) {
        const auto words = scientific_words(m[2].str(), "-" + m[3].str());
        if (!words) {
            return m.str();
        }
        const auto inverse = parse_ull(m[4].str());
        return m[1].str() + *words + " на градус Цельсія" +
               (inverse == 1 ? "" : " у степені " + number_to_words(inverse));
    });
    static const std::regex times_ten(
        R"((^|[^A-Za-zА-Яа-яЄєІіЇїҐґ\d.,])([+-]?\d+(?:[.,]\d+)?)\s*(?:×|·|x|X|\*)\s*10\s*\^\s*([+-]?\d+)(?!\d))" +
        unit_suffix);
    text = regex_sub(text, times_ten, [&](const std::smatch& m) {
        const auto words = scientific_words(m[2].str(), m[3].str());
        return words ? m[1].str() + *words + scientific_unit(m[2].str(), m[4]) : m.str();
    });
    static const std::regex times_ten_plain_signed_exponent(
        R"((^|[^A-Za-zА-Яа-яЄєІіЇїҐґ\d.,])([+-]?\d+(?:[.,]\d+)?)\s*(?:×|·|x|X|\*)\s*10\s*((?:\+|-|−)\d+)(?!\d))" +
        optional_unit);
    text = regex_sub(text, times_ten_plain_signed_exponent, [&](const std::smatch& m) {
        const auto words = scientific_words(m[2].str(), m[3].str());
        return words ? m[1].str() + *words + scientific_unit(m[2].str(), m[4]) : m.str();
    });
    static const std::regex e_notation(
        R"((^|[^A-Za-zА-Яа-яЄєІіЇїҐґ\d.,])([+-]?\d+(?:[.,]\d+)?)[eE]([+-]?\d+)(?![A-Za-zА-Яа-яЄєІіЇїҐґ\d]))");
    text = regex_sub(text, e_notation, [&](const std::smatch& m) {
        const auto words = scientific_words(m[2].str(), m[3].str());
        return words ? m[1].str() + *words : m.str();
    });
    static const std::regex negative_power_range(
        R"((^|[^\d])10−(\d+)\s*(?:-|–|—)\s*10−(\d+)(?:\s*(секунди|секунда|секунд|)" + unit_alt() +
        R"())?(?![A-Za-zА-Яа-яЄєІіЇїҐґ]))");
    text = regex_sub(text, negative_power_range, [&](const std::smatch& m) {
        const auto low = exponent_words("-" + m[2].str());
        const auto high = exponent_words("-" + m[3].str());
        if (!low || !high) {
            return m.str();
        }
        std::string unit;
        if (m[4].matched) {
            unit = m[4].str() == "секунди" || m[4].str() == "секунда" || m[4].str() == "секунд"
                       ? " секунд"
                       : scientific_unit("10", m[4]);
        }
        return m[1].str() + range_connector(range_style, "десяти у степені " + *low, "десяти у степені " + *high) +
               unit;
    });
    // In technical prose a tightly joined Unicode minus after 10 denotes a
    // negative power (10−9), not a numeric range from 10 to 9.
    static const std::regex plain_negative_power(R"((^|[^\d])10−(\d+)(?!\d))" + optional_unit);
    text = regex_sub(text, plain_negative_power, [&](const std::smatch& m) {
        const auto exponent = exponent_words("-" + m[2].str());
        return exponent ? m[1].str() + "десять у степені " + *exponent + scientific_unit("10", m[3]) : m.str();
    });
    static const std::regex power(R"((^|[^A-Za-zА-Яа-яЄєІіЇїҐґ\d.,])([+-]?\d+(?:[.,]\d+)?)\s*\^\s*([+-]?\d+)(?!\d))");
    return regex_sub(text, power, [&](const std::smatch& m) {
        const auto base = signed_number_words(m[2].str(), "nom");
        const auto exponent = exponent_words(m[3].str());
        return base && exponent ? m[1].str() + *base + " у степені " + *exponent : m.str();
    });
}
std::string normalize_math(std::string text)
{
    return ctre_sub<R"((\d)\s*\+\s*(?=\d))">(text, [](const auto& m) { return cap_string<1>(m) + " плюс "; });
}

std::string normalize_decimals(std::string text)
{
    return ctre_sub<R"(\b(\d+)[,.](\d+)\b)">(text, [](const auto& m) {
        const auto integer_part = cap_string<1>(m);
        const auto fractional_part = cap_string<2>(m);
        if (fractional_part.find_first_not_of('0') == std::string::npos) {
            return decimal_to_words_or_digits(integer_part, fractional_part);
        }
        return decimal_to_words_or_digits(integer_part, fractional_part);
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
std::string normalize_multipliers(std::string text, bool governed_only)
{
    struct Multiplier {
        Forms forms;
        std::string_view decimal;
        bool feminine;
    };
    static const std::unordered_map<std::string, Multiplier> mult = {
        {"тис", {{"тисяча", "тисячі", "тисяч"}, "тисячі", true}},
        {"млн", {{"мільйон", "мільйони", "мільйонів"}, "мільйона", false}},
        {"млрд", {{"мільярд", "мільярди", "мільярдів"}, "мільярда", false}},
        {"трлн", {{"трильйон", "трильйони", "трильйонів"}, "трильйона", false}}};
    static const std::regex governed(
        R"((^|[^А-Яа-яЄєІіЇїҐґ-])(Близько|близько|Після|після|Менше|менше|Більше|більше|Серед|серед|Без|без|Від|від|До|до|Із|із)\s+(\d+(?:[.,]\d+)?)\s*(тис|млн|млрд|трлн)\.?(?![а-яіїєґ]))",
        std::regex::icase);
    text = regex_sub(text, governed, [&](const std::smatch& m) {
        const auto key = lower_text(m[4].str());
        const auto& [forms, decimal, feminine] = mult.at(key);
        const auto words = signed_number_words(m[3].str(), "gen", feminine ? 'f' : 'm');
        if (!words) {
            return m.str();
        }
        const auto unit = m[3].str().find_first_of(".,") == std::string::npos ? forms[2] : decimal;
        return m[1].str() + m[2].str() + " " + *words + " " + std::string(unit);
    });
    if (governed_only) {
        return text;
    }
    static const std::regex re(R"(\b(\d+(?:[.,]\d+)?)\s*(тис|млн|млрд|трлн)(\.?)(?![а-яіїєґ]))", std::regex::icase);
    return regex_sub(text, re, [&](const std::smatch& m) {
        const auto key = lower_text(m[2].str());
        const auto& [forms, decimal, feminine] = mult.at(key);
        const auto num = m[1].str();
        const auto pos = num.find_first_of(".,");
        if (pos != std::string::npos) {
            auto words = decimal_to_words(std::string_view(num).substr(0, pos), std::string_view(num).substr(pos + 1));
            return words.empty() ? m.str()
                                 : words + " " + std::string(decimal) +
                                       (m[3].matched && m.suffix().str().empty() ? m[3].str() : "");
        }
        const auto n = try_parse_ull(num);
        if (!n) {
            return number_to_words_digit_by_digit(num) + " " + std::string(forms[2]) +
                   (m[3].matched && m.suffix().str().empty() ? m[3].str() : "");
        }
        auto words = split_words(number_to_words(*n));
        if (feminine) {
            feminine_last(words);
        }
        return join(words) + " " + plural(*n, forms) + (m[3].matched && m.suffix().str().empty() ? m[3].str() : "");
    });
}
std::string normalize_versions(std::string text)
{
    static const std::regex named(
        R"((^|[\s(\[{:,;])((?:(?:В|в)ерсі(?:я|ї|ю|єю)|(?:Р|р)еліз(?:у|ом)?|(?:В|в)ипуск(?:у|ом)?|(?:П|п)ункт(?:у|ом|і|а)?|(?:Р|р)озділ(?:у|ом|і|а)?)\s+)(\d+(?:\.\d+)+)\b)");
    text =
        regex_sub(text, named, [](const std::smatch& m) { return m[1].str() + m[2].str() + read_dotted(m[3].str()); });
    text = ctre_sub<R"(\b([A-Za-z][A-Za-z0-9_\-]*\s+)(\d+(?:\.\d+)+)\b)">(
        text, [](const auto& m) { return cap_string<1>(m) + read_dotted(cap<2>(m)); });
    text = ctre_sub<R"(\b([vV])(\d+(?:\.\d+)+)\b)">(
        text, [](const auto& m) { return spell_identifier_letters(cap<1>(m)) + " " + read_dotted(cap<2>(m)); });
    text = ctre_sub<R"(\b([A-Za-z])\.(\d{1,6})\b)">(text, [](const auto& m) {
        return spell_identifier_letters(cap<1>(m)) + " крапка " + number_digits_or_words(cap<2>(m));
    });
    text = ctre_sub<R"(\b(\d+)\.(\d+)([A-Za-z]{1,6})\b)">(text, [](const auto& m) {
        return number_digits_or_words(cap<1>(m)) + " крапка " + number_digits_or_words(cap<2>(m)) + " " +
               spell_identifier_letters(cap<3>(m));
    });
    return ctre_sub<R"(\b\d+(?:\.\d+){2,}\b)">(text, [](const auto& m) { return read_dotted(cap<0>(m)); });
}

std::string normalize_negatives(std::string text)
{
    static const std::regex negative(R"((^|[\s(\[])(?:-|−|–|—)(\d))");
    return regex_sub(text, negative, [](const std::smatch& m) { return m[1].str() + "мінус " + m[2].str(); });
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
