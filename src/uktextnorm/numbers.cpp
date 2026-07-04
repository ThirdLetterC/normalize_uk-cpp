#include "uktextnorm/uktextnorm.hpp"

#include "internal.hpp"

namespace uktextnorm {

using namespace detail;

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

} // namespace uktextnorm
