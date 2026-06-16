#include "rozpodil/rozpodil.hpp"

#include "../common/utf8.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace rozpodil {
namespace {

enum class AtomType {
    Uk,
    Lat,
    Int,
    Punct,
    Other
};
enum class Action {
    None,
    Split,
    Join
};

struct Cp {
    char32_t value = 0;
    std::size_t start = 0;
    std::size_t stop = 0;
};

using normalize_uk_cpp::detail::decode_one;

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

bool contains(std::u32string_view set, char32_t cp)
{
    return set.contains(cp);
}

bool is_space(char32_t cp)
{
    return cp == U' ' || cp == U'\t' || cp == U'\n' || cp == U'\r' || cp == U'\f' || cp == U'\v' || cp == 0x00A0;
}

bool is_digit(char32_t cp)
{
    return cp >= U'0' && cp <= U'9';
}

bool is_lat(char32_t cp)
{
    return (cp >= U'a' && cp <= U'z') || (cp >= U'A' && cp <= U'Z');
}

bool is_uk(char32_t cp)
{
    return (cp >= U'а' && cp <= U'я') || (cp >= U'А' && cp <= U'Я') || cp == U'ґ' || cp == U'Ґ' || cp == U'є' ||
           cp == U'Є' || cp == U'і' || cp == U'І' || cp == U'ї' || cp == U'Ї';
}

bool is_alpha(char32_t cp)
{
    return is_lat(cp) || is_uk(cp);
}

bool is_uk_apostrophe(char32_t cp)
{
    return cp == U'\'' || cp == U'’' || cp == U'ʼ' || cp == U'`';
}

bool is_word_mark(char32_t cp)
{
    return cp == U'_' || cp == U'-';
}

bool is_inner_uk_apostrophe(const std::vector<Cp>& cps, std::size_t index)
{
    return index > 0 && index + 1 < cps.size() && is_uk_apostrophe(cps[index].value) && is_uk(cps[index - 1].value) &&
           is_uk(cps[index + 1].value);
}

bool is_word_letter(char32_t cp)
{
    return is_alpha(cp) || cp == U'_' || cp == U'Å' || cp == U'å' || cp == U'ğ' || cp == U'Ğ' ||
           (cp >= 0x0370 && cp <= 0x03FF);
}

bool is_python_alpha(char32_t cp)
{
    return is_alpha(cp) || cp == U'Å' || cp == U'å' || cp == U'ğ' || cp == U'Ğ' || (cp >= 0x0370 && cp <= 0x03FF);
}

char32_t lower_cp(char32_t cp)
{
    if (cp >= U'A' && cp <= U'Z') {
        return cp + 32;
    }
    if (cp >= U'А' && cp <= U'Я') {
        return cp + 32;
    }
    if (cp == U'Ґ') {
        return U'ґ';
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
    if (cp == U'Å') {
        return U'å';
    }
    if (cp == U'Ğ') {
        return U'ğ';
    }
    if (cp >= 0x0391 && cp <= 0x03A9) {
        return cp + 32;
    }
    return cp;
}

std::string lower_ascii_uk(std::string_view text)
{
    std::string out;
    for (std::size_t i = 0; i < text.size();) {
        std::size_t next = i + 1;
        const auto cp = decode_one(text, i, next);
        if (cp >= U'A' && cp <= U'Z') {
            out.push_back(static_cast<char>(cp + 32));
        } else if (cp >= U'А' && cp <= U'Я') {
            const auto low = cp + 32;
            out.push_back(static_cast<char>(0xD0 | ((low >> 6) & 0x1F)));
            out.push_back(static_cast<char>(0x80 | (low & 0x3F)));
        } else if (cp == U'Ґ') {
            out += "\xD2\x91";
        } else if (cp == U'Є') {
            out += "\xD1\x94";
        } else if (cp == U'І') {
            out += "\xD1\x96";
        } else if (cp == U'Ї') {
            out += "\xD1\x97";
        } else {
            out.append(text.substr(i, next - i));
        }
        i = next;
    }
    return out;
}

bool is_lower_alpha(std::string_view token)
{
    if (token.empty()) {
        return false;
    }
    bool saw_alpha = false;
    for (std::size_t i = 0; i < token.size();) {
        std::size_t next = i + 1;
        const auto cp = decode_one(token, i, next);
        if (!is_python_alpha(cp)) {
            return false;
        }
        if (lower_cp(cp) != cp) {
            return false;
        }
        saw_alpha = true;
        i = next;
    }
    return saw_alpha;
}

bool is_upper_one(std::string_view token)
{
    auto cps = codepoints(token);
    if (cps.size() != 1) {
        return false;
    }
    const auto cp = cps[0].value;
    return is_python_alpha(cp) && lower_cp(cp) != cp;
}

std::string_view trim_view(std::string_view text, std::size_t& offset)
{
    auto cps = codepoints(text);
    std::size_t first = 0;
    std::size_t last = cps.size();
    while (first < last && is_space(cps[first].value)) {
        ++first;
    }
    while (last > first && is_space(cps[last - 1].value)) {
        --last;
    }
    const std::size_t start = first < cps.size() ? cps[first].start : text.size();
    const std::size_t stop = last > 0 ? cps[last - 1].stop : start;
    offset += start;
    return text.substr(start, stop - start);
}

bool starts_with_space(std::string_view text)
{
    if (text.empty()) {
        return false;
    }
    std::size_t next = 0;
    return is_space(decode_one(text, 0, next));
}

bool ends_with_space(std::string_view text)
{
    auto cps = codepoints(text);
    return !cps.empty() && is_space(cps.back().value);
}

std::optional<std::string_view> first_token(std::string_view text)
{
    auto cps = codepoints(text);
    for (std::size_t i = 0; i < cps.size(); ++i) {
        if (is_space(cps[i].value)) {
            continue;
        }
        if (is_word_letter(cps[i].value)) {
            std::size_t j = i + 1;
            while (j < cps.size() && is_word_letter(cps[j].value)) {
                ++j;
            }
            return text.substr(cps[i].start, cps[j - 1].stop - cps[i].start);
        }
        if (is_digit(cps[i].value)) {
            std::size_t j = i + 1;
            while (j < cps.size() && is_digit(cps[j].value)) {
                ++j;
            }
            return text.substr(cps[i].start, cps[j - 1].stop - cps[i].start);
        }
        return text.substr(cps[i].start, cps[i].stop - cps[i].start);
    }
    return std::nullopt;
}

std::optional<std::string_view> first_word(std::string_view text)
{
    auto cps = codepoints(text);
    for (std::size_t i = 0; i < cps.size(); ++i) {
        if (is_word_letter(cps[i].value)) {
            std::size_t j = i + 1;
            while (j < cps.size() && is_word_letter(cps[j].value)) {
                ++j;
            }
            return text.substr(cps[i].start, cps[j - 1].stop - cps[i].start);
        }
        if (is_digit(cps[i].value)) {
            std::size_t j = i + 1;
            while (j < cps.size() && is_digit(cps[j].value)) {
                ++j;
            }
            return text.substr(cps[i].start, cps[j - 1].stop - cps[i].start);
        }
    }
    return std::nullopt;
}

bool starts_with_letter_bullet(std::string_view text)
{
    auto cps = codepoints(text);
    std::size_t i = 0;
    while (i < cps.size() && is_space(cps[i].value)) {
        ++i;
    }
    if (i + 1 >= cps.size() || !is_alpha(cps[i].value)) {
        return false;
    }
    if (cps[i + 1].value != U')') {
        return false;
    }
    return i + 2 >= cps.size() || is_space(cps[i + 2].value);
}

std::optional<std::string_view> last_token(std::string_view text)
{
    auto cps = codepoints(text);
    for (std::size_t n = cps.size(); n > 0; --n) {
        const std::size_t i = n - 1;
        if (is_space(cps[i].value)) {
            continue;
        }
        if (is_word_letter(cps[i].value)) {
            std::size_t j = i;
            while (j > 0 && is_word_letter(cps[j - 1].value)) {
                --j;
            }
            return text.substr(cps[j].start, cps[i].stop - cps[j].start);
        }
        if (is_digit(cps[i].value)) {
            std::size_t j = i;
            while (j > 0 && is_digit(cps[j - 1].value)) {
                --j;
            }
            return text.substr(cps[j].start, cps[i].stop - cps[j].start);
        }
        return text.substr(cps[i].start, cps[i].stop - cps[i].start);
    }
    return std::nullopt;
}

bool is_word_cp(char32_t cp)
{
    return is_word_letter(cp) || is_digit(cp);
}

std::optional<std::string_view> last_compound_abbrev_token(std::string_view text)
{
    auto cps = codepoints(text);
    for (std::size_t n = cps.size(); n > 0; --n) {
        const std::size_t i = n - 1;
        if (is_space(cps[i].value)) {
            continue;
        }
        if (!is_word_cp(cps[i].value)) {
            return std::nullopt;
        }
        bool saw_compound_mark = false;
        std::size_t j = i;
        while (j > 0 && (is_word_cp(cps[j - 1].value) || cps[j - 1].value == U'-' || cps[j - 1].value == U'.' ||
                         cps[j - 1].value == U'/')) {
            if (cps[j - 1].value == U'-' || cps[j - 1].value == U'.' || cps[j - 1].value == U'/') {
                saw_compound_mark = true;
            }
            --j;
        }
        while (j <= i && (cps[j].value == U'-' || cps[j].value == U'.' || cps[j].value == U'/')) {
            ++j;
        }
        if (j > i || !saw_compound_mark) {
            return std::nullopt;
        }
        return text.substr(cps[j].start, cps[i].stop - cps[j].start);
    }
    return std::nullopt;
}

std::optional<std::string_view> trailing_dot_abbrev_token(std::string_view text)
{
    auto cps = codepoints(text);
    if (cps.empty()) {
        return std::nullopt;
    }
    std::size_t i = cps.size();
    while (i > 0 && is_space(cps[i - 1].value)) {
        --i;
    }
    if (i == 0 || cps[i - 1].value != U'.') {
        return std::nullopt;
    }
    --i;
    while (i > 0 && is_space(cps[i - 1].value)) {
        --i;
    }
    const std::size_t stop = i;
    while (i > 0 && is_word_cp(cps[i - 1].value)) {
        --i;
    }
    if (i == stop) {
        return std::nullopt;
    }
    return text.substr(cps[i].start, cps[stop - 1].stop - cps[i].start);
}

std::optional<std::pair<std::string_view, std::string_view>> left_pair_sokr(std::string_view text)
{
    auto cps = codepoints(text);
    if (cps.empty()) {
        return std::nullopt;
    }
    std::size_t i = cps.size();
    while (i > 0 && is_space(cps[i - 1].value)) {
        --i;
    }
    const std::size_t b_stop = i;
    while (i > 0 && is_word_cp(cps[i - 1].value)) {
        --i;
    }
    if (i == b_stop) {
        return std::nullopt;
    }
    const std::size_t b_start = i;
    while (i > 0 && is_space(cps[i - 1].value)) {
        --i;
    }
    if (i == 0 || cps[i - 1].value != U'.') {
        return std::nullopt;
    }
    --i;
    while (i > 0 && is_space(cps[i - 1].value)) {
        --i;
    }
    const std::size_t a_stop = i;
    while (i > 0 && is_word_cp(cps[i - 1].value)) {
        --i;
    }
    if (i == a_stop) {
        return std::nullopt;
    }
    const std::size_t a_start = i;
    return std::pair<std::string_view, std::string_view>{
        text.substr(cps[a_start].start, cps[a_stop - 1].stop - cps[a_start].start),
        text.substr(cps[b_start].start, cps[b_stop - 1].stop - cps[b_start].start)};
}

std::vector<std::string_view> tokens_in(std::string_view text)
{
    std::vector<std::string_view> out;
    auto cps = codepoints(text);
    for (std::size_t i = 0; i < cps.size();) {
        if (is_space(cps[i].value)) {
            ++i;
            continue;
        }
        const auto begin = i;
        if (is_word_letter(cps[i].value)) {
            while (i < cps.size() && is_word_letter(cps[i].value)) {
                ++i;
            }
        } else if (is_digit(cps[i].value)) {
            while (i < cps.size() && is_digit(cps[i].value)) {
                ++i;
            }
        } else {
            ++i;
        }
        out.push_back(text.substr(cps[begin].start, cps[i - 1].stop - cps[begin].start));
    }
    return out;
}

std::unordered_set<std::string_view> words(std::initializer_list<std::string_view> values)
{
    return {values.begin(), values.end()};
}

const auto tail_sokrs =
    words({"тис",  "млн", "млрд", "грн", "коп", "проц", "га",  "кг",  "г",   "т",   "куб", "кв",   "км",   "м",
           "см",   "мм",  "л",    "год", "хв",  "сек",  "ст",  "р",   "рр",  "с",   "к",   "руб",  "крб",  "co",
           "corp", "inc", "ed",   "al",  "мон", "моз",  "мвс", "сбу", "нбу", "дпс", "дбр", "набу", "назк", "ова",
           "ода",  "рда", "кмда", "мкм", "нм",  "квт",  "мвт", "шт",  "од",  "екз", "вс",  "оаск", "єрдр", "ecli"});
const auto head_sokrs = words(
    {"ст",     "укр",  "англ",  "нім",  "фр",     "італ",   "грец", "лат",     "mr",     "mrs",    "ms",      "dr",
     "vs",     "св",   "проф",  "акад", "доц",    "канд",   "д-р",  "ред",     "гр",     "ім",     "тов",     "п",
     "пп",     "ч",    "чч",    "гл",   "абз",    "пт",     "no",   "просп",   "пр",     "вул",    "ш",       "м",
     "смт",    "с",    "обл",   "р-н",  "корп",   "пер",    "пл",   "буд",     "кв",     "оф",     "каб",     "літ",
     "р",      "а",    "оз",    "г",    "напр",   "дод",    "юр",   "фіз",     "тел",    "тобто",  "див",     "розд",
     "табл",   "мал",  "рис",   "пор",  "упоряд", "мкр",    "наб",  "пров",    "шос",    "бул",    "деп",     "пост",
     "наказ",  "підп", "арк",   "вип",  "стор",   "ухв",    "ріш",  "провадж", "спр",    "поз",    "позов",   "відп",
     "заявн",  "оск",  "адмін", "крим", "цив",    "госп",   "док",  "прим",    "перекл", "вид",    "т",       "тт",
     "зб",     "зош",  "журн",  "газ",  "асист",  "викл",   "зав",  "лаб",     "інж",    "н",      "чл.-кор", "м-н",
     "ж/м",    "в/ч",  "остр",  "річ",  "станц",  "залізн", "бл",   "прибл",   "зокр",   "порівн", "підрозд", "тр",
     "скаржн", "кк",   "кпк",   "цк",   "цпк",    "гк",     "гпк",  "кас",     "купап",  "кзпп",   "пку",     "мку",
     "зку",    "ску",  "вс",    "вп",   "кцс",    "кгс",    "ккс",  "оаск",    "v",      "єрдр",   "ecli"});
const auto other_sokrs = words({"скор", "рис", "винят", "прим", "заст", "жарт"});
const auto initials = words({"дж", "ed"});

const std::unordered_set<std::string> head_pair_sokrs = {
    "т е", "т к", "т н", "и о", "к н", "к п", "п н", "к т", "л д", "і т", "ст ст", "а с"};
const std::unordered_set<std::string> pair_sokrs = {
    "т п", "т д", "у е", "н э", "p m", "a m", "с г", "р х",  "с ш",  "з д",        "л с",        "ч т", "т е",   "т к",
    "т н", "и о", "к н", "к п", "п н", "к т", "л д", "ед ч", "мн ч", "повел накл", "жен рмуж р", "і т", "ст ст", "а с"};

bool in_sokrs(std::string_view value)
{
    return tail_sokrs.contains(value) || head_sokrs.contains(value) || other_sokrs.contains(value);
}

bool is_sokr_right(std::string_view token)
{
    auto cps = codepoints(token);
    if (cps.empty()) {
        return false;
    }
    if (std::ranges::all_of(cps, [](const Cp& cp) { return is_digit(cp.value); })) {
        return true;
    }
    if (!std::ranges::all_of(cps, [](const Cp& cp) { return is_alpha(cp.value); })) {
        return true;
    }
    return is_lower_alpha(token);
}

bool is_roman_token(std::string_view token)
{
    if (token.empty()) {
        return false;
    }
    for (char c : token) {
        if (!std::string_view("IVXLCDM").contains(c)) {
            return false;
        }
    }
    return true;
}

bool is_article_abbrev_right(std::string_view token)
{
    const auto cps = codepoints(token);
    if (cps.empty()) {
        return false;
    }
    if (std::ranges::all_of(cps, [](const Cp& cp) { return is_digit(cp.value); })) {
        return true;
    }
    return is_roman_token(token);
}

bool roman(std::string_view token)
{
    if (token.empty()) {
        return false;
    }
    for (char c : token) {
        if (!std::string_view("IVXML").contains(c)) {
            return false;
        }
    }
    return true;
}

bool is_bullet(std::string_view token)
{
    if (token.empty()) {
        return false;
    }
    if (std::ranges::all_of(token, [](unsigned char ch) { return std::isdigit(ch); })) {
        return true;
    }
    if (token == "." || token == ")") {
        return true;
    }
    const auto lower = lower_ascii_uk(token);
    return std::string_view("§абвгдеabcdef").contains(std::string_view(lower)) || roman(token);
}

bool is_smile_at(std::string_view text, std::size_t pos, std::size_t& stop)
{
    if (pos >= text.size() || (text[pos] != '=' && text[pos] != ':' && text[pos] != ';')) {
        return false;
    }
    std::size_t i = pos + 1;
    if (i < text.size() && text[i] == '-') {
        ++i;
    }
    std::size_t count = 0;
    while (i < text.size() && (text[i] == '(' || text[i] == ')') && count < 3) {
        ++i;
        ++count;
    }
    if (count == 0) {
        return false;
    }
    stop = i;
    return true;
}

bool smile_prefix(std::string_view text)
{
    std::size_t i = 0;
    while (i < text.size()) {
        std::size_t next = i + 1;
        if (!is_space(decode_one(text, i, next))) {
            break;
        }
        i = next;
    }
    std::size_t stop = i;
    return is_smile_at(text, i, stop);
}

struct SentSplit {
    std::string_view left;
    std::string_view delimiter;
    std::string_view right;
    std::string buffer;
};

Action sent_join(const SentSplit& split)
{
    static constexpr std::u32string_view endings = U".?!…";
    static constexpr std::u32string_view dashes = U"‑–—−-";
    static constexpr std::u32string_view generic_quotes = U"\"„'";
    static constexpr std::u32string_view close_quotes = U"»”’";
    static constexpr std::u32string_view close_brackets = U")]}";
    static constexpr std::u32string_view delimiters = U".?!…;\"„'»”’)]}";

    const auto left = last_token(split.left);
    const auto right = first_token(split.right);
    if (!left || !right) {
        return Action::Join;
    }
    if (!starts_with_space(split.right)) {
        return Action::Join;
    }
    if (starts_with_letter_bullet(split.right)) {
        return Action::None;
    }
    if (is_lower_alpha(*right)) {
        return Action::Join;
    }
    std::size_t n = 0;
    const auto right_cp = decode_one(*right, 0, n);
    if (!contains(generic_quotes, right_cp) && (contains(delimiters, right_cp) || smile_prefix(split.right))) {
        return Action::Join;
    }
    std::size_t dnext = 0;
    const auto delimiter_cp = decode_one(split.delimiter, 0, dnext);
    const auto left_lower = lower_ascii_uk(last_compound_abbrev_token(split.left).value_or(*left));
    if (split.delimiter == ".") {
        bool skip_single_sokr = false;
        if (std::ranges::all_of(codepoints(*left), [](const Cp& cp) { return is_digit(cp.value); }) &&
            std::ranges::all_of(codepoints(*right), [](const Cp& cp) { return is_digit(cp.value); })) {
            return Action::Join;
        }
        if (auto dotted = trailing_dot_abbrev_token(split.left)) {
            const auto dotted_lower = lower_ascii_uk(*dotted);
            if (in_sokrs(dotted_lower) && is_sokr_right(*right)) {
                return Action::Join;
            }
        }
        if (auto pair_match = left_pair_sokr(split.left)) {
            const auto a = lower_ascii_uk(pair_match->first);
            const auto b = lower_ascii_uk(pair_match->second);
            const std::string pair = a + " " + b;
            if (head_pair_sokrs.contains(pair)) {
                return Action::Join;
            }
            if (pair_sokrs.contains(pair)) {
                if (is_sokr_right(*right)) {
                    return Action::Join;
                }
                skip_single_sokr = true;
            }
        }
        if (!skip_single_sokr) {
            if ((left_lower == "ст" && is_article_abbrev_right(*right)) ||
                (left_lower != "ст" && head_sokrs.contains(left_lower)) ||
                (in_sokrs(left_lower) && is_sokr_right(*right))) {
                return Action::Join;
            }
            const auto right_lower = lower_ascii_uk(*right);
            if (pair_sokrs.contains(left_lower + " " + right_lower)) {
                return Action::Join;
            }
        }
        if (is_upper_one(*left) || initials.contains(left_lower)) {
            return Action::Join;
        }
    }
    if ((split.delimiter == "." || split.delimiter == ")") && split.buffer.size() <= 20) {
        const auto toks = tokens_in(split.buffer);
        if (!toks.empty() && std::ranges::all_of(toks, is_bullet)) {
            return Action::Join;
        }
    }
    if (contains(close_quotes, delimiter_cp) || contains(generic_quotes, delimiter_cp) ||
        contains(close_brackets, delimiter_cp)) {
        std::size_t lnext = 0;
        const auto left_cp = decode_one(*left, 0, lnext);
        if (!contains(endings, left_cp)) {
            return Action::Join;
        }
        if (contains(generic_quotes, delimiter_cp) && ends_with_space(split.left)) {
            return Action::Join;
        }
    }
    if (contains(dashes, right_cp)) {
        auto rw = first_word(split.right);
        if (rw && is_lower_alpha(*rw)) {
            return Action::Join;
        }
    }
    return Action::None;
}

std::vector<Substring> find_substrings(const std::vector<std::string>& chunks, std::string_view text, bool trim)
{
    std::vector<Substring> out;
    std::size_t offset = 0;
    for (const auto& chunk : chunks) {
        const auto found = text.find(chunk, offset);
        if (found == std::string_view::npos) {
            continue;
        }
        std::size_t start = found;
        auto view = text.substr(found, chunk.size());
        if (trim) {
            view = trim_view(view, start);
        }
        if (!view.empty()) {
            out.push_back({start, start + view.size(), view});
        }
        offset = found + chunk.size();
    }
    return out;
}

struct Atom {
    std::size_t start = 0;
    std::size_t stop = 0;
    AtomType type = AtomType::Other;
    std::string_view text;
    std::string normal;
};

bool is_token_punct(char32_t cp)
{
    static constexpr std::u32string_view puncts = U"\\/!#$%&*+,.:;<=>?@^_`|~№…‑–—−-«“‘»”’\"„'()[]{}";
    return contains(puncts, cp);
}

bool starts_with_at(std::string_view text, std::size_t pos, std::string_view prefix)
{
    return pos + prefix.size() <= text.size() && text.substr(pos, prefix.size()) == prefix;
}

bool is_web_stop(char32_t cp)
{
    return is_space(cp) || cp == U'<' || cp == U'>' || cp == U'"' || cp == U'«' || cp == U'»' || cp == U'“' ||
           cp == U'”' || cp == U'(' || cp == U')' || cp == U'[' || cp == U']' || cp == U'{' || cp == U'}';
}

bool is_trailing_url_punct(char32_t cp)
{
    return cp == U'.' || cp == U',' || cp == U';' || cp == U':' || cp == U'!' || cp == U'?';
}

std::optional<std::size_t> legal_number_atom_stop(const std::vector<Cp>& cps, std::size_t index)
{
    if (cps[index].value != U'№') {
        return std::nullopt;
    }
    std::size_t i = index + 1;
    if (i < cps.size() && cps[i].value == U'-') {
        ++i;
    }
    const auto body_begin = i;
    while (i < cps.size() &&
           (is_alpha(cps[i].value) || is_digit(cps[i].value) || cps[i].value == U'/' || cps[i].value == U'-')) {
        ++i;
    }
    return i > body_begin ? std::optional<std::size_t>(i) : std::nullopt;
}

std::optional<std::size_t> web_atom_stop(std::string_view text, const std::vector<Cp>& cps, std::size_t index)
{
    const auto start = cps[index].start;
    const bool url = starts_with_at(text, start, "http://") || starts_with_at(text, start, "https://");
    if (url) {
        std::size_t i = index;
        while (i < cps.size() && !is_web_stop(cps[i].value)) {
            ++i;
        }
        while (i > index && is_trailing_url_punct(cps[i - 1].value)) {
            --i;
        }
        return i > index ? std::optional<std::size_t>(i) : std::nullopt;
    }

    if (cps[index].value == U'@' || cps[index].value == U'#') {
        std::size_t i = index + 1;
        while (i < cps.size() && (is_alpha(cps[i].value) || is_digit(cps[i].value) || is_word_mark(cps[i].value))) {
            ++i;
        }
        return i > index + 1 ? std::optional<std::size_t>(i) : std::nullopt;
    }

    if (!is_alpha(cps[index].value) && !is_digit(cps[index].value)) {
        return std::nullopt;
    }
    std::size_t i = index;
    bool saw_at = false;
    bool saw_dot_after_at = false;
    while (i < cps.size()) {
        const auto cp = cps[i].value;
        if (is_alpha(cp) || is_digit(cp) || cp == U'_' || cp == U'-' || cp == U'.') {
            if (saw_at && cp == U'.') {
                saw_dot_after_at = true;
            }
            ++i;
            continue;
        }
        if (cp == U'@' && !saw_at && i > index) {
            saw_at = true;
            ++i;
            continue;
        }
        break;
    }
    while (i > index && is_trailing_url_punct(cps[i - 1].value)) {
        --i;
    }
    return saw_at && saw_dot_after_at && i > index ? std::optional<std::size_t>(i) : std::nullopt;
}

std::vector<Atom> atoms(std::string_view text)
{
    std::vector<Atom> out;
    auto cps = codepoints(text);
    for (std::size_t i = 0; i < cps.size();) {
        if (is_space(cps[i].value)) {
            ++i;
            continue;
        }
        const std::size_t begin = i;
        AtomType type = AtomType::Other;
        if (auto stop = legal_number_atom_stop(cps, i)) {
            type = AtomType::Other;
            i = *stop;
        } else if (auto stop = web_atom_stop(text, cps, i)) {
            type = AtomType::Other;
            i = *stop;
        } else if (is_uk(cps[i].value)) {
            type = AtomType::Uk;
            while (i < cps.size() && (is_uk(cps[i].value) || is_inner_uk_apostrophe(cps, i))) {
                ++i;
            }
        } else if (is_lat(cps[i].value)) {
            type = AtomType::Lat;
            while (i < cps.size() && is_lat(cps[i].value)) {
                ++i;
            }
        } else if (is_digit(cps[i].value)) {
            type = AtomType::Int;
            while (i < cps.size() && is_digit(cps[i].value)) {
                ++i;
            }
        } else {
            type = is_token_punct(cps[i].value) ? AtomType::Punct : AtomType::Other;
            ++i;
        }
        auto sv = text.substr(cps[begin].start, cps[i - 1].stop - cps[begin].start);
        out.push_back({cps[begin].start, cps[i - 1].stop, type, sv, lower_ascii_uk(sv)});
    }
    return out;
}

bool token_smile(std::string_view text)
{
    std::size_t stop = 0;
    return is_smile_at(text, 0, stop) && stop == text.size();
}

bool token_join(const Atom& left_1,
                const std::optional<Atom>& left_2,
                std::string_view delimiter,
                const Atom& right_1,
                const std::optional<Atom>& right_2,
                std::string_view buffer)
{
    auto rule2112 = [&](char32_t delim, auto pred) {
        std::size_t next = 0;
        if (!delimiter.empty() && decode_one(delimiter, 0, next) == delim) {
            if (!left_1.text.empty() && !right_1.text.empty()) {
                return pred(left_1, right_1);
            }
        }
        if (delimiter.empty() && left_2 && right_2) {
            std::size_t ln = 0;
            if (decode_one(left_1.text, 0, ln) == delim) {
                return pred(*left_2, right_1);
            }
            std::size_t rn = 0;
            if (decode_one(right_1.text, 0, rn) == delim) {
                return pred(left_1, *right_2);
            }
        }
        if (delimiter.empty() && left_2) {
            std::size_t ln = 0;
            if (decode_one(left_1.text, 0, ln) == delim) {
                return pred(*left_2, right_1);
            }
        }
        if (delimiter.empty() && right_2) {
            std::size_t rn = 0;
            if (decode_one(right_1.text, 0, rn) == delim) {
                return pred(left_1, *right_2);
            }
        }
        return false;
    };
    auto dash_or_underscore = [](const Atom& l, const Atom& r) {
        return l.type != AtomType::Punct && r.type != AtomType::Punct;
    };
    for (char32_t dash : std::u32string_view(U"‑–—−-")) {
        if (rule2112(dash, dash_or_underscore)) {
            return true;
        }
    }
    if (rule2112(U'_', dash_or_underscore)) {
        return true;
    }
    for (char32_t dot : std::u32string_view(U".,")) {
        if (rule2112(dot,
                     [](const Atom& l, const Atom& r) { return l.type == AtomType::Int && r.type == AtomType::Int; })) {
            return true;
        }
    }
    for (char32_t slash : std::u32string_view(U"/\\")) {
        if (rule2112(slash,
                     [](const Atom& l, const Atom& r) { return l.type == AtomType::Int && r.type == AtomType::Int; })) {
            return true;
        }
    }
    if (left_1.type == AtomType::Punct && right_1.type == AtomType::Punct) {
        std::string candidate(buffer);
        candidate.append(right_1.text);
        if (token_smile(candidate)) {
            return true;
        }
        if (std::string_view(".?!…").contains(left_1.text) && std::string_view(".?!…").contains(right_1.text)) {
            return true;
        }
        if (left_1.text == right_1.text && (left_1.text == "-" || left_1.text == "*")) {
            return true;
        }
    }
    if (left_1.type == AtomType::Other &&
        (right_1.type == AtomType::Other || right_1.type == AtomType::Uk || right_1.type == AtomType::Lat)) {
        return true;
    }
    if ((left_1.type == AtomType::Other || left_1.type == AtomType::Uk || left_1.type == AtomType::Lat) &&
        right_1.type == AtomType::Other) {
        return true;
    }
    if (delimiter.empty() && left_1.type == AtomType::Int &&
        (right_1.type == AtomType::Uk || right_1.type == AtomType::Lat) && in_sokrs(right_1.normal)) {
        return true;
    }
    return left_1.normal == "yahoo" && right_1.text == "!";
}

} // namespace

std::vector<Substring> sentenize(std::string_view text)
{
    if (std::ranges::all_of(codepoints(text), [](const Cp& cp) { return is_space(cp.value); })) {
        return {};
    }
    static constexpr std::u32string_view delimiters = U".?!…;\"„'»”’)]}";
    auto cps = codepoints(text);
    struct Part {
        bool split = false;
        std::string value;
        SentSplit sent;
    };
    std::vector<Part> parts;
    std::size_t previous = 0;
    for (std::size_t ci = 0; ci < cps.size(); ++ci) {
        std::size_t stop = cps[ci].stop;
        bool is_delim = contains(delimiters, cps[ci].value);
        std::size_t delimiter_end_index = ci + 1;
        if (std::size_t smile_stop = 0; is_smile_at(text, cps[ci].start, smile_stop)) {
            stop = smile_stop;
            is_delim = true;
            while (delimiter_end_index < cps.size() && cps[delimiter_end_index].start < stop) {
                ++delimiter_end_index;
            }
        }
        if (!is_delim) {
            continue;
        }
        const auto start = cps[ci].start;
        const auto delimiter = text.substr(start, stop - start);
        parts.push_back({false, std::string(text.substr(previous, start - previous)), {}});
        const auto left_start = ci > 10 ? cps[ci - 10].start : 0;
        std::size_t right_stop = text.size();
        const auto right_begin_index = delimiter_end_index;
        if (right_begin_index < cps.size()) {
            const auto right_end_index = std::min(cps.size(), right_begin_index + 10);
            right_stop = cps[right_end_index - 1].stop;
        }
        parts.push_back(
            {true,
             {},
             SentSplit{
                 text.substr(left_start, start - left_start), delimiter, text.substr(stop, right_stop - stop), {}}});
        previous = stop;
        ci = delimiter_end_index - 1;
    }
    parts.push_back({false, std::string(text.substr(previous)), {}});
    if (parts.empty()) {
        return {};
    }
    std::vector<std::string> chunks;
    std::string buffer = parts.front().value;
    for (std::size_t i = 1; i + 1 < parts.size(); i += 2) {
        auto split = parts[i].sent;
        split.buffer = buffer;
        const auto& right = parts[i + 1].value;
        if (sent_join(split) == Action::Join) {
            buffer += std::string(split.delimiter) + right;
        } else {
            chunks.push_back(buffer + std::string(split.delimiter));
            buffer = right;
        }
    }
    chunks.push_back(buffer);
    return find_substrings(chunks, text, true);
}

std::vector<Substring> tokenize(std::string_view text)
{
    const auto as = atoms(text);
    if (as.empty()) {
        return {};
    }
    std::vector<std::string> chunks;
    std::string buffer(as.front().text);
    for (std::size_t i = 1; i < as.size(); ++i) {
        const auto delimiter = text.substr(as[i - 1].stop, as[i].start - as[i - 1].stop);
        std::optional<Atom> left_2;
        std::optional<Atom> right_2;
        if (i >= 2) {
            left_2 = as[i - 2];
        }
        if (i + 1 < as.size()) {
            right_2 = as[i + 1];
        }
        if (delimiter.empty() && token_join(as[i - 1], left_2, delimiter, as[i], right_2, buffer)) {
            buffer += std::string(as[i].text);
        } else {
            chunks.push_back(buffer);
            buffer = std::string(as[i].text);
        }
    }
    chunks.push_back(buffer);
    return find_substrings(chunks, text, false);
}

} // namespace rozpodil
