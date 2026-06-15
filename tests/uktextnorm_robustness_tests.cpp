#include "uktextnorm/uktextnorm.hpp"

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

int failures = 0;

void fail(const std::string& name, const std::string& message)
{
    ++failures;
    std::cerr << name << ": " << message << "\n";
}

std::size_t utf8_char_count(std::string_view text)
{
    std::size_t count = 0;
    for (std::size_t i = 0; i < text.size();) {
        const auto ch = static_cast<unsigned char>(text[i]);
        if (ch < 0x80) {
            i += 1;
        } else if ((ch & 0xE0) == 0xC0 && i + 1 < text.size()) {
            i += 2;
        } else if ((ch & 0xF0) == 0xE0 && i + 2 < text.size()) {
            i += 3;
        } else if ((ch & 0xF8) == 0xF0 && i + 3 < text.size()) {
            i += 4;
        } else {
            i += 1;
        }
        ++count;
    }
    return count;
}

void exercise_one(const std::string& name, const std::string& text)
{
    const std::vector<uktextnorm::NormalizePreset> presets = {uktextnorm::NormalizePreset::Default,
                                                              uktextnorm::NormalizePreset::TtsFriendly,
                                                              uktextnorm::NormalizePreset::Conservative,
                                                              uktextnorm::NormalizePreset::SearchIndexing};

    for (const auto preset : presets) {
        try {
            const auto normalized = uktextnorm::normalize_ukrainian(text, preset);
            if (!text.empty() && normalized.empty()) {
                fail(name, "normalization returned empty output for non-empty input");
            }
        } catch (const std::exception& ex) {
            fail(name, std::string("normalize_ukrainian threw: ") + ex.what());
        } catch (...) {
            fail(name, "normalize_ukrainian threw an unknown exception");
        }
    }

    try {
        const auto spans = uktextnorm::flag_uncertain(text);
        const auto text_chars = utf8_char_count(text);
        for (const auto& span : spans) {
            if (span.start > span.stop || span.stop > text_chars) {
                fail(name, "uncertainty span is outside source bounds");
            }
            if (span.stop - span.start != utf8_char_count(span.text)) {
                fail(name, "uncertainty span text size does not match character range");
            }
        }
    } catch (const std::exception& ex) {
        fail(name, std::string("flag_uncertain threw: ") + ex.what());
    } catch (...) {
        fail(name, "flag_uncertain threw an unknown exception");
    }
}

} // namespace

int main()
{
    const std::vector<std::pair<std::string, std::string>> cases = {
        {"empty", ""},
        {"spaces", " \t  \n "},
        {"oversized number", "Номер 1234567890123456789012345678901234567890"},
        {"oversized dotted", "Версія 999999999999999999999999.999999999999999999999999.1"},
        {"malformed date", "Дата 99.99.9999 і 2026-99-99"},
        {"malformed url email", "Контакти https:// test@ @ _"},
        {"mixed scripts", "FooКиїв BarЛьвів АAАA"},
        {"dangling signs", "Ціна ₴ $ € £ № + - / : ;"},
        {"overprecise money", "Сума 1,234567890123456789 грн і $999999999999999999999999.99"},
        {"long phone-like", "Телефон 380671234567890123456789 і 0671234567890"},
        {"legal soup", "ч. ст. п. розд. № -- 910//1234///24"},
        {"roman soup", "IIII ст. VX розд. XIX-INVALID"},
        {"unicode punctuation", "«Тест» – — … ½ ⅞ 50/0"},
        {"combining apostrophes", "П'ять зв’язків мʼясо ІМ`Я"},
        {"latin products", "OpenAI ChatGPT GitHub Kubernetes TypeScript v999999999999999999999.1"},
        {"query string", "https://example.com/a?x=1&y=2&&&&"},
        {"compact finance", "BTC/UAH ETH/USD 000000000000000000000001 BTC"},
        {"measurement soup", "999999999999999999999999 кг 1,23456789 мг/мл -999999999999999999999999%"},
    };

    for (const auto& [name, text] : cases) {
        exercise_one(name, text);
    }

    return failures == 0 ? 0 : 1;
}
