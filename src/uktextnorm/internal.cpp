#include "uktextnorm/uktextnorm.hpp"

#include "internal.hpp"

#include "generated/uktextnorm_lexicons.hpp"

namespace uktextnorm::detail {

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
    std::unordered_set<char32_t> candidates;
    for (std::size_t i = 0; i < chars.size();) {
        std::size_t next = i + 1;
        candidates.insert(decode_one(chars, i, next));
        i = next;
    }
    for (std::size_t i = 0; i < text.size();) {
        std::size_t next = i + 1;
        if (candidates.contains(decode_one(text, i, next))) {
            return true;
        }
        i = next;
    }
    return false;
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
    if (std::ranges::any_of(lexicon::kCurrencies, [&](const auto& entry) {
            return !entry.symbol.empty() && text.find(entry.symbol) != std::string_view::npos;
        })) {
        return true;
    }
    const auto lowered = lower_text(text);
    if (contains_any_token(lowered,
                           {"грн",
                            "долар",
                            "євро",
                            "фунт",
                            "злот",
                            "франк",
                            "єн",
                            "юан",
                            "крон",
                            "лір",
                            "руп",
                            "рубл",
                            "вон",
                            "реал",
                            "ранд"})) {
        return true;
    }
    return std::ranges::any_of(lexicon::kCurrencies, [&](const auto& entry) {
        return lowered.find(lower_text(entry.code)) != std::string::npos;
    });
}

bool has_symbol_candidate(std::string_view text)
{
    return contains_any_token(text, {"°", "℃", "℉", "K", "±", "≈", "≠", "≤", "≥", "×", "÷", "=", "<",
                                     ">", "‰", "§", "₿", "•", "·", "~", "&", "#", "_", "²", "³", "№"});
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

std::string join(const std::vector<std::string>& words, std::string_view sep)
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
    static const std::unordered_map<std::size_t, std::array<std::string_view, 2>> places = {
        {1, {"десята", "десятих"}},
        {2, {"сота", "сотих"}},
        {3, {"тисячна", "тисячних"}},
        {4, {"десятитисячна", "десятитисячних"}},
        {5, {"стотисячна", "стотисячних"}},
        {6, {"мільйонна", "мільйонних"}}};
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
    const bool singular_fraction = *frac_value % 10 == 1 && *frac_value % 100 != 11;
    return join(int_words) + " " + whole + " і " + join(frac_words) + " " +
           std::string(it->second[singular_fraction ? 0 : 1]);
}

std::string decimal_to_words_or_digits(std::string_view int_part, std::string_view frac_part)
{
    if (auto words = decimal_to_words(int_part, frac_part); !words.empty()) {
        return words;
    }
    return number_digits_or_words(int_part.empty() ? std::string_view("0") : int_part) + " кома " +
           number_to_words_digit_by_digit(frac_part);
}

const std::unordered_map<std::string, std::string>& abbreviation_map()
{
    static const std::unordered_map<std::string, std::string> map = [] {
        std::unordered_map<std::string, std::string> out;
        for (const auto& entry : lexicon::kAbbreviations) {
            out.emplace(compact_spaces_lower(entry.key), std::string(entry.expansion));
        }
        return out;
    }();
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

const std::unordered_map<std::string, std::string>& cyrillic_transliteration_map()
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
    static const std::unordered_map<std::string, Measurement> map = [] {
        std::unordered_map<std::string, Measurement> out;
        for (const auto& entry : lexicon::kUnits) {
            out.emplace(std::string(entry.key),
                        Measurement{entry.one, entry.few, entry.many, entry.decimal, entry.gender});
        }
        return out;
    }();
    return map;
}

std::string regex_alternation(std::vector<std::string> keys)
{
    std::sort(keys.begin(), keys.end(), [](const auto& a, const auto& b) { return a.size() > b.size(); });
    std::string out;
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (i) {
            out += "|";
        }
        for (const char ch : keys[i]) {
            if (std::string_view(R"(\-[]{}()*+?.,^$|# )").contains(ch)) {
                out.push_back('\\');
            }
            out.push_back(ch);
        }
    }
    return out;
}

const std::string& unit_alt()
{
    static const std::string alt = [] {
        std::vector<std::string> units;
        for (const auto& [unit, _] : measurements()) {
            units.push_back(unit);
        }
        return regex_alternation(std::move(units));
    }();
    return alt;
}

const std::string& counted_noun_alt();

const std::string& month_alt()
{
    static const std::string alt =
        "січня|січ\\.|лютого|лют\\.|березня|бер\\.|квітня|квіт\\.|травня|трав\\.|червня|черв\\.|липня|лип\\.|серпня|"
        "серп\\.|вересня|вер\\.|жовтня|жовт\\.|листопада|лист\\.|грудня|груд\\.|"
        "Січня|Лютого|Березня|Квітня|Травня|Червня|Липня|Серпня|Вересня|Жовтня|Листопада|Грудня";
    return alt;
}

const std::regex& date_day_range_re()
{
    static const std::regex re("\\b(\\d{1,2})\\s*(?:-|−|–|—)\\s*(\\d{1,2})\\s+(" + month_alt() +
                               R"()\s+(\d{3,4})(?:\s+року(?![А-Яа-яЄєІіЇїҐґ])|\s*р\.(?![а-яіїєґ]))?)");
    return re;
}

const std::regex& date_spelled_re()
{
    static const std::regex re("\\b(\\d{1,2})\\s+(" + month_alt() +
                                   R"()\s+(\d{3,4})(?:\s+року(?![А-Яа-яЄєІіЇїҐґ])|\s*р\.(?![а-яіїєґ]))?)",
                               std::regex::icase);
    return re;
}

