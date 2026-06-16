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

namespace uktextnorm {
namespace {

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

using normalize_uk_cpp::detail::append_utf8;
using normalize_uk_cpp::detail::decode_one;

bool is_uk(char32_t cp)
{
    return (cp >= U'а' && cp <= U'я') || (cp >= U'А' && cp <= U'Я') || cp == U'є' || cp == U'Є' || cp == U'і' ||
           cp == U'І' || cp == U'ї' || cp == U'Ї' || cp == U'ґ' || cp == U'Ґ' || cp == U'’' || cp == U'\'';
}

bool is_upper_uk(char32_t cp)
{
    return (cp >= U'А' && cp <= U'Я') || cp == U'Є' || cp == U'І' || cp == U'Ї' || cp == U'Ґ';
}

bool is_latin(char32_t cp)
{
    return (cp >= U'a' && cp <= U'z') || (cp >= U'A' && cp <= U'Z');
}

bool is_word_joiner(char32_t cp)
{
    return cp == U'\'' || cp == U'’' || cp == U'–' || cp == U'-';
}

char32_t lower_cp(char32_t cp)
{
    if (cp >= U'A' && cp <= U'Z') {
        return cp + 32;
    }
    if (cp >= U'А' && cp <= U'Я') {
        return cp + 32;
    }
    if (cp == U'Є') {
        return U'є';
    }
    if (cp == U'І') {
        return U'і';
    }
    if (cp == U'Ї') {
        return U'ї';
    }
    if (cp == U'Ґ') {
        return U'ґ';
    }
    return cp;
}

char32_t upper_cp(char32_t cp)
{
    if (cp >= U'a' && cp <= U'z') {
        return cp - 32;
    }
    if (cp >= U'а' && cp <= U'я') {
        return cp - 32;
    }
    if (cp == U'є') {
        return U'Є';
    }
    if (cp == U'і') {
        return U'І';
    }
    if (cp == U'ї') {
        return U'Ї';
    }
    if (cp == U'ґ') {
        return U'Ґ';
    }
    return cp;
}

std::string lower_text(std::string_view text)
{
    std::string out;
    for (std::size_t i = 0; i < text.size();) {
        std::size_t next = i + 1;
        append_utf8(out, lower_cp(decode_one(text, i, next)));
        i = next;
    }
    return out;
}

std::string capitalize_first_letter(std::string text)
{
    for (std::size_t i = 0; i < text.size();) {
        std::size_t next = i + 1;
        const auto cp = decode_one(text, i, next);
        if ((cp >= U'a' && cp <= U'z') || is_uk(cp)) {
            std::string out;
            append_utf8(out, upper_cp(cp));
            out.append(text.substr(next));
            return out;
        }
        i = next;
    }
    return text;
}

std::vector<Cp> codepoints(std::string_view text)
{
    std::vector<Cp> out;
    for (std::size_t i = 0; i < text.size();) {
        std::size_t next = i + 1;
        const auto cp = decode_one(text, i, next);
        out.push_back({cp, i, next});
        i = next;
    }
    return out;
}

std::vector<std::size_t> byte_to_char_offsets(std::string_view text)
{
    std::vector<std::size_t> offsets(text.size() + 1);
    std::size_t char_offset = 0;
    for (std::size_t i = 0; i < text.size();) {
        std::size_t next = i + 1;
        decode_one(text, i, next);
        for (std::size_t byte = i; byte < next; ++byte) {
            offsets[byte] = char_offset;
        }
        i = next;
        ++char_offset;
    }
    offsets[text.size()] = char_offset;
    return offsets;
}

bool has_ascii_digit(std::string_view text)
{
    return std::ranges::any_of(text, [](unsigned char ch) { return ch >= '0' && ch <= '9'; });
}

bool has_ascii_alpha(std::string_view text)
{
    return std::ranges::any_of(text,
                               [](unsigned char ch) { return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'); });
}

bool contains_any(std::string_view text, std::string_view chars)
{
    return text.find_first_of(chars) != std::string_view::npos;
}

bool contains_any_token(std::string_view text, std::initializer_list<std::string_view> tokens)
{
    return std::ranges::any_of(tokens,
                               [&](std::string_view token) { return text.find(token) != std::string_view::npos; });
}

bool is_utf8_continuation(char ch)
{
    return (static_cast<unsigned char>(ch) & 0xC0) == 0x80;
}

bool has_roman_candidate(std::string_view text)
{
    return text.find_first_of("MDCLXVI") != std::string_view::npos;
}

bool has_currency_candidate(std::string_view text)
{
    return contains_any(text, "$€£₴") ||
           contains_any_token(text, {"грн", "UAH", "USD", "EUR", "GBP", "долар", "євро", "фунт"});
}

bool has_symbol_candidate(std::string_view text)
{
    return contains_any_token(text, {"°", "±", "≈", "≠", "≤", "≥", "×", "÷", "=", "<", ">", "‰",
                                     "§", "₿", "•", "·", "~", "&", "#", "_", "²", "³", "№"});
}

bool is_ascii_acronym(std::string_view text)
{
    if (text.size() < 2 || text.size() > 6) {
        return false;
    }
    return std::ranges::all_of(text, [](unsigned char ch) { return ch >= 'A' && ch <= 'Z'; });
}

bool is_uncertain_word_char(char32_t cp)
{
    return is_latin(cp) || is_uk(cp) || is_word_joiner(cp);
}

std::vector<Cp> uncertain_word_spans(std::string_view text)
{
    std::vector<Cp> spans;
    std::size_t word_start = std::string_view::npos;
    std::size_t word_stop = 0;
    bool has_letter = false;
    for (std::size_t i = 0; i < text.size();) {
        std::size_t next = i + 1;
        const auto cp = decode_one(text, i, next);
        if (is_uncertain_word_char(cp)) {
            if (word_start == std::string_view::npos) {
                word_start = i;
            }
            word_stop = next;
            has_letter = has_letter || is_latin(cp) || (is_uk(cp) && !is_word_joiner(cp));
        } else {
            if (word_start != std::string_view::npos && has_letter) {
                spans.push_back({0, word_start, word_stop});
            }
            word_start = std::string_view::npos;
            has_letter = false;
        }
        i = next;
    }
    if (word_start != std::string_view::npos && has_letter) {
        spans.push_back({0, word_start, word_stop});
    }
    return spans;
}

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

void replace_all(std::string& text, std::string_view from, std::string_view to)
{
    std::size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos) {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
}

std::vector<std::string> split_words(std::string_view text)
{
    std::vector<std::string> out;
    std::istringstream in{std::string(text)};
    std::string word;
    while (in >> word) {
        out.push_back(word);
    }
    return out;
}

std::string join(const std::vector<std::string>& words, std::string_view sep = " ")
{
    std::string out;
    for (std::size_t i = 0; i < words.size(); ++i) {
        if (i) {
            out += sep;
        }
        out += words[i];
    }
    return out;
}

unsigned long long parse_ull(std::string_view text)
{
    unsigned long long out = 0;
    std::from_chars(text.data(), text.data() + text.size(), out);
    return out;
}

std::optional<unsigned long long> try_parse_ull(std::string_view text)
{
    unsigned long long out = 0;
    const auto* begin = text.data();
    const auto* end = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(begin, end, out);
    if (ec != std::errc{} || ptr != end) {
        return std::nullopt;
    }
    return out;
}

std::string number_digits_or_words(std::string_view digits)
{
    if (const auto parsed = try_parse_ull(digits)) {
        return number_to_words(*parsed);
    }
    return number_to_words_digit_by_digit(digits);
}

int parse_int(std::string_view text)
{
    int out = 0;
    std::from_chars(text.data(), text.data() + text.size(), out);
    return out;
}

std::string trim_spaces(std::string text)
{
    std::string out;
    out.reserve(text.size());
    bool previous_space = false;
    for (const char ch : text) {
        if (ch == ' ') {
            if (!previous_space) {
                out.push_back(ch);
            }
            previous_space = true;
        } else {
            out.push_back(ch);
            previous_space = false;
        }
    }
    if (!out.empty() && out.front() == ' ') {
        out.erase(out.begin());
    }
    if (!out.empty() && out.back() == ' ') {
        out.pop_back();
    }
    return out;
}

std::string plural(unsigned long long n, const Forms& forms)
{
    if (n % 10 == 1 && n % 100 != 11) {
        return std::string(forms[0]);
    }
    if (2 <= n % 10 && n % 10 <= 4 && !(12 <= n % 100 && n % 100 <= 14)) {
        return std::string(forms[1]);
    }
    return std::string(forms[2]);
}

void feminine_last(std::vector<std::string>& words)
{
    if (words.empty()) {
        return;
    }
    if (words.back() == "один") {
        words.back() = "одна";
    } else if (words.back() == "два") {
        words.back() = "дві";
    }
}

void neuter_last(std::vector<std::string>& words)
{
    if (words.empty()) {
        return;
    }
    if (words.back() == "один") {
        words.back() = "одне";
    }
}

std::vector<std::string> under_thousand(unsigned n)
{
    static constexpr std::array<std::string_view, 10> units = {
        "", "один", "два", "три", "чотири", "п'ять", "шість", "сім", "вісім", "дев'ять"};
    static constexpr std::array<std::string_view, 10> teens = {"десять",
                                                               "одинадцять",
                                                               "дванадцять",
                                                               "тринадцять",
                                                               "чотирнадцять",
                                                               "п'ятнадцять",
                                                               "шістнадцять",
                                                               "сімнадцять",
                                                               "вісімнадцять",
                                                               "дев'ятнадцять"};
    static constexpr std::array<std::string_view, 10> tens = {"",
                                                              "десять",
                                                              "двадцять",
                                                              "тридцять",
                                                              "сорок",
                                                              "п'ятдесят",
                                                              "шістдесят",
                                                              "сімдесят",
                                                              "вісімдесят",
                                                              "дев'яносто"};
    static constexpr std::array<std::string_view, 10> hundreds = {
        "", "сто", "двісті", "триста", "чотириста", "п'ятсот", "шістсот", "сімсот", "вісімсот", "дев'ятсот"};
    if (n == 0) {
        return {};
    }
    if (n < 10) {
        return {std::string(units[n])};
    }
    if (n < 20) {
        return {std::string(teens[n - 10])};
    }
    if (n < 100) {
        auto rest = under_thousand(n % 10);
        rest.insert(rest.begin(), std::string(tens[n / 10]));
        return rest;
    }
    auto rest = under_thousand(n % 100);
    rest.insert(rest.begin(), std::string(hundreds[n / 100]));
    return rest;
}

std::string decimal_to_words(std::string_view int_part, std::string_view frac_part)
{
    static const std::unordered_map<std::size_t, std::string_view> places = {
        {1, "десятих"}, {2, "сотих"}, {3, "тисячних"}, {4, "десятитисячних"}, {5, "стотисячних"}, {6, "мільйонних"}};
    const auto it = places.find(frac_part.size());
    const auto int_value = try_parse_ull(int_part);
    const auto frac_value = try_parse_ull(frac_part);
    if (it == places.end() || !int_value || !frac_value) {
        return {};
    }
    auto int_words = split_words(number_to_words(*int_value));
    feminine_last(int_words);
    const auto whole = (*int_value % 10 == 1 && *int_value % 100 != 11) ? "ціла" : "цілих";
    auto frac_words = split_words(number_to_words(*frac_value));
    feminine_last(frac_words);
    return join(int_words) + " " + whole + " і " + join(frac_words) + " " + std::string(it->second);
}

const std::unordered_map<std::string, std::string>& abbreviation_map()
{
    static const std::unordered_map<std::string, std::string> map = {
        {"рр.", "роки"},
        {"р-н", "район"},
        {"дон.е.", "до нашої ери"},
        {"н.е.", "нашої ери"},
        {"див.також", "дивись також"},
        {"пн.ш.", "північної широти"},
        {"пд.ш.", "південної широти"},
        {"сх.д.", "східної довготи"},
        {"зх.д.", "західної довготи"},
        {"іт.д.", "і так далі"},
        {"іт.ін.", "і таке інше"},
        {"б/в", "був у використанні"},
        {"таін.", "та інші"},
        {"т.б.", "тобто"},
        {"грн.", "гривень"},
        {"коп.", "копійок"},
        {"пдв", "податок на додану вартість"},
        {"єдрпоу", "єдиний державний реєстр підприємств та організацій України"},
        {"фоп", "фізична особа підприємець"},
        {"тов", "товариство з обмеженою відповідальністю"}};
    return map;
}

std::string compact_spaces_lower(std::string_view text)
{
    std::string out;
    for (std::size_t i = 0; i < text.size();) {
        std::size_t next = i + 1;
        const auto cp = decode_one(text, i, next);
        if (cp != U' ' && cp != U'\t' && cp != U'\n' && cp != U'\r') {
            append_utf8(out, lower_cp(cp));
        }
        i = next;
    }
    return out;
}

const std::unordered_map<std::string, std::string>& cyr_map()
{
    static const std::unordered_map<std::string, std::string> map = {
        {"a", "а"},  {"b", "б"},  {"c", "к"},  {"d", "д"},  {"e", "е"},  {"f", "ф"},  {"g", "г"},  {"h", "г"},
        {"i", "і"},  {"j", "дж"}, {"k", "к"},  {"l", "л"},  {"m", "м"},  {"n", "н"},  {"o", "о"},  {"p", "п"},
        {"q", "к"},  {"r", "р"},  {"s", "с"},  {"t", "т"},  {"u", "у"},  {"v", "в"},  {"w", "в"},  {"x", "кс"},
        {"y", "и"},  {"z", "з"},  {"sh", "ш"}, {"ch", "ч"}, {"zh", "ж"}, {"kh", "х"}, {"ts", "ц"}, {"yu", "ю"},
        {"ya", "я"}, {"ye", "є"}, {"yi", "ї"}, {"oo", "у"}, {"ee", "і"}, {"sch", "щ"}};
    return map;
}

const std::unordered_map<std::string, std::string>& pronunciation_map()
{
    static const std::unordered_map<std::string, std::string> map = {
        {"А", "а"},  {"Б", "бе"}, {"В", "ве"},          {"Г", "ге"}, {"Ґ", "ґе"}, {"Д", "де"}, {"Е", "е"},
        {"Є", "є"},  {"Ж", "же"}, {"З", "зе"},          {"И", "и"},  {"І", "і"},  {"Ї", "ї"},  {"Й", "йот"},
        {"К", "ка"}, {"Л", "ел"}, {"М", "ем"},          {"Н", "ен"}, {"О", "о"},  {"П", "пе"}, {"Р", "ер"},
        {"С", "ес"}, {"Т", "те"}, {"У", "у"},           {"Ф", "еф"}, {"Х", "ха"}, {"Ц", "це"}, {"Ч", "че"},
        {"Ш", "ша"}, {"Щ", "ща"}, {"Ь", "м'який знак"}, {"Ю", "ю"},  {"Я", "я"}};
    return map;
}

const std::unordered_map<std::string, Measurement>& measurements()
{
    static const std::unordered_map<std::string, Measurement> map = {
        {"км", {"кілометр", "кілометри", "кілометрів", 'm'}},
        {"м", {"метр", "метри", "метрів", 'm'}},
        {"см", {"сантиметр", "сантиметри", "сантиметрів", 'm'}},
        {"мм", {"міліметр", "міліметри", "міліметрів", 'm'}},
        {"дм", {"дециметр", "дециметри", "дециметрів", 'm'}},
        {"кг", {"кілограм", "кілограми", "кілограмів", 'm'}},
        {"мг", {"міліграм", "міліграми", "міліграмів", 'm'}},
        {"л", {"літр", "літри", "літрів", 'm'}},
        {"мл", {"мілілітр", "мілілітри", "мілілітрів", 'm'}},
        {"год", {"година", "години", "годин", 'f'}},
        {"хв", {"хвилина", "хвилини", "хвилин", 'f'}},
        {"с", {"секунда", "секунди", "секунд", 'f'}},
        {"га", {"гектар", "гектари", "гектарів", 'm'}},
        {"м²", {"квадратний метр", "квадратні метри", "квадратних метрів", 'm'}},
        {"м2", {"квадратний метр", "квадратні метри", "квадратних метрів", 'm'}},
        {"км²", {"квадратний кілометр", "квадратні кілометри", "квадратних кілометрів", 'm'}},
        {"км2", {"квадратний кілометр", "квадратні кілометри", "квадратних кілометрів", 'm'}},
        {"м³", {"кубічний метр", "кубічні метри", "кубічних метрів", 'm'}},
        {"м3", {"кубічний метр", "кубічні метри", "кубічних метрів", 'm'}},
        {"км/год", {"кілометр на годину", "кілометри на годину", "кілометрів на годину", 'm'}},
        {"м/с", {"метр за секунду", "метри за секунду", "метрів за секунду", 'm'}},
        {"Гц", {"герц", "герци", "герц", 'm'}},
        {"кГц", {"кілогерц", "кілогерци", "кілогерц", 'm'}},
        {"МГц", {"мегагерц", "мегагерци", "мегагерц", 'm'}},
        {"ГГц", {"гігагерц", "гігагерци", "гігагерц", 'm'}},
        {"Вт", {"ват", "вати", "ват", 'm'}},
        {"кВт", {"кіловат", "кіловати", "кіловат", 'm'}},
        {"МВт", {"мегават", "мегавати", "мегават", 'm'}},
        {"В", {"вольт", "вольти", "вольт", 'm'}},
        {"А", {"ампер", "ампери", "ампер", 'm'}},
        {"Ом", {"ом", "оми", "ом", 'm'}},
        {"байт", {"байт", "байти", "байт", 'm'}},
        {"КБ", {"кілобайт", "кілобайти", "кілобайт", 'm'}},
        {"МБ", {"мегабайт", "мегабайти", "мегабайт", 'm'}},
        {"ГБ", {"гігабайт", "гігабайти", "гігабайт", 'm'}},
        {"°", {"градус", "градуси", "градусів", 'm'}},
        {"т", {"тонна", "тонни", "тонн", 'f'}},
        {"хв.", {"хвилина", "хвилини", "хвилин", 'f'}},
        {"год.", {"година", "години", "годин", 'f'}}};
    return map;
}

const std::string& unit_alt()
{
    static const std::string alt = [] {
        std::vector<std::string> units;
        for (const auto& [unit, _] : measurements()) {
            units.push_back(unit);
        }
        std::sort(units.begin(), units.end(), [](const auto& a, const auto& b) { return a.size() > b.size(); });
        std::string out;
        for (std::size_t i = 0; i < units.size(); ++i) {
            if (i) {
                out += "|";
            }
            for (const char ch : units[i]) {
                if (std::string_view(R"(\-[]{}()*+?.,^$|# )").contains(ch)) {
                    out.push_back('\\');
                }
                out.push_back(ch);
            }
        }
        return out;
    }();
    return alt;
}

const std::string& counted_noun_alt();

const std::string& month_alt()
{
    static const std::string alt =
        "січня|січ\\.|лютого|лют\\.|березня|бер\\.|квітня|квіт\\.|травня|трав\\.|червня|черв\\.|липня|лип\\.|серпня|"
        "серп\\.|вересня|вер\\.|жовтня|жовт\\.|листопада|лист\\.|грудня|груд\\.";
    return alt;
}

const std::regex& date_day_range_re()
{
    static const std::regex re("\\b(\\d{1,2})\\s*[-–—]\\s*(\\d{1,2})\\s+(" + month_alt() +
                               R"()\s+(\d{3,4})(?:\s+року\b|\s*р\.(?![а-яіїєґ]))?)");
    return re;
}

const std::regex& date_spelled_re()
{
    static const std::regex re("\\b(\\d{1,2})\\s+(" + month_alt() +
                               R"()\s+(\d{3,4})(?:\s+року\b|\s*р\.(?![а-яіїєґ]))?)");
    return re;
}

const std::regex& range_units_re()
{
    static const std::regex re("(^|[^\\d])(\\d+)\\s*[-–—]\\s*(\\d+)\\s*(" + unit_alt() +
                               R"()(?![A-Za-zА-Яа-яЄєІіЇїҐґ]))");
    return re;
}

const std::regex& case_prep_re()
{
    static const std::regex re(
        R"((^|[^А-Яа-яЄєІіЇїҐґ-])(близько|після|понад|менше|більше|перед|між|без|від|до|із|об|на|к|о|у|в|з)\s+(\d+)(?:\s*()" +
            unit_alt() + R"())?(?![\d.,:%–—-])(?![A-Za-zА-Яа-яЄєІіЇїҐґ]))",
        std::regex::icase);
    return re;
}

const std::regex& counted_ponad_re()
{
    static const std::regex re("(^|[^А-Яа-яЄєІіЇїҐґ\\d])(понад)\\s+([1-9]\\d{0,5})\\s+(" + counted_noun_alt() +
                                   R"()(?![А-Яа-яЄєІіЇїҐґ]))",
                               std::regex::icase);
    return re;
}

const std::regex& counted_genitive_re()
{
    static const std::regex re(
        "(^|[^А-Яа-яЄєІіЇїҐґ\\d])(близько|більше|менше|до|від|без|після|із)\\s+([1-9]\\d{0,5})\\s+(" +
            counted_noun_alt() + R"()(?![А-Яа-яЄєІіЇїҐґ]))",
        std::regex::icase);
    return re;
}

const std::regex& counted_nouns_re()
{
    static const std::regex re("(^|[^А-Яа-яЄєІіЇїҐґ\\d])([1-9]\\d{0,5})\\s+(" + counted_noun_alt() +
                                   R"()(?![А-Яа-яЄєІіЇїҐґ]))",
                               std::regex::icase);
    return re;
}

const std::regex& measurements_re()
{
    static const std::regex re(R"((^|[^\d.,])(\d+(?:[.,]\d+)?)\s*()" + unit_alt() +
                               R"()\.?(?![A-Za-zА-Яа-яЄєІіЇїҐґ]))");
    return re;
}

const std::regex& symbol_currency_prefix_re()
{
    static const std::string token = R"((?:тис|млн|млрд|трлн)\.?)";
    static const std::string currency = R"((?:грн|UAH|USD|EUR|GBP|[$€£₴]))";
    static const std::regex re("(" + currency + ")\\s*(\\d+(?:[.,]\\d+)?)\\s*(" + token + ")");
    return re;
}

const std::regex& symbol_currency_suffix_re()
{
    static const std::string token = R"((?:тис|млн|млрд|трлн)\.?)";
    static const std::string currency = R"((?:грн|UAH|USD|EUR|GBP|[$€£₴]))";
    static const std::regex re("(\\d+(?:[.,]\\d+)?)\\s*(" + token + ")\\s*(" + currency + ")");
    return re;
}

const std::unordered_map<std::string, std::string>& cardinal_to_ordinal()
{
    static const std::unordered_map<std::string, std::string> map = {{"один", "перший"},
                                                                     {"одна", "перший"},
                                                                     {"два", "другий"},
                                                                     {"дві", "другий"},
                                                                     {"три", "третій"},
                                                                     {"чотири", "четвертий"},
                                                                     {"п'ять", "п'ятий"},
                                                                     {"шість", "шостий"},
                                                                     {"сім", "сьомий"},
                                                                     {"вісім", "восьмий"},
                                                                     {"дев'ять", "дев'ятий"},
                                                                     {"десять", "десятий"},
                                                                     {"одинадцять", "одинадцятий"},
                                                                     {"дванадцять", "дванадцятий"},
                                                                     {"тринадцять", "тринадцятий"},
                                                                     {"чотирнадцять", "чотирнадцятий"},
                                                                     {"п'ятнадцять", "п'ятнадцятий"},
                                                                     {"шістнадцять", "шістнадцятий"},
                                                                     {"сімнадцять", "сімнадцятий"},
                                                                     {"вісімнадцять", "вісімнадцятий"},
                                                                     {"дев'ятнадцять", "дев'ятнадцятий"},
                                                                     {"двадцять", "двадцятий"},
                                                                     {"тридцять", "тридцятий"},
                                                                     {"сорок", "сороковий"},
                                                                     {"п'ятдесят", "п'ятдесятий"},
                                                                     {"шістдесят", "шістдесятий"},
                                                                     {"сімдесят", "сімдесятий"},
                                                                     {"вісімдесят", "вісімдесятий"},
                                                                     {"дев'яносто", "дев'яностий"},
                                                                     {"сто", "сотий"},
                                                                     {"двісті", "двохсотий"},
                                                                     {"триста", "трьохсотий"},
                                                                     {"чотириста", "чотирьохсотий"},
                                                                     {"п'ятсот", "п'ятисотий"},
                                                                     {"шістсот", "шестисотий"},
                                                                     {"сімсот", "семисотий"},
                                                                     {"вісімсот", "восьмисотий"},
                                                                     {"дев'ятсот", "дев'ятисотий"},
                                                                     {"тисяча", "тисячний"},
                                                                     {"тисячі", "тисячний"},
                                                                     {"тисяч", "тисячний"}};
    return map;
}

std::string inflect_ordinal(std::string stem, std::string_view form)
{
    if (form == "nom_m") {
        return stem;
    }
    static const std::unordered_map<std::string, std::string_view> endings = {{"nom_n", "е"},
                                                                              {"nom_f", "а"},
                                                                              {"nom_pl", "і"},
                                                                              {"gen", "ого"},
                                                                              {"dat", "ому"},
                                                                              {"prep", "ому"},
                                                                              {"pl", "их"},
                                                                              {"acc_f", "у"}};
    const auto it = endings.find(std::string(form));
    if (it == endings.end()) {
        return stem;
    }
    if (stem.ends_with("ій")) {
        stem.resize(stem.size() - std::string_view("ій").size());
    } else if (stem.ends_with("ий")) {
        stem.resize(stem.size() - std::string_view("ий").size());
    }
    stem += it->second;
    return stem;
}

const std::unordered_map<std::string, CaseForms>& case_forms()
{
    static const std::unordered_map<std::string, CaseForms> map = {
        {"нуль", {"нуля", "нулю", "нулем", "нулі"}},
        {"один", {"одного", "одному", "одним", "одному"}},
        {"одна", {"однієї", "одній", "однією", "одній"}},
        {"два", {"двох", "двом", "двома", "двох"}},
        {"дві", {"двох", "двом", "двома", "двох"}},
        {"три", {"трьох", "трьом", "трьома", "трьох"}},
        {"чотири", {"чотирьох", "чотирьом", "чотирма", "чотирьох"}},
        {"п'ять", {"п'яти", "п'яти", "п'ятьма", "п'яти"}},
        {"шість", {"шести", "шести", "шістьма", "шести"}},
        {"сім", {"семи", "семи", "сьома", "семи"}},
        {"вісім", {"восьми", "восьми", "вісьмома", "восьми"}},
        {"дев'ять", {"дев'яти", "дев'яти", "дев'ятьма", "дев'яти"}},
        {"десять", {"десяти", "десяти", "десятьма", "десяти"}},
        {"одинадцять", {"одинадцяти", "одинадцяти", "одинадцятьма", "одинадцяти"}},
        {"дванадцять", {"дванадцяти", "дванадцяти", "дванадцятьма", "дванадцяти"}},
        {"тринадцять", {"тринадцяти", "тринадцяти", "тринадцятьма", "тринадцяти"}},
        {"чотирнадцять", {"чотирнадцяти", "чотирнадцяти", "чотирнадцятьма", "чотирнадцяти"}},
        {"п'ятнадцять", {"п'ятнадцяти", "п'ятнадцяти", "п'ятнадцятьма", "п'ятнадцяти"}},
        {"шістнадцять", {"шістнадцяти", "шістнадцяти", "шістнадцятьма", "шістнадцяти"}},
        {"сімнадцять", {"сімнадцяти", "сімнадцяти", "сімнадцятьма", "сімнадцяти"}},
        {"вісімнадцять", {"вісімнадцяти", "вісімнадцяти", "вісімнадцятьма", "вісімнадцяти"}},
        {"дев'ятнадцять", {"дев'ятнадцяти", "дев'ятнадцяти", "дев'ятнадцятьма", "дев'ятнадцяти"}},
        {"двадцять", {"двадцяти", "двадцяти", "двадцятьма", "двадцяти"}},
        {"тридцять", {"тридцяти", "тридцяти", "тридцятьма", "тридцяти"}},
        {"сорок", {"сорока", "сорока", "сорока", "сорока"}},
        {"дев'яносто", {"дев'яноста", "дев'яноста", "дев'яноста", "дев'яноста"}},
        {"сто", {"ста", "ста", "ста", "ста"}},
        {"двісті", {"двохсот", "двомстам", "двомастами", "двохстах"}},
        {"триста", {"трьохсот", "трьомстам", "трьомастами", "трьохстах"}},
        {"чотириста", {"чотирьохсот", "чотирьомстам", "чотирмастами", "чотирьохстах"}},
        {"п'ятсот", {"п'ятисот", "п'ятистам", "п'ятьмастами", "п'ятистах"}},
        {"шістсот", {"шестисот", "шестистам", "шістьмастами", "шестистах"}},
        {"сімсот", {"семисот", "семистам", "сьомастами", "семистах"}},
        {"вісімсот", {"восьмисот", "восьмистам", "вісьмомастами", "восьмистах"}},
        {"дев'ятсот", {"дев'ятисот", "дев'ятистам", "дев'ятьмастами", "дев'ятистах"}},
        {"тисяча", {"тисячі", "тисячі", "тисячею", "тисячі"}},
        {"тисячі", {"тисяч", "тисячам", "тисячами", "тисячах"}},
        {"тисяч", {"тисяч", "тисячам", "тисячами", "тисячах"}},
        {"мільйон", {"мільйона", "мільйону", "мільйоном", "мільйоні"}},
        {"мільйони", {"мільйонів", "мільйонам", "мільйонами", "мільйонах"}},
        {"мільйонів", {"мільйонів", "мільйонам", "мільйонами", "мільйонах"}},
        {"мільярд", {"мільярда", "мільярду", "мільярдом", "мільярді"}},
        {"мільярди", {"мільярдів", "мільярдам", "мільярдами", "мільярдах"}},
        {"мільярдів", {"мільярдів", "мільярдам", "мільярдами", "мільярдах"}}};
    return map;
}

std::string normalize_typography(std::string text)
{
    for (auto space : {"\xC2\xA0", "\xE2\x80\x89", "\xE2\x80\xAF", "\xE2\x81\xA0"}) {
        replace_all(text, space, " ");
    }
    auto strip_exact_pair = [](std::string value, char marker) {
        std::string out;
        for (std::size_t i = 0; i < value.size();) {
            std::size_t j = i;
            while (j < value.size() && value[j] == marker) {
                ++j;
            }
            const auto run = j - i;
            if (run == 2) {
                i = j;
                continue;
            }
            out.append(value.substr(i, run == 0 ? 1 : run));
            i += run == 0 ? 1 : run;
        }
        return out;
    };
    text = strip_exact_pair(std::move(text), '*');
    text = strip_exact_pair(std::move(text), '_');
    replace_all(text, "`", "");
    replace_all(text, "’", "'");
    return text;
}

std::string normalize_web(std::string text)
{
    static const std::regex email(R"(\b[\w.+-]+@[\w-]+\.[A-Za-zА-Яа-яЄєІіЇїҐґ]{2,}\b)");
    static const std::regex url(R"(\b(?:https?://|www\.)\S+|\b[\w-]+\.(?:com|ua|org|net|info|io|edu|gov|укр)\b)",
                                std::regex::icase);
    auto spell = [](std::string s) {
        while (!s.empty() && std::string_view(".,!?").contains(s.back())) {
            s.pop_back();
        }
        for (const auto& [sym, word] : std::array<std::pair<std::string_view, std::string_view>, 5>{
                 {{"@", " равлик "}, {".", " крапка "}, {"/", " слеш "}, {":", " двокрапка "}, {"-", " дефіс "}}}) {
            replace_all(s, sym, word);
        }
        replace_all(s, "_", " підкреслення ");
        replace_all(s, "?", " знак питання ");
        replace_all(s, "=", " дорівнює ");
        replace_all(s, "&", " амперсанд ");
        replace_all(s, "+", " плюс ");
        return trim_spaces(std::move(s));
    };
    text = regex_sub(text, email, [&](const std::smatch& m) { return spell(m.str()); });
    text = regex_sub(text, url, [&](const std::smatch& m) { return spell(m.str()); });
    text =
        ctre_sub<R"(#([A-Za-zА-Яа-яЄєІіЇїҐґ0-9_]+))">(text, [](const auto& m) { return "хештег " + cap_string<1>(m); });
    static const std::regex handle(R"((^|[^A-Za-zА-Яа-яЄєІіЇїҐґ0-9._%+-])@([A-Za-z][A-Za-z0-9_]{1,30}))");
    return regex_sub(text, handle, [](const std::smatch& m) { return m[1].str() + "акаунт " + m[2].str(); });
}

std::string normalize_dates(std::string text, DateStyle style)
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
        return day_words(m[3].str()) + " " + std::string(months[m1 - 1]) + " " +
               number_to_ordinal_words(parse_ull(m[1].str()), "gen") + " року " + day_words(m[6].str()) + " " +
               std::string(months[m2 - 1]) + " " + number_to_ordinal_words(parse_ull(m[4].str()), "gen") + " року";
    });
    text = ctre_sub<R"(\b(\d{1,2})\.(\d{1,2})\.(\d{4})\b)">(text, [&](const auto& m) {
        const int month = parse_int(cap<2>(m));
        if (month < 1 || month > 12) {
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
        return day_words(cap<1>(m)) + " " + std::string(months[month - 1]) + " " +
               number_to_ordinal_words(parse_ull(cap<3>(m)), "gen") + " року";
    });
    text = ctre_sub<R"(\b(\d{4})-(\d{2})-(\d{2})\b)">(text, [&](const auto& m) {
        const int month = parse_int(cap<2>(m));
        if (month < 1 || month > 12) {
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

int roman_to_int(std::string_view s)
{
    static const std::unordered_map<char, int> values = {
        {'I', 1}, {'V', 5}, {'X', 10}, {'L', 50}, {'C', 100}, {'D', 500}, {'M', 1000}};
    int total = 0;
    int prev = 0;
    for (auto it = s.rbegin(); it != s.rend(); ++it) {
        const int v = values.at(*it);
        total += v < prev ? -v : v;
        prev = v;
    }
    return total;
}

bool valid_roman(std::string_view s)
{
    return ctre::match<R"(M{0,4}(?:CM|CD|D?C{0,3})(?:XC|XL|L?X{0,3})(?:IX|IV|V?I{0,3}))">(s);
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
                                                                                  {"х", "pl"}};
    static const std::unordered_set<std::string> stop = {
        "CD", "DVD", "MD", "DC", "MC", "MI", "MM", "DI", "DIV", "MIX", "CIV", "LCD"};
    text = ctre_sub<R"((\d+)(?:-|–|—)(го|му|й|м|а|у|е|х)(?![А-Яа-яЄєІіЇїҐґ]))">(text, [&](const auto& m) {
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

std::string normalize_addresses(std::string text)
{
    static const std::unordered_map<std::string, std::string> words = {{"м", "місто"},
                                                                       {"с", "село"},
                                                                       {"смт", "селище міського типу"},
                                                                       {"вул", "вулиця"},
                                                                       {"просп", "проспект"},
                                                                       {"пр", "проспект"},
                                                                       {"пров", "провулок"},
                                                                       {"пл", "площа"},
                                                                       {"бул", "бульвар"},
                                                                       {"наб", "набережна"},
                                                                       {"буд", "будинок"},
                                                                       {"б", "будинок"},
                                                                       {"кв", "квартира"},
                                                                       {"оф", "офіс"},
                                                                       {"корп", "корпус"},
                                                                       {"під", "під'їзд"},
                                                                       {"пов", "поверх"},
                                                                       {"обл", "область"},
                                                                       {"р-н", "район"}};
    static const std::regex re(
        R"((^|[^0-9А-Яа-яЄєІіЇїҐґ])((?:смт|просп|пров|корп|буд|вул|наб|бул|оф|кв|обл|під|пов|р-н|пр|пл|м|с|б))\.(?=\s*[A-Za-zА-Яа-яЄєІіЇїҐґ0-9]))",
        std::regex::icase);
    return regex_sub(text, re, [](const std::smatch& m) {
        const auto key = lower_text(m[2].str());
        return m[1].str() + words.at(key);
    });
}

std::string normalize_number_groups(std::string text)
{
    return ctre_sub<R"(\b\d{1,3}(?: \d{3})+\b)">(text, [](const auto& m) {
        auto s = whole_string(m);
        replace_all(s, " ", "");
        return s;
    });
}

std::string normalize_sections(std::string text)
{
    static const std::unordered_map<std::string, std::string> section = {{"ст", "стаття"},
                                                                         {"ч", "частина"},
                                                                         {"пп", "підпункт"},
                                                                         {"п", "пункт"},
                                                                         {"абз", "абзац"},
                                                                         {"розд", "розділ"},
                                                                         {"гл", "глава"},
                                                                         {"табл", "таблиця"},
                                                                         {"рис", "рисунок"}};
    static const std::regex re(R"((^|[^А-Яа-яЄєІіЇїҐґA-Za-z])(ст|ч|пп|п|абз|розд|гл|табл|рис)\.\s*(?=\d|[MDCLXVI]))",
                               std::regex::icase);
    return regex_sub(text, re, [&](const std::smatch& m) {
        const auto key = lower_text(m[2].str());
        if (key == "ст") {
            const auto prefix = m.prefix().str();
            std::smatch prev;
            if (std::regex_search(prefix, prev, std::regex(R"(([MDCLXVI]{1,6})\s*$)"))) {
                if (valid_roman(prev[1].str())) {
                    return m.str();
                }
            }
        }
        return m[1].str() + section.at(key) + " ";
    });
}

std::string normalize_known_acronyms(std::string text)
{
    static const std::unordered_map<std::string, std::string> map = {
        {"ПДВ", "податок на додану вартість"},
        {"ЄДРПОУ", "єдиний державний реєстр підприємств та організацій України"},
        {"ФОП", "фізична особа підприємець"},
        {"ТОВ", "товариство з обмеженою відповідальністю"},
        {"АТ", "акціонерне товариство"},
        {"ОСББ", "об'єднання співвласників багатоквартирного будинку"},
        {"ЗСУ", "збройні сили України"},
        {"СБУ", "служба безпеки України"},
        {"МВС", "міністерство внутрішніх справ"},
        {"ДСНС", "державна служба України з надзвичайних ситуацій"},
        {"НПУ", "національна поліція України"},
        {"ДБР", "державне бюро розслідувань"},
        {"НАБУ", "національне антикорупційне бюро України"},
        {"САП", "спеціалізована антикорупційна прокуратура"},
        {"НАЗК", "національне агентство з питань запобігання корупції"},
        {"НБУ", "національний банк України"},
        {"КМУ", "кабінет міністрів України"},
        {"ВРУ", "верховна рада України"},
        {"ЦВК", "центральна виборча комісія"},
        {"МОН", "міністерство освіти і науки України"},
        {"МОЗ", "міністерство охорони здоров'я України"},
        {"ДПС", "державна податкова служба України"},
        {"ЄС", "європейський союз"},
        {"УЗ", "українська залізниця"},
        {"ОВА", "обласна військова адміністрація"},
        {"РДА", "районна державна адміністрація"}};
    static const std::regex re(
        R"((^|[^А-Яа-яЄєІіЇїҐґ])((?:ПДВ|ЄДРПОУ|ФОП|ТОВ|АТ|ОСББ|ЗСУ|СБУ|МВС|ДСНС|НПУ|ДБР|НАБУ|САП|НАЗК|НБУ|КМУ|ВРУ|ЦВК|МОН|МОЗ|ДПС|ЄС|УЗ|ОВА|РДА))(?![А-Яа-яЄєІіЇїҐґ]))");
    return regex_sub(text, re, [](const std::smatch& m) {
        auto out = map.at(m[2].str());
        const auto prefix = m[1].str();
        auto before = m.prefix().str() + prefix;
        while (!before.empty() && std::isspace(static_cast<unsigned char>(before.back()))) {
            before.pop_back();
        }
        if (before.empty() || before.back() == '.' || before.back() == '!' || before.back() == '?') {
            out = capitalize_first_letter(std::move(out));
        }
        return prefix + out;
    });
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
        const bool instr_case = noun.ends_with("ами") || noun.ends_with("ями") || noun.ends_with("ма");
        return number_to_words_case(parse_ull(m[1].str()), instr_case ? "instr" : "prep") + " " + noun;
    });
}

const std::unordered_map<std::string, CountedNoun>& counted_nouns()
{
    static const std::unordered_map<std::string, CountedNoun> map = {
        {"користувач", {"користувач", "користувачі", "користувачів", 'm'}},
        {"користувачі", {"користувач", "користувачі", "користувачів", 'm'}},
        {"користувачів", {"користувач", "користувачі", "користувачів", 'm'}},
        {"документ", {"документ", "документи", "документів", 'm'}},
        {"документи", {"документ", "документи", "документів", 'm'}},
        {"документів", {"документ", "документи", "документів", 'm'}},
        {"файл", {"файл", "файли", "файлів", 'm'}},
        {"файли", {"файл", "файли", "файлів", 'm'}},
        {"файлів", {"файл", "файли", "файлів", 'm'}},
        {"товар", {"товар", "товари", "товарів", 'm'}},
        {"товари", {"товар", "товари", "товарів", 'm'}},
        {"товарів", {"товар", "товари", "товарів", 'm'}},
        {"учасник", {"учасник", "учасники", "учасників", 'm'}},
        {"учасники", {"учасник", "учасники", "учасників", 'm'}},
        {"учасників", {"учасник", "учасники", "учасників", 'm'}},
        {"день", {"день", "дні", "днів", 'm'}},
        {"дні", {"день", "дні", "днів", 'm'}},
        {"днів", {"день", "дні", "днів", 'm'}},
        {"тиждень", {"тиждень", "тижні", "тижнів", 'm'}},
        {"тижні", {"тиждень", "тижні", "тижнів", 'm'}},
        {"тижнів", {"тиждень", "тижні", "тижнів", 'm'}},
        {"місяць", {"місяць", "місяці", "місяців", 'm'}},
        {"місяці", {"місяць", "місяці", "місяців", 'm'}},
        {"місяців", {"місяць", "місяці", "місяців", 'm'}},
        {"заявка", {"заявка", "заявки", "заявок", 'f'}},
        {"заявки", {"заявка", "заявки", "заявок", 'f'}},
        {"заявок", {"заявка", "заявки", "заявок", 'f'}},
        {"спроба", {"спроба", "спроби", "спроб", 'f'}},
        {"спроби", {"спроба", "спроби", "спроб", 'f'}},
        {"спроб", {"спроба", "спроби", "спроб", 'f'}},
        {"людина", {"людина", "людини", "людей", 'f'}},
        {"людини", {"людина", "людини", "людей", 'f'}},
        {"людей", {"людина", "людини", "людей", 'f'}},
        {"особа", {"особа", "особи", "осіб", 'f'}},
        {"особи", {"особа", "особи", "осіб", 'f'}},
        {"осіб", {"особа", "особи", "осіб", 'f'}},
        {"дитина", {"дитина", "дитини", "дітей", 'f'}},
        {"дитини", {"дитина", "дитини", "дітей", 'f'}},
        {"дітей", {"дитина", "дитини", "дітей", 'f'}},
        {"місто", {"місто", "міста", "міст", 'n'}},
        {"міста", {"місто", "міста", "міст", 'n'}},
        {"міст", {"місто", "міста", "міст", 'n'}},
        {"село", {"село", "села", "сіл", 'n'}},
        {"села", {"село", "села", "сіл", 'n'}},
        {"сіл", {"село", "села", "сіл", 'n'}},
        {"питання", {"питання", "питання", "питань", 'n'}},
        {"питань", {"питання", "питання", "питань", 'n'}}};
    return map;
}

std::string number_words_for_gender(unsigned long long n, char gender)
{
    auto words = split_words(number_to_words(n));
    if (gender == 'f') {
        feminine_last(words);
    } else if (gender == 'n') {
        neuter_last(words);
    }
    return join(words);
}

const std::string& counted_noun_alt()
{
    static const std::string alt =
        "користувач(?:і|ів)?|документ(?:и|ів)?|файл(?:и|ів)?|товар(?:и|ів)?|учасник(?:и|ів)?|день|дні|днів|тиждень|"
        "тижні|тижнів|місяць|місяці|місяців|заявка|заявки|заявок|спроба|спроби|спроб|людина|людини|людей|особа|особи|"
        "осіб|дитина|дитини|дітей|місто|міста|міст|село|села|сіл|питання|питань";
    return alt;
}

bool prefers_many_after_genitive_number(unsigned long long n)
{
    const auto mod100 = n % 100;
    const auto mod10 = n % 10;
    return mod10 == 0 || mod10 >= 5 || (mod100 >= 11 && mod100 <= 14);
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

const std::unordered_map<std::string, std::string>& compound_prefix_forms()
{
    static const std::unordered_map<std::string, std::string> map = {{"один", "одно"},
                                                                     {"одна", "одно"},
                                                                     {"два", "дво"},
                                                                     {"дві", "дво"},
                                                                     {"три", "три"},
                                                                     {"чотири", "чотири"},
                                                                     {"п'ять", "п'яти"},
                                                                     {"шість", "шести"},
                                                                     {"сім", "семи"},
                                                                     {"вісім", "восьми"},
                                                                     {"дев'ять", "дев'яти"},
                                                                     {"десять", "десяти"},
                                                                     {"одинадцять", "одинадцяти"},
                                                                     {"дванадцять", "дванадцяти"},
                                                                     {"тринадцять", "тринадцяти"},
                                                                     {"чотирнадцять", "чотирнадцяти"},
                                                                     {"п'ятнадцять", "п'ятнадцяти"},
                                                                     {"шістнадцять", "шістнадцяти"},
                                                                     {"сімнадцять", "сімнадцяти"},
                                                                     {"вісімнадцять", "вісімнадцяти"},
                                                                     {"дев'ятнадцять", "дев'ятнадцяти"},
                                                                     {"двадцять", "двадцяти"},
                                                                     {"тридцять", "тридцяти"},
                                                                     {"сорок", "сорока"},
                                                                     {"п'ятдесят", "п'ятдесяти"},
                                                                     {"шістдесят", "шістдесяти"},
                                                                     {"сімдесят", "сімдесяти"},
                                                                     {"вісімдесят", "вісімдесяти"},
                                                                     {"дев'яносто", "дев'яносто"},
                                                                     {"сто", "сто"},
                                                                     {"двісті", "двохсот"},
                                                                     {"триста", "трьохсот"},
                                                                     {"чотириста", "чотирьохсот"},
                                                                     {"п'ятсот", "п'ятисот"},
                                                                     {"шістсот", "шестисот"},
                                                                     {"сімсот", "семисот"},
                                                                     {"вісімсот", "восьмисот"},
                                                                     {"дев'ятсот", "дев'ятисот"},
                                                                     {"тисяча", "тисячо"},
                                                                     {"тисячі", "тисячо"},
                                                                     {"тисяч", "тисячо"}};
    return map;
}

std::string normalize_compounds(std::string text)
{
    static const std::regex re(R"((^|[^\d])(\d+)-([^0-9A-Za-z\s,.;:!?()]+))");
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

std::string hours_words(int h)
{
    auto words = split_words(number_to_words(h));
    feminine_last(words);
    return join(words) + " " + plural(h, {"година", "години", "годин"});
}

std::string minutes_words(int m, const Forms& forms = {"хвилина", "хвилини", "хвилин"})
{
    auto words = split_words(number_to_words(m));
    feminine_last(words);
    return join(words) + " " + plural(m, forms);
}

std::string normalize_time(std::string text)
{
    text = ctre_sub<R"((^|[^\d:])(\d{1,2}):([0-5]\d):([0-5]\d)(?![\d:]))">(text, [](const auto& m) {
        return cap_string<1>(m) + hours_words(parse_int(cap<2>(m))) + " " + minutes_words(parse_int(cap<3>(m))) + " " +
               minutes_words(parse_int(cap<4>(m)), {"секунда", "секунди", "секунд"});
    });
    text = ctre_sub<R"((^|[^А-Яа-яЄєІіЇїҐґ\d])(\d{1,2})(?:-|–|—)?(?:й|ій|а|ої)(?![А-Яа-яЄєІіЇїҐґ]))">(
        text, [](const auto& m) {
            return cap_string<1>(m) + number_to_ordinal_words(parse_ull(cap<2>(m)), "nom_f") + " година";
        });
    text = ctre_sub<R"((^|[^\d:])(\d{1,2}):([0-5]\d)\s+(ранку|дня|вечора|ночі)(?![А-Яа-яЄєІіЇїҐґ\d:]))">(
        text, [](const auto& m) {
            const int h = parse_int(cap<2>(m));
            const int mn = parse_int(cap<3>(m));
            std::string out = cap_string<1>(m) + hours_words(h);
            if (mn) {
                out += " " + minutes_words(mn);
            }
            return out + " " + cap_string<4>(m);
        });
    return ctre_sub<R"((^|[^\d:])(\d{1,2}):([0-5]\d)(?![\d:]))">(text, [](const auto& m) {
        const int h = parse_int(cap<2>(m));
        const int mn = parse_int(cap<3>(m));
        std::string out = cap_string<1>(m) + hours_words(h);
        if (mn) {
            out += " " + minutes_words(mn);
        }
        return out;
    });
}

std::string say_fraction(unsigned long long num, unsigned long long den)
{
    auto words = split_words(number_to_words(num));
    feminine_last(words);
    const bool singular = num % 10 == 1 && num % 100 != 11;
    return join(words) + " " + number_to_ordinal_words(den, singular ? "nom_f" : "pl");
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

std::string read_measurement_quantity(std::string_view num, const Measurement& meas)
{
    const auto pos = num.find_first_of(".,");
    if (pos != std::string_view::npos) {
        auto words = decimal_to_words(num.substr(0, pos), num.substr(pos + 1));
        return words.empty() ? std::string(num) : words + " " + std::string(meas.few);
    }
    const auto n = try_parse_ull(num);
    if (!n) {
        return number_to_words_digit_by_digit(num) + " " + std::string(meas.many);
    }
    auto words = split_words(number_to_words(*n));
    if (meas.gender == 'f') {
        feminine_last(words);
    }
    return join(words) + " " + plural(*n, {meas.one, meas.few, meas.many});
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

std::string normalize_symbols(std::string text)
{
    static const std::vector<std::pair<std::string, std::string>> symbols = {{"°C", "градусів цельсія"},
                                                                             {"°С", "градусів цельсія"},
                                                                             {"°F", "градусів фаренгейта"},
                                                                             {"±", "плюс мінус"},
                                                                             {"≈", "приблизно дорівнює"},
                                                                             {"≠", "не дорівнює"},
                                                                             {"≤", "менше або дорівнює"},
                                                                             {"≥", "більше або дорівнює"},
                                                                             {"×", "помножити на"},
                                                                             {"÷", "поділити на"},
                                                                             {"=", "дорівнює"},
                                                                             {"<", "менше"},
                                                                             {">", "більше"},
                                                                             {"‰", "проміле"},
                                                                             {"§", "параграф"},
                                                                             {"₿", "біткоїн"},
                                                                             {"•", " "},
                                                                             {"·", " "},
                                                                             {"~", "тильда"},
                                                                             {"&", "і"},
                                                                             {"#", "решітка"},
                                                                             {"_", "нижнє підкреслення"},
                                                                             {"²", "у квадраті"},
                                                                             {"³", "у кубі"},
                                                                             {"№", "номер"}};
    for (const auto& [sym, word] : symbols) {
        replace_all(text, sym, " " + word + " ");
    }
    return trim_spaces(std::move(text));
}

std::string normalize_math(std::string text)
{
    return ctre_sub<R"((\d)\s*\+\s*(?=\d))">(text, [](const auto& m) { return cap_string<1>(m) + " плюс "; });
}

std::string normalize_decimals(std::string text)
{
    return ctre_sub<R"(\b(\d+),(\d+)\b)">(text, [](const auto& m) {
        const auto ip = cap_string<1>(m);
        const auto fp = cap_string<2>(m);
        if (fp.find_first_not_of('0') == std::string::npos) {
            return number_digits_or_words(ip);
        }
        auto words = decimal_to_words(ip, fp);
        return words.empty() ? number_digits_or_words(ip) + " кома " + number_to_words_digit_by_digit(fp) : words;
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

std::string normalize_symbol_currency(std::string text)
{
    static const std::unordered_map<std::string, std::string> genpl = {{"грн", "гривень"},
                                                                       {"UAH", "гривень"},
                                                                       {"₴", "гривень"},
                                                                       {"$", "доларів"},
                                                                       {"USD", "доларів"},
                                                                       {"€", "євро"},
                                                                       {"EUR", "євро"},
                                                                       {"£", "фунтів"},
                                                                       {"GBP", "фунтів"}};
    text = regex_sub(text, symbol_currency_prefix_re(), [&](const std::smatch& m) {
        return m[2].str() + " " + m[3].str() + " " + genpl.at(m[1].str());
    });
    return regex_sub(text, symbol_currency_suffix_re(), [&](const std::smatch& m) {
        return m[1].str() + " " + m[2].str() + " " + genpl.at(m[3].str());
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

std::string normalize_currency(std::string text)
{
    struct Currency {
        Forms main;
        bool main_fem = false;
        Forms sub;
        bool sub_fem = false;
        std::vector<std::regex> patterns;
    };
    static const std::vector<Currency> currencies = {
        {{"гривня", "гривні", "гривень"},
         true,
         {"копійка", "копійки", "копійок"},
         true,
         {std::regex(R"(((?:\d+[.,]\d\d|\d+)(?!\d|[.,]\d))\s*(грн(ив(ень|ні|ня))?(?![а-яіїєґ])|₴))"),
          std::regex(R"(((?:\d+[.,]\d\d|\d+)(?!\d|[.,]\d))\s*UAH)"),
          std::regex(R"(₴\s*((?:\d+[.,]\d\d|\d+)(?!\d|[.,]\d)))"),
          std::regex(R"((\d+)\s*₴)")}},
        {{"долар", "долари", "доларів"},
         false,
         {"цент", "центи", "центів"},
         false,
         {std::regex(R"(((?:\d+[.,]\d\d|\d+)(?!\d|[.,]\d))\s*(долар(ів|и|а)?(?![а-яіїєґ])|\$))"),
          std::regex(R"(((?:\d+[.,]\d\d|\d+)(?!\d|[.,]\d))\s*USD)"),
          std::regex(R"(\$\s*((?:\d+[.,]\d\d|\d+)(?!\d|[.,]\d)))")}},
        {{"євро", "євро", "євро"},
         false,
         {"євроцент", "євроценти", "євроцентів"},
         false,
         {std::regex(R"(((?:\d+[.,]\d\d|\d+)(?!\d|[.,]\d))\s*(євро(?![а-яіїєґ])|€))"),
          std::regex(R"(((?:\d+[.,]\d\d|\d+)(?!\d|[.,]\d))\s*EUR)"),
          std::regex(R"(€\s*((?:\d+[.,]\d\d|\d+)(?!\d|[.,]\d)))"),
          std::regex(R"((\d+)\s*€)")}},
        {{"фунт", "фунти", "фунтів"},
         false,
         {"пенс", "пенси", "пенсів"},
         false,
         {std::regex(R"(((?:\d+[.,]\d\d|\d+)(?!\d|[.,]\d))\s*(фунт(ів|и|а)?(?![а-яіїєґ])|£))"),
          std::regex(R"(((?:\d+[.,]\d\d|\d+)(?!\d|[.,]\d))\s*GBP)"),
          std::regex(R"(£\s*((?:\d+[.,]\d\d|\d+)(?!\d|[.,]\d)))")}}};
    auto amount_words = [](std::string amount_text, const Currency& c) {
        replace_all(amount_text, " ", "");
        const auto pos = amount_text.find_first_of(".,");
        const auto main = parse_ull(pos == std::string::npos ? std::string_view(amount_text)
                                                             : std::string_view(amount_text).substr(0, pos));
        unsigned sub = 0;
        if (pos != std::string::npos) {
            auto frac = amount_text.substr(pos + 1);
            if (frac.size() == 1) {
                frac.push_back('0');
            }
            if (frac.size() >= 2) {
                sub = static_cast<unsigned>(parse_ull(std::string_view(frac).substr(0, 2)));
            }
        }
        auto main_words = split_words(number_to_words(main));
        if (c.main_fem) {
            feminine_last(main_words);
        }
        std::string out = join(main_words) + " " + plural(main, c.main);
        if (sub > 0) {
            auto sub_words = split_words(number_to_words(sub));
            if (c.sub_fem) {
                feminine_last(sub_words);
            }
            out += " " + join(sub_words) + " " + plural(sub, c.sub);
        }
        return out;
    };
    for (const auto& c : currencies) {
        for (const auto& re : c.patterns) {
            bool replaced = false;
            text = regex_sub(text, re, [&](const std::smatch& m) {
                if (replaced) {
                    return m.str();
                }
                replaced = true;
                return amount_words(m[1].str(), c);
            });
        }
    }
    return text;
}

struct FinanceUnit {
    Forms forms;
    bool feminine = false;
};

const std::unordered_map<std::string, FinanceUnit>& finance_units()
{
    static const std::unordered_map<std::string, FinanceUnit> map = {
        {"BTC", {{"біткоїн", "біткоїни", "біткоїнів"}, false}},
        {"ETH", {{"ефір", "ефіри", "ефірів"}, false}},
        {"USDT", {{"тезер", "тезери", "тезерів"}, false}},
        {"BNB", {{"бі ен бі", "бі ен бі", "бі ен бі"}, false}},
        {"UAH", {{"гривня", "гривні", "гривень"}, true}},
        {"USD", {{"долар США", "долари США", "доларів США"}, false}},
        {"EUR", {{"євро", "євро", "євро"}, false}},
        {"GBP", {{"фунт стерлінгів", "фунти стерлінгів", "фунтів стерлінгів"}, false}},
    };
    return map;
}

std::string finance_unit_many(std::string_view ticker)
{
    const auto it = finance_units().find(std::string(ticker));
    return it == finance_units().end() ? std::string(ticker) : std::string(it->second.forms[2]);
}

std::string finance_amount_words(std::string amount, const FinanceUnit& unit)
{
    replace_all(amount, " ", "");
    const auto pos = amount.find_first_of(".,");
    if (pos != std::string::npos) {
        auto words =
            decimal_to_words(std::string_view(amount).substr(0, pos), std::string_view(amount).substr(pos + 1));
        return words.empty() ? amount : words + " " + std::string(unit.forms[2]);
    }
    const auto n = parse_ull(amount);
    auto words = split_words(number_to_words(n));
    if (unit.feminine) {
        feminine_last(words);
    }
    return join(words) + " " + plural(n, unit.forms);
}

std::string normalize_finance(std::string text)
{
    text = ctre_sub<R"(\b(BTC|ETH|USDT|BNB|USD|EUR|GBP|UAH)/(BTC|ETH|USDT|BNB|USD|EUR|GBP|UAH)\b)">(
        text, [](const auto& m) { return finance_unit_many(cap<1>(m)) + " до " + finance_unit_many(cap<2>(m)); });
    return ctre_sub<R"((^|[^\wА-Яа-яЄєІіЇїҐґ])(\d+(?:[.,]\d+)?)\s*(BTC|ETH|USDT|BNB)\b)">(text, [](const auto& m) {
        const auto& unit = finance_units().at(cap_string<3>(m));
        return cap_string<1>(m) + finance_amount_words(cap_string<2>(m), unit);
    });
}

std::string normalize_phone_number(std::string_view phone, PhoneStyle style)
{
    std::string digits;
    for (char ch : phone) {
        if (ch >= '0' && ch <= '9') {
            digits.push_back(ch);
        }
    }
    if (digits.size() == 10 && digits[0] == '0') {
        digits = "38" + digits;
    }
    if (digits.size() != 12 || !digits.starts_with("380")) {
        return std::string(phone);
    }
    if (style == PhoneStyle::DigitByDigit) {
        return "плюс " + number_to_words_digit_by_digit(digits);
    }
    std::vector<std::string> parts = {"плюс", "триста вісімдесят"};
    const std::array<std::string, 4> segs = {
        digits.substr(3, 2), digits.substr(5, 3), digits.substr(8, 2), digits.substr(10, 2)};
    for (const auto& seg : segs) {
        if (seg.size() > 1 && seg[0] == '0') {
            parts.push_back(number_to_words_digit_by_digit(seg));
        } else {
            parts.push_back(number_to_words(parse_ull(seg)));
        }
    }
    return join(parts);
}

std::string normalize_text_with_phone_numbers(std::string text, PhoneStyle style)
{
    return ctre_sub<R"((^|[^\d])((?:\+?380|0)\s*\(?\d{2}\)?[\-\s]?\d{3}[\-\s]?\d{2}[\-\s]?\d{2})(?!\d))">(
        text, [&](const auto& m) { return cap_string<1>(m) + normalize_phone_number(cap<2>(m), style); });
}

std::string spell_identifier_letters(std::string_view letters)
{
    static const std::unordered_map<char, std::string_view> latin = {
        {'A', "ей"},  {'B', "бі"},     {'C', "сі"},   {'D', "ді"},  {'E', "і"},  {'F', "еф"}, {'G', "джі"},
        {'H', "ейч"}, {'I', "ай"},     {'J', "джей"}, {'K', "кей"}, {'L', "ел"}, {'M', "ем"}, {'N', "ен"},
        {'O', "оу"},  {'P', "пі"},     {'Q', "к'ю"},  {'R', "ар"},  {'S', "ес"}, {'T', "ті"}, {'U', "ю"},
        {'V', "ві"},  {'W', "дабл ю"}, {'X', "екс"},  {'Y', "вай"}, {'Z', "зед"}};
    std::vector<std::string> parts;
    for (std::size_t i = 0; i < letters.size();) {
        std::size_t next = i + 1;
        const auto cp = decode_one(letters, i, next);
        if (cp < 128) {
            const auto ch = static_cast<char>(cp >= 'a' && cp <= 'z' ? cp - 32 : cp);
            if (const auto it = latin.find(ch); it != latin.end()) {
                parts.emplace_back(it->second);
            }
        } else {
            std::string letter;
            append_utf8(letter, upper_cp(cp));
            if (const auto it = pronunciation_map().find(letter); it != pronunciation_map().end()) {
                parts.push_back(it->second);
            }
        }
        i = next;
    }
    return join(parts);
}

std::string read_identifier_number(std::string_view digits)
{
    if (digits.size() > 4 || (digits.size() > 1 && digits[0] == '0')) {
        return number_to_words_digit_by_digit(digits);
    }
    return number_to_words(parse_ull(digits));
}

std::optional<std::string> read_roman_identifier_segment(std::string_view letters)
{
    std::string roman;
    for (char ch : letters) {
        if (ch >= 'a' && ch <= 'z') {
            ch = static_cast<char>(ch - 32);
        }
        if (ch != 'I' && ch != 'V' && ch != 'X' && ch != 'L' && ch != 'C' && ch != 'D' && ch != 'M') {
            return std::nullopt;
        }
        roman.push_back(ch);
    }
    if (roman.empty() || !valid_roman(roman)) {
        return std::nullopt;
    }
    return number_to_ordinal_words(static_cast<unsigned long long>(roman_to_int(roman)), "nom_m");
}

std::string read_identifier_segment(std::string_view segment)
{
    std::vector<std::string> parts;
    std::string digits;
    std::string letters;
    auto flush_digits = [&] {
        if (!digits.empty()) {
            parts.push_back(read_identifier_number(digits));
            digits.clear();
        }
    };
    auto flush_letters = [&] {
        if (!letters.empty()) {
            if (auto roman = read_roman_identifier_segment(letters)) {
                parts.push_back(*roman);
            } else {
                parts.push_back(spell_identifier_letters(letters));
            }
            letters.clear();
        }
    };
    for (std::size_t i = 0; i < segment.size();) {
        const auto ch = segment[i];
        if (ch >= '0' && ch <= '9') {
            flush_letters();
            digits.push_back(ch);
            ++i;
            continue;
        }
        std::size_t next = i + 1;
        const auto cp = decode_one(segment, i, next);
        if ((cp >= U'A' && cp <= U'Z') || (cp >= U'a' && cp <= U'z') || is_uk(cp)) {
            flush_digits();
            letters.append(segment.substr(i, next - i));
        } else {
            flush_digits();
            flush_letters();
        }
        i = next;
    }
    flush_digits();
    flush_letters();
    return join(parts);
}

std::string read_structured_identifier(std::string_view value)
{
    std::vector<std::string> parts;
    std::string current;
    auto flush = [&] {
        if (current.empty()) {
            return;
        }
        parts.push_back(read_identifier_segment(current));
        current.clear();
    };
    for (std::size_t i = 0; i < value.size();) {
        const auto ch = value[i];
        if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) {
            current.push_back(ch);
            ++i;
            continue;
        }
        std::size_t next = i + 1;
        const auto cp = decode_one(value, i, next);
        if (is_uk(cp)) {
            current.append(value.substr(i, next - i));
            i = next;
            continue;
        }
        flush();
        if (cp == U'/') {
            parts.emplace_back("слеш");
        } else if (cp == U'-' || cp == U'‑' || cp == U'–' || cp == U'—') {
            parts.emplace_back("дефіс");
        }
        i = next;
    }
    flush();
    return join(parts);
}

std::string normalize_identifiers(std::string text)
{
    static const std::regex iban(R"(\bUA\s*(\d{2})(?:\s*(\d{4})){6}\s*(\d{1})\b)", std::regex::icase);
    static const std::regex edrpou(R"((ЄДРПОУ|ЄДР|код\s+ЄДРПОУ)\s*[:№#]?\s*(\d{8})\b)", std::regex::icase);
    static const std::regex tax_id(R"((РНОКПП|ІПН|податковий\s+номер)\s*[:№#]?\s*(\d{10})\b)", std::regex::icase);
    static const std::regex postcode(R"((індекс|поштовий\s+індекс)\s*[:№#]?\s*(\d{5})\b)", std::regex::icase);
    static const std::regex legal_number(
        R"((^|[^A-Za-zА-Яа-яЄєІіЇїҐґ])((?:справа|Справа|справі|Справі|справу|Справу|провадження|Провадження|закон|Закон|закону|Закону|наказ|Наказ|постанова|Постанова|розпорядження|Розпорядження|рішення|Рішення|ухвала|Ухвала|договір|Договір|контракт|Контракт|рахунок|Рахунок|замовлення|Замовлення|акт|Акт|лист|Лист)(?:\s+[^№\s]+)?\s*)№\s*([^\s,.;:!?()]+))");
    static const std::regex erdr(R"((^|[^A-Za-zА-Яа-яЄєІіЇїҐґ])(ЄРДР\.?\s*)№?\s*(\d{8,20})(?!\d))", std::regex::icase);
    static const std::regex passport(R"((^|[^A-Za-zА-Яа-яЄєІіЇїҐґ])(паспорт\s+)([^\s\d]+)\s*(\d{6,9})(?!\d))",
                                     std::regex::icase);
    static const std::regex bank_card(
        R"((^|[^A-Za-zА-Яа-яЄєІіЇїҐґ])((?:картка|картку|карта|карту)\s+)(\d{4})[\s-]+(?:\*{4}|xxxx|XXXX)[\s-]+(?:\*{4}|xxxx|XXXX)[\s-]+(\d{4})(?!\d))",
        std::regex::icase);
    static const std::regex full_bank_card(
        R"((^|[^A-Za-zА-Яа-яЄєІіЇїҐґ])((?:картка|картку|карта|карту)\s+)(\d{4})[\s-]+(\d{4})[\s-]+(\d{4})[\s-]+(\d{4})(?!\d))",
        std::regex::icase);
    static const std::regex plate(
        R"((^|[\s,.;:!?()])((?:А|В|Е|І|К|М|Н|О|Р|С|Т|Х){2})\s*(\d{4})\s*((?:А|В|Е|І|К|М|Н|О|Р|С|Т|Х){2})(?![А-Яа-яЄєІіЇїҐґ]))");
    text = regex_sub(text, iban, [](const std::smatch& m) {
        std::string digits;
        for (char ch : m.str()) {
            if (ch >= '0' && ch <= '9') {
                digits.push_back(ch);
            }
        }
        if (digits.size() != 27) {
            return m.str();
        }
        return "айбан " + spell_identifier_letters("UA") + " " + number_to_words_digit_by_digit(digits);
    });
    text = regex_sub(text, edrpou, [](const std::smatch& m) {
        return "єдиний державний реєстр підприємств та організацій України " +
               number_to_words_digit_by_digit(m[2].str());
    });
    text = regex_sub(text, tax_id, [](const std::smatch& m) {
        return lower_text(m[1].str()) + " " + number_to_words_digit_by_digit(m[2].str());
    });
    text = regex_sub(text, postcode, [](const std::smatch& m) {
        return lower_text(m[1].str()) + " " + number_to_words_digit_by_digit(m[2].str());
    });
    text = regex_sub(text, legal_number, [](const std::smatch& m) {
        return m[1].str() + m[2].str() + "номер " + read_structured_identifier(m[3].str());
    });
    text = regex_sub(text, erdr, [](const std::smatch& m) {
        return m[1].str() + "єдиний реєстр досудових розслідувань номер " + number_to_words_digit_by_digit(m[3].str());
    });
    text = regex_sub(text, passport, [](const std::smatch& m) {
        return m[1].str() + m[2].str() + spell_identifier_letters(m[3].str()) + " " +
               number_to_words_digit_by_digit(m[4].str());
    });
    text = regex_sub(text, bank_card, [](const std::smatch& m) {
        return m[1].str() + m[2].str() + number_to_words_digit_by_digit(m[3].str()) + " зірочки зірочки " +
               number_to_words_digit_by_digit(m[4].str());
    });
    text = regex_sub(text, full_bank_card, [](const std::smatch& m) {
        return m[1].str() + m[2].str() + number_to_words_digit_by_digit(m[3].str()) + " " +
               number_to_words_digit_by_digit(m[4].str()) + " " + number_to_words_digit_by_digit(m[5].str()) + " " +
               number_to_words_digit_by_digit(m[6].str());
    });
    return regex_sub(text, plate, [](const std::smatch& m) {
        return m[1].str() + "номерний знак " + spell_identifier_letters(m[2].str()) + " " +
               number_to_words_digit_by_digit(m[3].str()) + " " + spell_identifier_letters(m[4].str());
    });
}

std::string read_dotted(std::string_view num)
{
    static constexpr std::array<std::string_view, 10> digit_words = {
        "нуль", "один", "два", "три", "чотири", "п'ять", "шість", "сім", "вісім", "дев'ять"};
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= num.size()) {
        const auto pos = num.find('.', start);
        const auto p = num.substr(start, pos == std::string_view::npos ? std::string_view::npos : pos - start);
        if (p.size() > 1 && p[0] == '0') {
            std::string s;
            for (char ch : p) {
                if (!s.empty()) {
                    s += " ";
                }
                s += digit_words[ch - '0'];
            }
            parts.push_back(s);
        } else {
            parts.push_back(number_digits_or_words(p));
        }
        if (pos == std::string_view::npos) {
            break;
        }
        start = pos + 1;
    }
    return join(parts, " крапка ");
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

const std::unordered_map<std::string, std::string>& english_words()
{
    static const std::unordered_map<std::string, std::string> map = {{"google", "гугл"},
                                                                     {"python", "пайтон"},
                                                                     {"ios", "айос"},
                                                                     {"iphone", "айфон"},
                                                                     {"windows", "віндовс"},
                                                                     {"microsoft", "майкрософт"},
                                                                     {"apple", "епл"},
                                                                     {"facebook", "фейсбук"},
                                                                     {"youtube", "ютуб"},
                                                                     {"twitter", "твітер"},
                                                                     {"instagram", "інстаграм"},
                                                                     {"telegram", "телеграм"},
                                                                     {"whatsapp", "вотсап"},
                                                                     {"skype", "скайп"},
                                                                     {"android", "андроїд"},
                                                                     {"samsung", "самсунг"},
                                                                     {"linux", "лінукс"},
                                                                     {"internet", "інтернет"},
                                                                     {"online", "онлайн"},
                                                                     {"email", "імейл"},
                                                                     {"ok", "окей"},
                                                                     {"openai", "опеней"},
                                                                     {"chatgpt", "чатджипіті"},
                                                                     {"github", "гітхаб"},
                                                                     {"gitlab", "гітлаб"},
                                                                     {"macbook", "макбук"},
                                                                     {"ipad", "айпад"},
                                                                     {"airpods", "еірподс"},
                                                                     {"gmail", "джимейл"},
                                                                     {"zoom", "зум"},
                                                                     {"slack", "слак"},
                                                                     {"docker", "докер"},
                                                                     {"kubernetes", "кубернетіс"},
                                                                     {"react", "ріакт"},
                                                                     {"node", "ноуд"},
                                                                     {"javascript", "джаваскрипт"},
                                                                     {"typescript", "тайпскрипт"}};
    return map;
}

std::string normalize_english(std::string text)
{
    static const std::regex word(R"(\b[A-Za-z][A-Za-z'’-]*\b)");
    static const std::regex acronym(R"(\b[A-Z]{2,6}\b)");
    static const std::unordered_map<char, std::string> names = {
        {'a', "ей"},  {'b', "бі"},     {'c', "сі"},   {'d', "ді"},  {'e', "і"},  {'f', "еф"}, {'g', "джі"},
        {'h', "ейч"}, {'i', "ай"},     {'j', "джей"}, {'k', "кей"}, {'l', "ел"}, {'m', "ем"}, {'n', "ен"},
        {'o', "оу"},  {'p', "пі"},     {'q', "к'ю"},  {'r', "ар"},  {'s', "ес"}, {'t', "ті"}, {'u', "ю"},
        {'v', "ві"},  {'w', "дабл ю"}, {'x', "екс"},  {'y', "вай"}, {'z', "зед"}};
    text = regex_sub(text, word, [](const std::smatch& m) {
        const auto low = lower_text(m.str());
        if (const auto it = english_words().find(low); it != english_words().end()) {
            return it->second;
        }
        return m.str();
    });
    return regex_sub(text, acronym, [&](const std::smatch& m) {
        const auto low = lower_text(m.str());
        if (english_words().contains(low)) {
            return m.str();
        }
        std::vector<std::string> parts;
        for (char ch : low) {
            parts.push_back(names.at(ch));
        }
        return join(parts);
    });
}

} // namespace

std::string number_to_words_digit_by_digit(std::string_view digits)
{
    static constexpr std::array<std::string_view, 10> words = {
        "нуль", "один", "два", "три", "чотири", "п'ять", "шість", "сім", "вісім", "дев'ять"};
    std::string out;
    for (char ch : digits) {
        if (ch < '0' || ch > '9') {
            continue;
        }
        if (!out.empty()) {
            out += " ";
        }
        out += words[ch - '0'];
    }
    return out;
}

std::string number_to_words(unsigned long long n)
{
    if (n == 0) {
        return "нуль";
    }
    if (n >= 1'000'000'000'000'000'000ULL) {
        return number_to_words_digit_by_digit(std::to_string(n));
    }
    struct Scale {
        unsigned long long value;
        Forms forms;
        bool feminine;
    };
    static constexpr std::array<Scale, 5> scales = {
        {{1'000'000'000'000'000ULL, {"квадрильйон", "квадрильйони", "квадрильйонів"}, false},
         {1'000'000'000'000ULL, {"трильйон", "трильйони", "трильйонів"}, false},
         {1'000'000'000ULL, {"мільярд", "мільярди", "мільярдів"}, false},
         {1'000'000ULL, {"мільйон", "мільйони", "мільйонів"}, false},
         {1'000ULL, {"тисяча", "тисячі", "тисяч"}, true}}};
    std::vector<std::string> words;
    for (const auto& scale : scales) {
        const auto count = static_cast<unsigned>((n / scale.value) % 1000);
        if (!count) {
            continue;
        }
        auto chunk = under_thousand(count);
        if (scale.feminine) {
            feminine_last(chunk);
            if (count == 1) {
                chunk.pop_back();
            }
        }
        words.insert(words.end(), chunk.begin(), chunk.end());
        words.push_back(plural(count, scale.forms));
    }
    auto rest = under_thousand(static_cast<unsigned>(n % 1000));
    words.insert(words.end(), rest.begin(), rest.end());
    return join(words);
}

std::string number_to_ordinal_words(unsigned long long n, std::string_view form)
{
    auto words = split_words(number_to_words(n));
    if (words.empty()) {
        return {};
    }
    if (const auto it = cardinal_to_ordinal().find(words.back()); it != cardinal_to_ordinal().end()) {
        words.back() = inflect_ordinal(it->second, form);
    } else {
        words.back() = inflect_ordinal(words.back(), form);
    }
    return join(words);
}

std::string number_to_words_case(unsigned long long n, std::string_view grammatical_case)
{
    static const std::unordered_map<std::string_view, std::size_t> cases = {
        {"gen", 0}, {"dat", 1}, {"instr", 2}, {"prep", 3}};
    const auto idx = cases.at(grammatical_case);
    auto words = split_words(number_to_words(n));
    for (auto& w : words) {
        if (const auto it = case_forms().find(w); it != case_forms().end()) {
            w = it->second[idx];
        }
    }
    return join(words);
}

std::string normalize_abbreviations(std::string_view text)
{
    static const std::vector<std::string> keys = {"див. також",
                                                  "до н. е.",
                                                  "і т. д.",
                                                  "і т. ін.",
                                                  "пн. ш.",
                                                  "пд. ш.",
                                                  "сх. д.",
                                                  "зх. д.",
                                                  "грн.",
                                                  "коп.",
                                                  "н. е.",
                                                  "т. б.",
                                                  "рр.",
                                                  "р-н",
                                                  "б/в",
                                                  "та ін."};
    std::string out;
    for (std::size_t i = 0; i < text.size();) {
        bool matched = false;
        for (const auto& key : keys) {
            std::size_t pos = i;
            std::size_t kpos = 0;
            while (kpos < key.size()) {
                if (key[kpos] == ' ') {
                    while (pos < text.size() && text[pos] == ' ') {
                        ++pos;
                    }
                    ++kpos;
                } else if (key[kpos] == '.') {
                    if (pos >= text.size() || text[pos] != '.') {
                        break;
                    }
                    ++pos;
                    ++kpos;
                    if (kpos < key.size()) {
                        while (pos < text.size() && text[pos] == ' ') {
                            ++pos;
                        }
                    }
                } else {
                    std::size_t next_key = kpos + 1;
                    std::size_t next_text = pos + 1;
                    const auto kc = lower_cp(decode_one(key, kpos, next_key));
                    const auto tc = pos < text.size() ? lower_cp(decode_one(text, pos, next_text)) : U'\0';
                    if (kc != tc) {
                        break;
                    }
                    kpos = next_key;
                    pos = next_text;
                }
            }
            if (kpos == key.size()) {
                out += abbreviation_map().at(compact_spaces_lower(std::string_view(text).substr(i, pos - i)));
                i = pos;
                matched = true;
                break;
            }
        }
        if (!matched) {
            std::size_t next = i + 1;
            decode_one(text, i, next);
            out.append(text.substr(i, next - i));
            i = next;
        }
    }
    return out;
}

std::string expand_abbreviations(std::string_view text)
{
    static const std::u32string_view vowels = U"АЕЄИІЇОУЮЯ";
    std::string out;
    for (std::size_t i = 0; i < text.size();) {
        std::size_t next = i + 1;
        const auto cp = decode_one(text, i, next);
        if (!is_upper_uk(cp)) {
            out.append(text.substr(i, next - i));
            i = next;
            continue;
        }
        const auto start = i;
        std::size_t count = 0;
        while (i < text.size()) {
            std::size_t n = i + 1;
            if (!is_upper_uk(decode_one(text, i, n))) {
                break;
            }
            i = n;
            ++count;
        }
        const auto token = std::string(text.substr(start, i - start));
        if (count < 2) {
            out += token;
            continue;
        }
        bool has_vowel = false;
        for (const auto& cp : codepoints(token)) {
            if (vowels.contains(cp.value)) {
                has_vowel = true;
                break;
            }
        }
        if (has_vowel) {
            out += token;
            continue;
        }
        std::vector<std::string> parts;
        for (const auto& cp : codepoints(token)) {
            std::string letter;
            append_utf8(letter, cp.value);
            if (const auto it = pronunciation_map().find(letter); it != pronunciation_map().end()) {
                parts.push_back(it->second);
            }
        }
        out += join(parts);
    }
    return out;
}

std::string cyrilize(std::string_view text)
{
    std::string out;
    for (std::size_t i = 0; i < text.size();) {
        std::size_t next = i + 1;
        auto cp = decode_one(text, i, next);
        if (cp < 128 && ((cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z'))) {
            std::string tri;
            if (i + 2 < text.size()) {
                tri = lower_text(text.substr(i, 3));
            }
            if (const auto it = cyr_map().find(tri); it != cyr_map().end()) {
                out += it->second;
                i += 3;
                continue;
            }
            std::string di;
            if (i + 1 < text.size()) {
                di = lower_text(text.substr(i, 2));
            }
            if (const auto it = cyr_map().find(di); it != cyr_map().end()) {
                out += it->second;
                i += 2;
                continue;
            }
            const std::string one = lower_text(text.substr(i, 1));
            if (const auto it = cyr_map().find(one); it != cyr_map().end()) {
                out += it->second;
            } else {
                out.append(text.substr(i, next - i));
            }
            i = next;
        } else {
            out.append(text.substr(i, next - i));
            i = next;
        }
    }
    return out;
}

std::string cyrrilize(std::string_view text)
{
    return cyrilize(text);
}

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
        return options;
    case NormalizePreset::Conservative:
        options.expand_known_acronyms = false;
        options.spell_unknown_acronyms = false;
        options.normalize_english_words = false;
        options.transliterate_latin = false;
        options.symbol_style = SymbolStyle::Preserve;
        return options;
    case NormalizePreset::SearchIndexing:
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

    text = normalize_typography(std::move(text));
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
        text = normalize_number_groups(std::move(text));
        text = normalize_identifiers(std::move(text));
        if (contains_any(text, "-–—%") || contains_any_token(text, {" рр", "роки", "стор.", "с."})) {
            text = normalize_ranges(std::move(text), options.range_style);
        }
        text = normalize_dates(std::move(text), options.date_style);
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
            text = cyrilize(text);
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
        }
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
        R"((^|[^\dA-Za-zА-Яа-яЄєІіЇїҐґ])(\d+(?:[.,]\d+)?)\s*(PLN|CHF|CAD|AUD|JPY|CNY|₽|₺|zł|¥)(?![A-Za-zА-Яа-яЄєІіЇїҐґ]))",
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
    static const std::unordered_set<std::string> known_unit_words = {
        "грн", "коп", "uah",  "usd",  "eur", "gbp",  "btc",  "eth", "usdt", "bnb",
        "тис", "млн", "млрд", "трлн", "рік", "року", "році", "раз", "рази", "разів"};
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
        "близько", "понад", "менше", "більше", "від", "до", "із", "з", "без", "після", "к", "у", "в", "о", "об"};
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
    std::sort(spans.begin(), spans.end(), [](const auto& a, const auto& b) {
        return std::tie(a.start, a.stop) < std::tie(b.start, b.stop);
    });
    return spans;
}

} // namespace uktextnorm
