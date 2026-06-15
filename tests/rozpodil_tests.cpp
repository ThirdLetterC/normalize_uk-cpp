#include "rozpodil/rozpodil.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

struct Expected {
    std::size_t start = 0;
    std::size_t stop = 0;
    std::string text;
};

void expect_eq(const std::string& name,
               const std::vector<rozpodil::Substring>& actual,
               const std::vector<Expected>& expected)
{
    if (actual.size() != expected.size()) {
        ++failures;
        std::cerr << name << ": size expected " << expected.size() << " actual " << actual.size() << "\n";
        return;
    }
    for (std::size_t i = 0; i < actual.size(); ++i) {
        if (actual[i].start != expected[i].start || actual[i].stop != expected[i].stop ||
            actual[i].text != expected[i].text) {
            ++failures;
            std::cerr << name << ": mismatch at " << i << "\nexpected: " << expected[i].start << ", "
                      << expected[i].stop << ", " << expected[i].text << "\nactual:   " << actual[i].start << ", "
                      << actual[i].stop << ", " << actual[i].text << "\n";
            return;
        }
    }
}

std::vector<Expected> expected_chunks(const std::string& text, const std::vector<std::string>& chunks)
{
    std::vector<Expected> out;
    std::size_t offset = 0;
    for (const auto& chunk : chunks) {
        const auto pos = text.find(chunk, offset);
        if (pos == std::string::npos) {
            ++failures;
            std::cerr << "missing expected chunk: " << chunk << "\n";
            continue;
        }
        out.push_back({pos, pos + chunk.size(), chunk});
        offset = pos + chunk.size();
    }
    return out;
}

void expect_tokens(const std::string& name, const std::string& text, const std::vector<std::string>& chunks)
{
    expect_eq(name, rozpodil::tokenize(text), expected_chunks(text, chunks));
}

void expect_sents(const std::string& name, const std::string& text, const std::vector<std::string>& chunks)
{
    expect_eq(name, rozpodil::sentenize(text), expected_chunks(text, chunks));
}

} // namespace

