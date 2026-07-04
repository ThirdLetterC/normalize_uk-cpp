#include "uktextnorm/uktextnorm.hpp"

#include "internal.hpp"

namespace uktextnorm::detail {

namespace {

bool is_quote_cp(char32_t cp)
{
    return cp == U'«' || cp == U'»' || cp == U'„' || cp == U'“' || cp == U'”' || cp == U'‟' || cp == U'‹' ||
           cp == U'›' || cp == U'"';
}

bool is_opening_quote_cp(char32_t cp)
{
    return cp == U'«' || cp == U'„' || cp == U'‟' || cp == U'‹';
}

bool is_apostrophe_variant(char32_t cp)
{
    // U+02BC modifier apostrophe, U+00B4 acute, U+2032 prime, U+201B and U+2018 single quotes.
    return cp == U'ʼ' || cp == U'´' || cp == U'′' || cp == U'‛' || cp == U'‘';
}

bool is_uk_letter(char32_t cp)
{
    return is_uk(cp) && !is_word_joiner(cp);
}

bool parse_octet(std::string_view text, int& value)
{
    if (text.empty() || text.size() > 3) {
        return false;
    }
    value = 0;
    for (const char ch : text) {
        if (ch < '0' || ch > '9') {
            return false;
        }
        value = value * 10 + (ch - '0');
    }
    return value <= 255;
}

bool has_hex_letter(std::string_view text)
{
    return std::any_of(text.begin(), text.end(), [](char ch) {
        return (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f');
    });
}

bool preceded_by_version_label(std::string_view prefix)
{
    std::size_t end = prefix.size();
    while (end > 0 && static_cast<unsigned char>(prefix[end - 1]) <= ' ') {
        --end;
    }
    std::size_t start = end;
    while (start > 0) {
        const unsigned char ch = static_cast<unsigned char>(prefix[start - 1]);
        if (ch <= ' ' || ch == '(' || ch == '[' || ch == ':' || ch == '=') {
            break;
        }
        --start;
    }
    const auto label = lower_text(prefix.substr(start, end - start));
    return label == "версія" || label == "версії" || label == "version" || label == "ver" || label == "v";
}

std::string read_ipv6_group(std::string_view group)
{
    if (group.empty()) {
        return "порожня група";
    }
    std::vector<std::string> parts;
    std::string current_digits;
    auto flush_digits = [&] {
        if (!current_digits.empty()) {
            parts.push_back(number_to_words_digit_by_digit(current_digits));
            current_digits.clear();
        }
    };
    for (const char ch : group) {
        if (ch >= '0' && ch <= '9') {
            current_digits.push_back(ch);
            continue;
        }
        flush_digits();
        const char letter = static_cast<char>(ch >= 'a' && ch <= 'z' ? ch - 32 : ch);
        parts.push_back(spell_identifier_letters(std::string_view(&letter, 1)));
    }
    flush_digits();
    return join(parts);
}

std::string read_coordinate_number(std::string_view digits)
{
    return number_to_words(parse_ull(digits));
}

std::string coordinate_hemisphere(std::string marker)
{
    marker = lower_text(marker);
    replace_all(marker, " ", "");
    if (marker.starts_with("пн") || marker.contains("північ")) {
        return "північної широти";
    }
    if (marker.starts_with("пд") || marker.contains("півден")) {
        return "південної широти";
    }
    if (marker.starts_with("сх") || marker.contains("схід")) {
        return "східної довготи";
    }
    if (marker.starts_with("зх") || marker.contains("зах")) {
        return "західної довготи";
    }
    return marker;
}

} // namespace

std::string normalize_unicode(std::string text, QuoteStyle quote_style)
{
    const auto cps = codepoints(text);
    std::string out;
    out.reserve(text.size());
    for (std::size_t idx = 0; idx < cps.size(); ++idx) {
        const auto cp = cps[idx].value;
        const auto next = idx + 1 < cps.size() ? cps[idx + 1].value : U'\0';
        const auto prev = idx > 0 ? cps[idx - 1].value : U'\0';
        // Targeted NFC for Ukrainian: compose и/і + combining breve/diaeresis.
        if (next == U'̆' && (cp == U'и' || cp == U'И')) {
            append_utf8(out, cp == U'и' ? U'й' : U'Й');
            ++idx;
            continue;
        }
        if (next == U'̈' && (cp == U'і' || cp == U'І')) {
            append_utf8(out, cp == U'і' ? U'ї' : U'Ї');
            ++idx;
            continue;
        }
        if (is_apostrophe_variant(cp) && is_uk_letter(prev) && is_uk_letter(next)) {
            out.push_back('\'');
            continue;
        }
        if (is_quote_cp(cp) && quote_style != QuoteStyle::Keep) {
            switch (quote_style) {
            case QuoteStyle::Straight:
                out.push_back('"');
                break;
            case QuoteStyle::Guillemets:
                append_utf8(out, is_opening_quote_cp(cp) || cp == U'“' ? U'«' : U'»');
                break;
            case QuoteStyle::Strip:
            default:
                break;
            }
            continue;
        }
        out.append(text.substr(cps[idx].start, cps[idx].stop - cps[idx].start));
    }
    return out;
}

std::string normalize_homoglyphs(std::string text)
{
    static const std::unordered_map<char32_t, char32_t> latin_to_cyr = {
        {U'a', U'а'}, {U'e', U'е'}, {U'i', U'і'}, {U'o', U'о'}, {U'p', U'р'}, {U'c', U'с'}, {U'x', U'х'},
        {U'y', U'у'}, {U'A', U'А'}, {U'B', U'В'}, {U'C', U'С'}, {U'E', U'Е'}, {U'H', U'Н'}, {U'I', U'І'},
        {U'K', U'К'}, {U'M', U'М'}, {U'O', U'О'}, {U'P', U'Р'}, {U'T', U'Т'}, {U'X', U'Х'}};
    static const std::unordered_map<char32_t, char32_t> cyr_to_latin = [] {
        std::unordered_map<char32_t, char32_t> out;
        for (const auto& [latin, cyr] : latin_to_cyr) {
            out.emplace(cyr, latin);
        }
        return out;
    }();
    const auto words = uncertain_word_spans(text);
    if (words.empty()) {
        return text;
    }
    std::string out;
    out.reserve(text.size());
    std::size_t last = 0;
    for (const auto& word : words) {
        std::size_t latin_count = 0;
        std::size_t cyr_count = 0;
        for (std::size_t i = word.start; i < word.stop;) {
            std::size_t next = i + 1;
            const auto cp = decode_one(text, i, next);
            if (is_latin(cp)) {
                ++latin_count;
            } else if (is_uk_letter(cp)) {
                ++cyr_count;
            }
            i = next;
        }
        if (!latin_count || !cyr_count) {
            continue;
        }
        const bool to_cyrillic = cyr_count >= latin_count;
        const auto& map = to_cyrillic ? latin_to_cyr : cyr_to_latin;
        bool repairable = true;
        std::string repaired;
        for (std::size_t i = word.start; i < word.stop && repairable;) {
            std::size_t next = i + 1;
            const auto cp = decode_one(text, i, next);
            const bool minority = to_cyrillic ? is_latin(cp) : is_uk_letter(cp);
            if (minority) {
                if (const auto it = map.find(cp); it != map.end()) {
                    append_utf8(repaired, it->second);
                } else {
                    repairable = false;
                }
            } else {
                repaired.append(text.substr(i, next - i));
            }
            i = next;
        }
        if (!repairable) {
            continue;
        }
        out.append(text, last, word.start - last);
        out += repaired;
        last = word.stop;
    }
    out.append(text, last, std::string::npos);
    return out;
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

std::string normalize_number_groups(std::string text, bool parse_thousand_separators)
{
    text = ctre_sub<R"(\b\d{1,3}(?: \d{3})+\b)">(text, [](const auto& m) {
        auto s = whole_string(m);
        replace_all(s, " ", "");
        return s;
    });
    if (!parse_thousand_separators) {
        return text;
    }
    // 1,234,567 — at least two comma groups of exactly three digits.
    text = ctre_sub<R"((^|[^\d.,])(\d{1,3}(?:,\d{3}){2,})(?!\d))">(text, [](const auto& m) {
        auto s = cap_string<2>(m);
        replace_all(s, ",", "");
        return cap_string<1>(m) + s;
    });
    // 1.234.567 — exactly two dot groups, so IPv4 addresses (three groups) stay intact.
    return ctre_sub<R"((^|[^\d.,])(\d{1,3}(?:\.\d{3}){2})(?!\.?\d))">(text, [](const auto& m) {
        auto s = cap_string<2>(m);
        replace_all(s, ".", "");
        return cap_string<1>(m) + s;
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
std::string normalize_text_with_phone_numbers(std::string text, PhoneStyle style)
{
    text = ctre_sub<R"((^|[^\d])((?:\+?380|0)\s*\(?\d{2}\)?[\-\s]?\d{3}[\-\s]?\d{2}[\-\s]?\d{2})(?!\d))">(
        text, [&](const auto& m) { return cap_string<1>(m) + normalize_phone_number(cap<2>(m), style); });
    static const std::regex international(
        R"((^|[^\d])(\+\d{1,3}(?:[\s().-]*\d{1,4}){2,})(?![\d]))");
    return regex_sub(text, international, [&](const std::smatch& m) {
        return m[1].str() + normalize_phone_number(m[2].str(), style);
    });
}

std::string normalize_ip_addresses(std::string text)
{
    static const std::regex ipv4(R"((^|[^\d.])(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})(?![\d.]))");
    text = regex_sub(text, ipv4, [](const std::smatch& m) {
        if (preceded_by_version_label(m.prefix().str())) {
            return m.str();
        }
        std::array<int, 4> octets{};
        for (std::size_t i = 0; i < octets.size(); ++i) {
            if (!parse_octet(m[i + 2].str(), octets[i])) {
                return m.str();
            }
        }
        std::vector<std::string> parts = {"ай пі"};
        for (const auto octet : octets) {
            parts.push_back(number_to_words(static_cast<unsigned long long>(octet)));
        }
        return m[1].str() + join(parts);
    });

    static const std::regex ipv6(
        R"((^|[^0-9A-Fa-f:])((?:[0-9A-Fa-f]{1,4}:){2,7}:?(?:[0-9A-Fa-f]{1,4})?)(?![0-9A-Fa-f:]))");
    return regex_sub(text, ipv6, [](const std::smatch& m) {
        const auto value = m[2].str();
        if (!has_hex_letter(value) && value.find("::") == std::string::npos) {
            return m.str();
        }
        if (value.find("::") != std::string::npos && value.find("::") != value.rfind("::")) {
            return m.str();
        }
        std::vector<std::string> groups;
        std::size_t start = 0;
        while (start <= value.size()) {
            const auto end = value.find(':', start);
            groups.push_back(read_ipv6_group(
                std::string_view(value).substr(start, end == std::string::npos ? std::string_view::npos : end - start)));
            if (end == std::string::npos) {
                break;
            }
            start = end + 1;
        }
        return m[1].str() + "ай пі версії шість " + join(groups, " двокрапка ");
    });
}

std::string normalize_coordinates(std::string text)
{
    static const std::regex dms(
        R"((^|[^\d])(\d{1,3})\s*°\s*(?:(\d{1,2})\s*(?:′|')\s*)?(?:(\d{1,2})\s*(?:″|")\s*)?((?:пн|пд|сх|зх)\.?\s*(?:ш|д)\.?|північн[а-яіїєґ]+\s+широт[а-яіїєґ]+|південн[а-яіїєґ]+\s+широт[а-яіїєґ]+|східн[а-яіїєґ]+\s+довгот[а-яіїєґ]+|західн[а-яіїєґ]+\s+довгот[а-яіїєґ]+))",
        std::regex::icase);
    return regex_sub(text, dms, [](const std::smatch& m) {
        std::vector<std::string> parts = {read_coordinate_number(m[2].str()), "градусів"};
        if (m[3].matched) {
            parts.push_back(read_coordinate_number(m[3].str()));
            parts.push_back("хвилин");
        }
        if (m[4].matched) {
            parts.push_back(read_coordinate_number(m[4].str()));
            parts.push_back("секунд");
        }
        parts.push_back(coordinate_hemisphere(m[5].str()));
        return m[1].str() + join(parts);
    });
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

} // namespace uktextnorm::detail