const std::regex& case_prep_re()
{
    static const std::regex re(
        R"((^|[^А-Яа-яЄєІіЇїҐґ-])(Близько|близько|Після|після|Протягом|протягом|Впродовж|впродовж|Упродовж|упродовж|Менше|менше|Більше|більше|Серед|серед|Перед|перед|Між|між|Над|над|Під|під|При|при|Без|без|Від|від|До|до|Із|із|З|з|Об|об|К|к|О|о)\s+(\d+)(?:\s*()" +
        unit_alt() + R"()(\.?))?(?!\s*%)(?![\d.,:%–—-])(?![A-Za-zА-Яа-яЄєІіЇїҐґ]))");
    return re;
}

const std::regex& counted_ponad_re()
{
    static const std::regex re("(^|[^А-Яа-яЄєІіЇїҐґ\\d])(Понад|понад)\\s+([1-9]\\d{0,5})\\s+(" + counted_noun_alt() +
                               R"()(?![А-Яа-яЄєІіЇїҐґ]))");
    return re;
}

const std::regex& counted_genitive_re()
{
    static const std::regex re(
        "(^|[^А-Яа-яЄєІіЇїҐґ\\d])(Близько|близько|Більше|більше|Менше|менше|Серед|серед|До|до|Від|від|Без|"
        "без|Після|після|Протягом|протягом|Впродовж|впродовж|Упродовж|упродовж|Із|із)\\s+([1-9]\\d{0,5})\\s+(" +
        counted_noun_alt() + R"()(?![А-Яа-яЄєІіЇїҐґ]))");
    return re;
}

const std::regex& counted_nouns_re()
{
    static const std::regex re("(^|[^А-Яа-яЄєІіЇїҐґ\\d/])([1-9]\\d{0,5})\\s+(" + counted_noun_alt() +
                                   R"()(?![А-Яа-яЄєІіЇїҐґ]))",
                               std::regex::icase);
    return re;
}

const std::regex& measurements_re()
{
    static const std::regex re(R"((^|[^\d.,+\-])([+\-]?\d+(?:[.,]\d+)?)\s*()" + unit_alt() +
                               R"()(\.?)(?![A-Za-zА-Яа-яЄєІіЇїҐґ]))");
    return re;
}

const std::string& currency_token_alt()
{
    static const std::string alt = [] {
        std::vector<std::string> keys = {"грн"};
        for (const auto& entry : lexicon::kCurrencies) {
            keys.emplace_back(entry.code);
            if (!entry.symbol.empty()) {
                keys.emplace_back(entry.symbol);
            }
        }
        return "(?:" + regex_alternation(std::move(keys)) + ")";
    }();
    return alt;
}

const std::regex& symbol_currency_prefix_re()
{
    static const std::string token = R"((?:тис|млн|млрд|трлн)\.?)";
    static const std::regex re("(" + currency_token_alt() + ")\\s*(\\d+(?:[.,]\\d+)?)\\s*(" + token + ")");
    return re;
}

const std::regex& symbol_currency_suffix_re()
{
    static const std::string token = R"((?:тис|млн|млрд|трлн)\.?)";
    static const std::regex re("(\\d+(?:[.,]\\d+)?)\\s*(" + token + ")\\s*(" + currency_token_alt() + ")");
    return re;
}

const std::unordered_map<std::string, std::string>& cardinal_to_ordinal()
{
    static const std::unordered_map<std::string, std::string> map = {{"нуль", "нульовий"},
                                                                     {"один", "перший"},
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
                                                                              {"loc", "ому"},
                                                                              {"pl", "их"},
                                                                              {"loc_pl", "их"},
                                                                              {"acc_f", "у"},
                                                                              {"gen_f", "ої"},
                                                                              {"ins", "им"},
                                                                              {"ins_f", "ою"},
                                                                              {"ins_pl", "ими"},
                                                                              {"loc_f", "ій"}};
    // Soft-stem adjectives (третій) take softened endings.
    static const std::unordered_map<std::string, std::string_view> soft_endings = {{"nom_n", "є"},
                                                                                   {"nom_f", "я"},
                                                                                   {"gen", "ього"},
                                                                                   {"dat", "ьому"},
                                                                                   {"prep", "ьому"},
                                                                                   {"loc", "ьому"},
                                                                                   {"pl", "іх"},
                                                                                   {"loc_pl", "іх"},
                                                                                   {"acc_f", "ю"},
                                                                                   {"gen_f", "ьої"},
                                                                                   {"ins", "ім"},
                                                                                   {"ins_f", "ьою"},
                                                                                   {"ins_pl", "іми"}};
    const auto it = endings.find(std::string(form));
    if (it == endings.end()) {
        return stem;
    }
    bool soft = false;
    if (stem.ends_with("ій")) {
        soft = true;
        stem.resize(stem.size() - std::string_view("ій").size());
    } else if (stem.ends_with("ий")) {
        stem.resize(stem.size() - std::string_view("ий").size());
    }
    if (soft) {
        if (const auto soft_it = soft_endings.find(std::string(form)); soft_it != soft_endings.end()) {
            stem += soft_it->second;
            return stem;
        }
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
        {"п'ятдесят", {"п'ятдесяти", "п'ятдесяти", "п'ятдесятьма", "п'ятдесяти"}},
        {"шістдесят", {"шістдесяти", "шістдесяти", "шістдесятьма", "шістдесяти"}},
        {"сімдесят", {"сімдесяти", "сімдесяти", "сімдесятьма", "сімдесяти"}},
        {"вісімдесят", {"вісімдесяти", "вісімдесяти", "вісімдесятьма", "вісімдесяти"}},
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

} // namespace uktextnorm::detail