int main()
{
    const std::string apostrophes = "П'ять зв’язків і мʼясо.";
    expect_tokens("apostrophe words", apostrophes, {"П'ять", "зв’язків", "і", "мʼясо", "."});

    const std::string ocr_apostrophe = "ІМ`Я зобов`язати.";
    expect_tokens("ocr apostrophe words", ocr_apostrophe, {"ІМ`Я", "зобов`язати", "."});

    const std::string uk_letters = "Ґанок, їжа, Європа й Іван.";
    expect_tokens("ukrainian letters", uk_letters, {"Ґанок", ",", "їжа", ",", "Європа", "й", "Іван", "."});

    const std::string token_mix = "Науково-технічний звіт: 12,5 грн (50/64 см).";
    expect_tokens(
        "mixed tokens", token_mix, {"Науково-технічний", "звіт", ":", "12,5", "грн", "(", "50/64", "см", ")", "."});

    const std::string web_tokens =
        "Пишіть на test.user@example.com або @rada, тег #Київ: https://example.com/news?id=1.";
    expect_tokens("web social tokens",
                  web_tokens,
                  {"Пишіть",
                   "на",
                   "test.user@example.com",
                   "або",
                   "@rada",
                   ",",
                   "тег",
                   "#Київ",
                   ":",
                   "https://example.com/news?id=1",
                   "."});

    const std::string legal_number_tokens = "№-81 і №123/2026-р.";
    expect_tokens("legal number marker tokens", legal_number_tokens, {"№-81", "і", "№123/2026-р", "."});

    const std::string compact_suffix_tokens = "1991р. 78к. 50крб.";
    expect_tokens("compact numeric suffix tokens", compact_suffix_tokens, {"1991р", ".", "78к", ".", "50крб", "."});

    const std::string address = "м. Київ, вул. Хрещатик, 1. Зустріч о 10:30.";
    expect_sents("address", address, {"м. Київ, вул. Хрещатик, 1.", "Зустріч о 10:30."});

    const std::string full_address = "Адреса: Львівська обл., м. Львів, просп. Свободи, буд. 10, кв. 5. Прийом з 9:00.";
    expect_sents("full address",
                 full_address,
                 {"Адреса: Львівська обл., м. Львів, просп. Свободи, буд. 10, кв. 5.", "Прийом з 9:00."});

    const std::string street_abbrevs =
        "Маршрут: мкр. Сонячний, наб. Дніпровська, пров. Тихий, шос. Київське, бул. Лесі Українки. Далі пішки.";
    expect_sents(
        "street abbreviations",
        street_abbrevs,
        {"Маршрут: мкр. Сонячний, наб. Дніпровська, пров. Тихий, шос. Київське, бул. Лесі Українки.", "Далі пішки."});

    const std::string dashed_abbrevs = "Шевченківський р-н. Київ, каб. 12. Д-р. Петренко приймає о 15:00.";
    expect_sents("dashed abbreviations",
                 dashed_abbrevs,
                 {"Шевченківський р-н. Київ, каб. 12.", "Д-р. Петренко приймає о 15:00."});

    const std::string news = "Президент заявив: \"Ми готові\". Після цього відбулася зустріч.";
    expect_sents("news quotes", news, {"Президент заявив: \"Ми готові\".", "Після цього відбулася зустріч."});

    const std::string guillemets = "Міністр сказав: «Це важливо». Потім додав: «Працюємо далі!»";
    expect_sents("guillemets", guillemets, {"Міністр сказав: «Це важливо».", "Потім додав: «Працюємо далі!»"});

    const std::string legal =
        "Згідно зі ст. 10 Закону України, рішення набирає чинності. Оскарження можливе протягом 30 днів.";
    expect_sents(
        "legal abbreviation",
        legal,
        {"Згідно зі ст. 10 Закону України, рішення набирає чинності.", "Оскарження можливе протягом 30 днів."});

    const std::string article_abbrev_boundary = "Це не ст. Інше речення.";
    expect_sents("article abbreviation boundary", article_abbrev_boundary, {"Це не ст.", "Інше речення."});

    const std::string legal_refs = "Див. п. 2 ч. 1 ст. 5 ЦК України та розд. IV документа. Пор. табл. 3 і рис. 4.";
    expect_sents("legal references",
                 legal_refs,
                 {"Див. п. 2 ч. 1 ст. 5 ЦК України та розд. IV документа.", "Пор. табл. 3 і рис. 4."});

    const std::string citation_chain =
        "Застосовано пп. 1.2.3 п. 4 підрозд. 2 розд. XX ПКУ та абз. 2 ч. 3 ст. 10 КАС України. Суд погодився.";
    expect_sents(
        "legal citation chain",
        citation_chain,
        {"Застосовано пп. 1.2.3 п. 4 підрозд. 2 розд. XX ПКУ та абз. 2 ч. 3 ст. 10 КАС України.", "Суд погодився."});

    const std::string ocr_double_dot =
        "Керуючись ст.. 28, 29 Кодексу про шлюб та сім'ю України, суд. Позов задовольнити.";
    expect_sents("ocr double dot abbreviation",
                 ocr_double_dot,
                 {"Керуючись ст.. 28, 29 Кодексу про шлюб та сім'ю України, суд.", "Позов задовольнити."});

    const std::string repeated_article =
        "На підставі викладеного суд керуючись ст.ст. 15, 30, 202, 62, 203 ЦПК та ст. 38, 40 Кодексу, суд. Рішив.";
    expect_sents("repeated article abbreviation",
                 repeated_article,
                 {"На підставі викладеного суд керуючись ст.ст. 15, 30, 202, 62, 203 ЦПК та ст. 38, 40 Кодексу, суд.",
                  "Рішив."});

    const std::string archive_sheets = "Матеріали справи підтверджено протоколом (а.с. 137). Суд оцінив докази.";
    expect_sents("archive sheet abbreviation",
                 archive_sheets,
                 {"Матеріали справи підтверджено протоколом (а.с. 137).", "Суд оцінив докази."});

    const std::string editorial_refs = "Див. пп. 2-3, ред. Іваненко, упоряд. Олена, мал. 2. Наступний розділ.";
    expect_sents("editorial references",
                 editorial_refs,
                 {"Див. пп. 2-3, ред. Іваненко, упоряд. Олена, мал. 2.", "Наступний розділ."});

    const std::string admin_refs =
        "Деп. Іваненко послався на пост. № 12, наказ. № 4, підп. 1, абз. 2, арк. 5, вип. 7, стор. 9. Комітет погодив.";
    expect_sents("admin abbreviations",
                 admin_refs,
                 {"Деп. Іваненко послався на пост. № 12, наказ. № 4, підп. 1, абз. 2, арк. 5, вип. 7, стор. 9.",
                  "Комітет погодив."});

    const std::string court_refs = "Ухв. суду у спр. № 42 позов. вимоги відп. заперечив, заявн. подав скаргу, оск. "
                                   "рішення триває. Наступне засідання завтра.";
    expect_sents("court abbreviations",
                 court_refs,
                 {"Ухв. суду у спр. № 42 позов. вимоги відп. заперечив, заявн. подав скаргу, оск. рішення триває.",
                  "Наступне засідання завтра."});

    const std::string legal_domains =
        "Адмін. справа, крим. провадж. № 5, цив. позов і госп. спір розглядаються окремо. Суд повідомив сторони.";
    expect_sents(
        "legal domain abbreviations",
        legal_domains,
        {"Адмін. справа, крим. провадж. № 5, цив. позов і госп. спір розглядаються окремо.", "Суд повідомив сторони."});

    const std::string case_numbers =
        "Справа № 910/1234/24, провадження № 61-12345св24 та ЄРДР. № 12024100000000000 об'єднані. Розгляд триває.";
    expect_sents("case number references",
                 case_numbers,
                 {"Справа № 910/1234/24, провадження № 61-12345св24 та ЄРДР. № 12024100000000000 об'єднані.",
                  "Розгляд триває."});

    const std::string party_labels =
        "Поз. Іваненко, відп. Петренко, тр. особа Сидоренко та скаржн. Коваль подали пояснення. Суд оголосив перерву.";
    expect_sents("party label abbreviations",
                 party_labels,
                 {"Поз. Іваненко, відп. Петренко, тр. особа Сидоренко та скаржн. Коваль подали пояснення.",
                  "Суд оголосив перерву."});

    const std::string government = "Постанова КМУ № 123 набирає чинності. ВРУ ухвалила закон.";
    expect_sents("government references", government, {"Постанова КМУ № 123 набирає чинності.", "ВРУ ухвалила закон."});

    const std::string courts_codes = "КК. України, КПК. України, ЦК. України, ЦПК. України, ГК. України, ГПК. України, "
                                     "КУпАП. України та КЗпП. України згадані у висновку. ВС. зазначив інше.";
    expect_sents("court code abbreviations",
                 courts_codes,
                 {"КК. України, КПК. України, ЦК. України, ЦПК. України, ГК. України, ГПК. України, КУпАП. України та "
                  "КЗпП. України згадані у висновку.",
                  "ВС. зазначив інше."});

    const std::string court_metadata = "Постанова КЦС. ВС від 15.06.2026 у справі № 910/1234/24 набрала чинності. "
                                       "Рішення ОАСК. від 01.01.2024 скасовано.";
    expect_sents("court metadata abbreviations",
                 court_metadata,
                 {"Постанова КЦС. ВС від 15.06.2026 у справі № 910/1234/24 набрала чинності.",
                  "Рішення ОАСК. від 01.01.2024 скасовано."});

    const std::string legal_quotes = "Закон України \"Про судоустрій і статус суддів\" № 1402-VIII діє. Постанова КМУ "
                                     "\"Про затвердження порядку\" № 123/2026-р опублікована.";
    expect_sents("legal quoted document names",
                 legal_quotes,
                 {"Закон України \"Про судоустрій і статус суддів\" № 1402-VIII діє.",
                  "Постанова КМУ \"Про затвердження порядку\" № 123/2026-р опублікована."});

    const std::string legal_numbering =
        "2.3.4. Порядок подання заяви. 1) Позивач подає докази. ґ) Інші документи додаються.";
    expect_sents("legal numbering",
                 legal_numbering,
                 {"2.3.4. Порядок подання заяви.", "1) Позивач подає докази.", "ґ) Інші документи додаються."});

    const std::string echr =
        "ECLI. UA:SC:2026:12345 та § 1 рішення у справі Ivanenko v. Ukraine враховано. Суд навів No. 12345/20.";
    expect_sents(
        "echr markers",
        echr,
        {"ECLI. UA:SC:2026:12345 та § 1 рішення у справі Ivanenko v. Ukraine враховано.", "Суд навів No. 12345/20."});

    const std::string institutions =
        "МОН. повідомило правила, МОЗ. уточнило дані, НБУ. оприлюднив прогноз, КМДА. додала графік. Звіт готовий.";
    expect_sents("institution abbreviations",
                 institutions,
                 {"МОН. повідомило правила, МОЗ. уточнило дані, НБУ. оприлюднив прогноз, КМДА. додала графік.",
                  "Звіт готовий."});

    const std::string money = "Компанія залучила 5 млн грн. Це на 12,5% більше, ніж торік.";
    expect_sents("money measurement", money, {"Компанія залучила 5 млн грн.", "Це на 12,5% більше, ніж торік."});

    const std::string soviet_money =
        "Стягнути 62 руб. 59 коп. різниці у частках. Паркан залишити для загального користування.";
    expect_sents("soviet money abbreviation",
                 soviet_money,
                 {"Стягнути 62 руб. 59 коп. різниці у частках.", "Паркан залишити для загального користування."});

    const std::string karbovanets_money =
        "Стягнути 592 крб. 59 к., держмито 419 крб. 78к. та судвитрати 209 крб. 85 к. Зобов'язати виконати рішення.";
    expect_sents("karbovanets money abbreviation",
                 karbovanets_money,
                 {"Стягнути 592 крб. 59 к., держмито 419 крб. 78к. та судвитрати 209 крб. 85 к.",
                  "Зобов'язати виконати рішення."});

    const std::string spaced_thousands =
        "Спальні - 3 - 19. 961.800 крб., спальні - 4 - 33.664.400 крб. на загальну суму. Визнати право.";
    expect_sents("spaced thousands amount",
                 spaced_thousands,
                 {"Спальні - 3 - 19. 961.800 крб., спальні - 4 - 33.664.400 крб. на загальну суму.", "Визнати право."});

    const std::string units =
        "Прилад має 5 мкм. точність, 10 нм. шар, 3 кВт. потужність, 2 шт. деталей і 4 екз. звіту. Вимір завершено.";
    expect_sents("unit abbreviations",
                 units,
                 {"Прилад має 5 мкм. точність, 10 нм. шар, 3 кВт. потужність, 2 шт. деталей і 4 екз. звіту.",
                  "Вимір завершено."});

    const std::string dates = "Засідання відбулося 15.06.2026 о 10:30:15. Звіт охоплює 1991–2024 рр.";
    expect_sents(
        "dates times ranges", dates, {"Засідання відбулося 15.06.2026 о 10:30:15.", "Звіт охоплює 1991–2024 рр."});

    const std::string initials = "Т. Г. Шевченко народився у с. Моринці. Його твори знають усі.";
    expect_sents("names initials", initials, {"Т. Г. Шевченко народився у с. Моринці.", "Його твори знають усі."});

    const std::string names = "І. Франко зустрів М. М. Коцюбинського. Леся Українка писала листи О. Кобилянській.";
    expect_sents("ukrainian names",
                 names,
                 {"І. Франко зустрів М. М. Коцюбинського.", "Леся Українка писала листи О. Кобилянській."});

    const std::string academic = "Асист. кафедри, викл. курсу, ст. викл. Іваненко, зав. лаб. Петренко, інж. Сидоренко "
                                 "та чл.-кор. НАН виступили. Питання обговорили.";
    expect_sents("academic abbreviations",
                 academic,
                 {"Асист. кафедри, викл. курсу, ст. викл. Іваненко, зав. лаб. Петренко, інж. Сидоренко та чл.-кор. НАН "
                  "виступили.",
                  "Питання обговорили."});

    const std::string degrees = "К. ф.-м. н. Іваненко і д. ю. н. Петренко підписали висновок. Документ оприлюднили.";
    expect_sents("degree abbreviations",
                 degrees,
                 {"К. ф.-м. н. Іваненко і д. ю. н. Петренко підписали висновок.", "Документ оприлюднили."});

    const std::string dialogue = "- Ти прийдеш? - Так, звісно. - Тоді чекаю.";
    expect_sents("dialogue", dialogue, {"- Ти прийдеш?", "- Так, звісно.", "- Тоді чекаю."});

    const std::string list = "1. Перший пункт. 2. Другий пункт.";
    expect_sents("numbered list", list, {"1. Перший пункт.", "2. Другий пункт."});

    const std::string nested_list = "1.1. Загальні положення. А. Перший пункт. а) підпункт.";
    expect_sents("nested list", nested_list, {"1.1. Загальні положення.", "А. Перший пункт.", "а) підпункт."});

    const std::string ellipsis = "Він замовк... А потім сказав: \"Ходімо!\"";
    expect_sents("ellipsis quotes", ellipsis, {"Він замовк...", "А потім сказав: \"Ходімо!\""});

    const std::string punctuation = "Справді?! Так!!! Невже?.. Побачимо…";
    expect_sents("punctuation clusters", punctuation, {"Справді?!", "Так!!!", "Невже?..", "Побачимо…"});

    const std::string social = "Класно :) Але що далі? Побачимо.";
    expect_sents("social smile", social, {"Класно :)", "Але що далі?", "Побачимо."});

    const std::string web_sentence = "Деталі на https://example.com/news?id=1. Пишіть на test.user@example.com.";
    expect_sents(
        "web sentence", web_sentence, {"Деталі на https://example.com/news?id=1.", "Пишіть на test.user@example.com."});

    const std::string mixed = "Команда OpenAI відкрила офіс у м. Київ. Це тестовий приклад.";
    expect_sents("mixed latin", mixed, {"Команда OpenAI відкрила офіс у м. Київ.", "Це тестовий приклад."});

    const std::string geography = "Експедиція прибула на остр. Зміїний, до річ. Дніпро, оз. Світязь, г. Говерла, "
                                  "станц. Лавочне і залізн. вузол. Повернення завтра.";
    expect_sents("geography abbreviations",
                 geography,
                 {"Експедиція прибула на остр. Зміїний, до річ. Дніпро, оз. Світязь, г. Говерла, станц. Лавочне і "
                  "залізн. вузол.",
                  "Повернення завтра."});

    const std::string compound_places =
        "Кур'єр заїхав у м-н. Центральний, ж/м. Тополя та в/ч. А1234. Маршрут завершено.";
    expect_sents("compound place abbreviations",
                 compound_places,
                 {"Кур'єр заїхав у м-н. Центральний, ж/м. Тополя та в/ч. А1234.", "Маршрут завершено."});

    const std::string prose = "Він читав і т.д. А вона слухала і т.п. Потім усі пішли.";
    expect_sents("prose abbreviations", prose, {"Він читав і т.д.", "А вона слухала і т.п.", "Потім усі пішли."});

    const std::string prose_more = "Це т. зв. тест, та ін. приклади, бл. 20 рядків, прибл. 5 сторінок, зокр. додаток і "
                                   "порівн. таблиця. Далі висновки.";
    expect_sents("more prose abbreviations",
                 prose_more,
                 {"Це т. зв. тест, та ін. приклади, бл. 20 рядків, прибл. 5 сторінок, зокр. додаток і порівн. таблиця.",
                  "Далі висновки."});

    const std::string publishing = "Док. 1 містить дод. А, прим. 2, перекл. автора, вид. 3, т. 1, тт. 2-3, зб. праць, "
                                   "зош. 4, журн. огляд і газ. вирізку. Архів оновлено.";
    expect_sents("publishing abbreviations",
                 publishing,
                 {"Док. 1 містить дод. А, прим. 2, перекл. автора, вид. 3, т. 1, тт. 2-3, зб. праць, зош. 4, журн. "
                  "огляд і газ. вирізку.",
                  "Архів оновлено."});

    const std::string era = "Археологи датують знахідку V ст. до н. е. Це змінило висновки.";
    expect_sents("era abbreviation", era, {"Археологи датують знахідку V ст. до н. е.", "Це змінило висновки."});

    return failures == 0 ? 0 : 1;
}
