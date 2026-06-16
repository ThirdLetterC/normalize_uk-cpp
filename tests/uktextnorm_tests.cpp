#include "uktextnorm/uktextnorm.hpp"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void expect_eq(const std::string& name, const std::string& actual, const std::string& expected)
{
    if (actual == expected) {
        return;
    }
    ++failures;
    std::cerr << name << "\nexpected: " << expected << "\nactual:   " << actual << "\n";
}

void expect_uncertain(const std::string& name,
                      const std::vector<uktextnorm::UncertainSpan>& spans,
                      std::size_t index,
                      std::size_t start,
                      std::size_t stop,
                      const std::string& text)
{
    if (spans.size() > index && spans[index].start == start && spans[index].stop == stop && spans[index].text == text) {
        return;
    }
    ++failures;
    std::cerr << name << "\nexpected span: " << start << ", " << stop << ", " << text << "\n";
    if (spans.size() <= index) {
        std::cerr << "actual: missing span, size " << spans.size() << "\n";
    } else {
        std::cerr << "actual: " << spans[index].start << ", " << spans[index].stop << ", " << spans[index].text << "\n";
    }
}

void expect_uncertain_contains(const std::string& name,
                               const std::vector<uktextnorm::UncertainSpan>& spans,
                               const std::string& text,
                               const std::string& reason_fragment)
{
    for (const auto& span : spans) {
        if (span.text == text && span.reason.contains(reason_fragment)) {
            return;
        }
    }
    ++failures;
    std::cerr << name << "\nexpected span containing: " << text << " / " << reason_fragment << "\n";
    std::cerr << "actual spans:\n";
    for (const auto& span : spans) {
        std::cerr << "  " << span.start << ", " << span.stop << ", " << span.text << ", " << span.reason << "\n";
    }
}

void expect_uncertain_metadata(const std::string& name,
                               const std::vector<uktextnorm::UncertainSpan>& spans,
                               const std::string& text,
                               uktextnorm::UncertaintyCategory category,
                               uktextnorm::UncertaintySeverity severity)
{
    for (const auto& span : spans) {
        if (span.text == text && span.category == category && span.severity == severity) {
            return;
        }
    }
    ++failures;
    std::cerr << name << "\nexpected metadata span: " << text << "\n";
}

void run_golden_file(const std::string& path)
{
    std::ifstream in(path);
    if (!in) {
        ++failures;
        std::cerr << "golden file\nexpected readable file: " << path << "\n";
        return;
    }
    std::string line;
    std::size_t row = 0;
    while (std::getline(in, line)) {
        ++row;
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const auto tab = line.find('\t');
        if (tab == std::string::npos) {
            ++failures;
            std::cerr << "golden file row " << row << "\nexpected tab-separated input/output\n";
            continue;
        }
        expect_eq("golden row " + std::to_string(row),
                  uktextnorm::normalize_ukrainian(line.substr(0, tab)),
                  line.substr(tab + 1));
    }
}

} // namespace

int main(int argc, char** argv)
{
    using uktextnorm::normalize_ukrainian;

    expect_eq("number",
              uktextnorm::number_to_words(1234567),
              "один мільйон двісті тридцять чотири тисячі п'ятсот шістдесят сім");
    expect_eq("ordinal", uktextnorm::number_to_ordinal_words(21, "nom_f"), "двадцять перша");
    expect_eq("case", uktextnorm::number_to_words_case(500, "gen"), "п'ятисот");
    expect_eq("abbr", uktextnorm::normalize_abbreviations("І т. д. і т. ін."), "і так далі і таке інше");
    expect_eq("acronym", uktextnorm::expand_abbreviations("СБР і НАТО"), "ес бе ер і НАТО");
    expect_eq("cyrilize", uktextnorm::cyrilize("Google shop"), "гугле шоп");

    expect_eq("date", normalize_ukrainian("01.05.2024"), "перше травня дві тисячі двадцять четвертого року");
    expect_eq("time", normalize_ukrainian("Зустріч о 06:06"), "Зустріч о шість годин шість хвилин");
    expect_eq("currency", normalize_ukrainian("Ціна 12.50 грн"), "Ціна дванадцять гривень п'ятдесят копійок");
    expect_eq("measure", normalize_ukrainian("5 кг і 2 хв"), "п'ять кілограмів і дві хвилини");
    expect_eq("web", normalize_ukrainian("test@example.com"), "тест равлик ексампле крапка ком");
    expect_eq("mixed",
              normalize_ukrainian("Python 3.11, GPS, 50%"),
              "пайтон три крапка одинадцять, джі пі ес, п'ятдесят відсотків");
    expect_eq("phone",
              normalize_ukrainian("+380 67 123-45-67"),
              "плюс триста вісімдесят шістдесят сім сто двадцять три сорок п'ять шістдесят сім");
    expect_eq("local phone",
              normalize_ukrainian("067-123-45-67"),
              "плюс триста вісімдесят шістдесят сім сто двадцять три сорок п'ять шістдесят сім");
    expect_eq("address",
              normalize_ukrainian("м. Київ, вул. Хрещатик, буд. 1, кв. 7"),
              "місто Київ, вулиця Хрещатик, будинок один, квартира сім");
    expect_eq(
        "year range", normalize_ukrainian("2020-2024 рр."), "дві тисячі двадцятий дві тисячі двадцять четвертий роки");
    expect_eq("case year", normalize_ukrainian("у 2024 році"), "у дві тисячі двадцять четвертому році");
    expect_eq("ordinal suffix", normalize_ukrainian("1991-го"), "тисяча дев'ятсот дев'яносто першого");
    expect_eq("roman century", normalize_ukrainian("XXI ст."), "двадцять перше століття");
    expect_eq("unit range", normalize_ukrainian("5-7 кг"), "п'ять сім кілограмів");
    expect_eq("percent range", normalize_ukrainian("10-15%"), "десять п'ятнадцять відсотків");
    expect_eq("multiplier", normalize_ukrainian("2 млн користувачів"), "два мільйони користувачів");
    expect_eq("multiplier currency", normalize_ukrainian("2 млн грн"), "два мільйони гривень");
    expect_eq("symbol multiplier currency", normalize_ukrainian("$3 млн"), "три мільйони доларів");
    expect_eq(
        "known acronym", normalize_ukrainian("ФОП і ПДВ"), "Фізична особа підприємець і податок на додану вартість");
    expect_eq("known acronym sentence casing",
              normalize_ukrainian("КМУ ухвалив. ПДВ 20%"),
              "Кабінет міністрів України ухвалив. Податок на додану вартість двадцять відсотків");
    expect_eq("known acronym mid sentence casing",
              normalize_ukrainian("Постанова КМУ № 123/2026-р"),
              "Постанова кабінет міністрів України номер сто двадцять три слеш дві тисячі двадцять шість дефіс ер");
    expect_eq("institution acronyms",
              normalize_ukrainian("МОН, МОЗ і НБУ"),
              "Міністерство освіти і науки України, міністерство охорони здоров'я України і національний банк України");
    expect_eq("law enforcement acronyms",
              normalize_ukrainian("НАБУ, САП, ДБР і СБУ"),
              "Національне антикорупційне бюро України, спеціалізована антикорупційна прокуратура, державне бюро "
              "розслідувань і служба безпеки України");
    expect_eq("business admin acronyms",
              normalize_ukrainian("ТОВ, АТ, ОСББ, ОВА і РДА"),
              "Товариство з обмеженою відповідальністю, акціонерне товариство, об'єднання співвласників "
              "багатоквартирного будинку, обласна військова адміністрація і районна державна адміністрація");
    expect_eq("preposition genitive", normalize_ukrainian("до 5 кг"), "до п'яти кілограмів");
    expect_eq("instrumental context", normalize_ukrainian("з 3 друзями"), "з трьома друзями");
    expect_eq("prepositional oblique", normalize_ukrainian("у 4 містах"), "у чотирьох містах");
    expect_eq("counted masculine noun", normalize_ukrainian("21 користувач"), "двадцять один користувач");
    expect_eq("counted feminine noun", normalize_ukrainian("22 заявки"), "двадцять дві заявки");
    expect_eq("counted neuter noun", normalize_ukrainian("21 місто"), "двадцять одне місто");
    expect_eq("counted irregular person", normalize_ukrainian("5 людей"), "п'ять людей");
    expect_eq("counted irregular child", normalize_ukrainian("12 дітей"), "дванадцять дітей");
    expect_eq("counted document plural", normalize_ukrainian("104 документи"), "сто чотири документи");
    expect_eq("counted noun after ponad", normalize_ukrainian("понад 21 місто"), "понад двадцять одне місто");
    expect_eq("counted noun genitive governor", normalize_ukrainian("до 5 осіб"), "до п'яти осіб");
    expect_eq("counted noun approximate governor", normalize_ukrainian("близько 12 дітей"), "близько дванадцяти дітей");
    expect_eq("ordinal class", normalize_ukrainian("3 клас"), "третій клас");
    expect_eq("ordinal place", normalize_ukrainian("2 місце"), "друге місце");
    expect_eq("compound adjective", normalize_ukrainian("5-річний план"), "п'ятирічний план");
    expect_eq("number groups",
              normalize_ukrainian("1 234 567 грн"),
              "один мільйон двісті тридцять чотири тисячі п'ятсот шістдесят сім гривень");
    expect_eq("legal sections",
              normalize_ukrainian("ч. 2 ст. 19, п. 3 розд. II"),
              "частина два стаття дев'ятнадцять, пункт три розділ другий");
    expect_eq("century not section", normalize_ukrainian("XXI ст."), "двадцять перше століття");
    expect_eq("slash date", normalize_ukrainian("15/06/2026"), "п'ятнадцяте червня дві тисячі двадцять шостого року");
    expect_eq("iso date", normalize_ukrainian("2026-06-15"), "п'ятнадцяте червня дві тисячі двадцять шостого року");
    expect_eq(
        "abbrev month", normalize_ukrainian("15 черв. 2026"), "п'ятнадцятого червня дві тисячі двадцять шостого року");
    expect_eq("day month date range",
              normalize_ukrainian("15-16 червня 2026"),
              "п'ятнадцятого шістнадцятого червня дві тисячі двадцять шостого року");
    expect_eq(
        "numeric date range",
        normalize_ukrainian("15.06.2026-16.06.2026"),
        "п'ятнадцяте червня дві тисячі двадцять шостого року шістнадцяте червня дві тисячі двадцять шостого року");
    expect_eq("roman century range", normalize_ukrainian("XIX-XX ст."), "дев'ятнадцяте двадцяте століття");
    expect_eq("roman section range", normalize_ukrainian("I-IV розд."), "перший четвертий розділ");
    expect_eq("roman quarter", normalize_ukrainian("II кв. 2026"), "другий квартал дві тисячі двадцять шостого року");
    expect_eq("numeric quarter", normalize_ukrainian("2-й кв."), "другий квартал");
    expect_eq("apartment still address", normalize_ukrainian("кв. 7"), "квартира сім");
    expect_eq("marked hour", normalize_ukrainian("о 6-й"), "о шоста година");
    expect_eq("time part", normalize_ukrainian("6:00 ранку"), "шість годин ранку");
    expect_eq("comma currency",
              normalize_ukrainian("1 234,56 грн"),
              "тисяча двісті тридцять чотири гривні п'ятдесят шість копійок");
    expect_eq("symbol prefix currency",
              normalize_ukrainian("₴1234.56"),
              "тисяча двісті тридцять чотири гривні п'ятдесят шість копійок");
    expect_eq("dot decimal measure", normalize_ukrainian("2.5 кг"), "дві цілих і п'ять десятих кілограми");
    expect_eq("dot decimal percent", normalize_ukrainian("12.5%"), "дванадцять цілих і п'ять десятих відсотка");
    expect_eq("dot decimal multiplier currency",
              normalize_ukrainian("1.5 млн грн"),
              "одна ціла і п'ять десятих мільйони гривень");
    expect_eq("iban",
              normalize_ukrainian("UA213223130000026007233566001"),
              "айбан ю ей два один три два два три один три нуль нуль нуль нуль нуль два шість нуль нуль сім два три "
              "три п'ять шість шість нуль нуль один");
    expect_eq("edrpou",
              normalize_ukrainian("ЄДРПОУ 12345678"),
              "єдиний державний реєстр підприємств та організацій України один два три чотири п'ять шість сім вісім");
    expect_eq("postcode", normalize_ukrainian("індекс 01001"), "індекс нуль один нуль нуль один");
    expect_eq("tax id",
              normalize_ukrainian("РНОКПП 1234567890"),
              "рнокпп один два три чотири п'ять шість сім вісім дев'ять нуль");
    expect_eq("vehicle plate", normalize_ukrainian("АА 1234 КВ"), "номерний знак а а один два три чотири ка ве");
    expect_eq(
        "crypto amount", normalize_ukrainian("0,5 BTC і 2 ETH"), "нуль цілих і п'ять десятих біткоїнів і два ефіри");
    expect_eq(
        "exchange pair", normalize_ukrainian("BTC/UAH та USD/UAH"), "біткоїнів до гривень та доларів США до гривень");
    expect_eq("social handle", normalize_ukrainian("@OpenAI"), "акаунт опеней");
    expect_eq("brand exceptions",
              normalize_ukrainian("OpenAI, ChatGPT, GitHub і MacBook"),
              "опеней, чатджипіті, гітхаб і макбук");
    expect_eq("product exceptions",
              normalize_ukrainian("iPhone, YouTube, Docker і Kubernetes"),
              "айфон, ютуб, докер і кубернетіс");
    expect_eq(
        "url query",
        normalize_ukrainian("https://example.com/a?x=1&y=2"),
        "гттпс двокрапка слеш слеш ексампле крапка ком слеш а знак питання кс дорівнює один амперсанд и дорівнює два");
    expect_eq("court case number",
              normalize_ukrainian("справа № 910/1234/24"),
              "справа номер дев'ятсот десять слеш тисяча двісті тридцять чотири слеш двадцять чотири");
    expect_eq("proceeding number",
              normalize_ukrainian("провадження № 61-12345св24"),
              "провадження номер шістдесят один дефіс один два три чотири п'ять ес ве двадцять чотири");
    expect_eq("government resolution number",
              normalize_ukrainian("Постанова КМУ № 123/2026-р"),
              "Постанова кабінет міністрів України номер сто двадцять три слеш дві тисячі двадцять шість дефіс ер");
    expect_eq("inflected case legal number",
              normalize_ukrainian("у справі № 910/1234/24"),
              "у справі номер дев'ятсот десять слеш тисяча двісті тридцять чотири слеш двадцять чотири");
    expect_eq(
        "law roman suffix", normalize_ukrainian("Закон № 1402-VIII"), "Закон номер тисяча чотириста два дефіс восьмий");
    expect_eq("law genitive roman suffix",
              normalize_ukrainian("Закону № 1402-VIII"),
              "Закону номер тисяча чотириста два дефіс восьмий");
    expect_eq("erdr number",
              normalize_ukrainian("ЄРДР. № 12024100000000000"),
              "єдиний реєстр досудових розслідувань номер один два нуль два чотири один нуль нуль нуль нуль нуль нуль "
              "нуль нуль нуль нуль нуль");
    expect_eq(
        "passport number", normalize_ukrainian("паспорт КВ 123456"), "паспорт ка ве один два три чотири п'ять шість");
    expect_eq("masked card",
              normalize_ukrainian("картка 4149 **** **** 1234"),
              "картка чотири один чотири дев'ять зірочки зірочки один два три чотири");
    expect_eq("masked card accusative",
              normalize_ukrainian("на картку 4149 **** **** 1234"),
              "на картку чотири один чотири дев'ять зірочки зірочки один два три чотири");
    expect_eq("full card grouped",
              normalize_ukrainian("картка 4149 1234 5678 9012"),
              "картка чотири один чотири дев'ять один два три чотири п'ять шість сім вісім дев'ять нуль один два");
    expect_eq("order number reference", normalize_ukrainian("Замовлення №10"), "Замовлення номер десять");
    expect_eq("medical concentration", normalize_ukrainian("5 мг/мл"), "п'ять міліграмів на мілілітр");
    expect_eq("dot decimal medical concentration",
              normalize_ukrainian("5.5 мг/мл"),
              "п'ять цілих і п'ять десятих міліграми на мілілітр");
    expect_eq("medical frequency", normalize_ukrainian("2 рази на день"), "два рази на день");
    expect_eq(
        "medical temperature", normalize_ukrainian("37,5°C"), "тридцять сім цілих і п'ять десятих градуса цельсія");
    expect_eq("dot decimal medical temperature",
              normalize_ukrainian("37.5°C"),
              "тридцять сім цілих і п'ять десятих градуса цельсія");
    expect_eq("blood pressure",
              normalize_ukrainian("120/80 мм рт. ст."),
              "сто двадцять на вісімдесят міліметрів ртутного стовпа");
    expect_eq("labelled blood pressure",
              normalize_ukrainian("тиск 120/80 мм рт. ст."),
              "тиск сто двадцять на вісімдесят міліметрів ртутного стовпа");
    expect_eq("package number", normalize_ukrainian("препарат №10"), "препарат номер десять");

    uktextnorm::NormalizeOptions conservative;
    conservative.expand_known_acronyms = false;
    conservative.spell_unknown_acronyms = false;
    conservative.normalize_english_words = false;
    conservative.transliterate_latin = false;
    expect_eq("conservative options", normalize_ukrainian("Python 3 і ФОП", conservative), "Python три і ФОП");
    expect_eq("conservative brand options", normalize_ukrainian("OpenAI і ChatGPT", conservative), "OpenAI і ChatGPT");
    uktextnorm::NormalizeOptions range_options;
    range_options.range_style = uktextnorm::RangeStyle::FromTo;
    expect_eq("from-to unit range", normalize_ukrainian("5-7 кг", range_options), "від п'яти до семи кілограмів");
    expect_eq(
        "from-to percent range", normalize_ukrainian("10-15%", range_options), "від десяти до п'ятнадцяти відсотків");
    uktextnorm::NormalizeOptions phone_options;
    phone_options.phone_style = uktextnorm::PhoneStyle::DigitByDigit;
    expect_eq("phone digit by digit",
              normalize_ukrainian("+380 67 123-45-67", phone_options),
              "плюс три вісім нуль шість сім один два три чотири п'ять шість сім");
    uktextnorm::NormalizeOptions symbol_options;
    symbol_options.symbol_style = uktextnorm::SymbolStyle::Preserve;
    expect_eq("preserve symbols",
              normalize_ukrainian("2 + 2 = 4 і 50%", symbol_options),
              "два + два = чотири і п'ятдесят відсотків");
    expect_eq("expand symbols default", normalize_ukrainian("2 + 2 = 4"), "два плюс два дорівнює чотири");
    uktextnorm::NormalizeOptions spoken_dates;
    spoken_dates.date_style = uktextnorm::DateStyle::Spoken;
    expect_eq("spoken numeric date",
              normalize_ukrainian("15.06.2026", spoken_dates),
              "п'ятнадцятого червня дві тисячі двадцять шостого року");
    expect_eq("spoken iso date",
              normalize_ukrainian("2026-06-15", spoken_dates),
              "п'ятнадцятого червня дві тисячі двадцять шостого року");
    expect_eq(
        "spoken numeric date range",
        normalize_ukrainian("15.06.2026-16.06.2026", spoken_dates),
        "п'ятнадцятого червня дві тисячі двадцять шостого року шістнадцятого червня дві тисячі двадцять шостого року");
    uktextnorm::NormalizeOptions tts_options;
    tts_options.range_style = uktextnorm::RangeStyle::FromTo;
    tts_options.phone_style = uktextnorm::PhoneStyle::DigitByDigit;
    tts_options.date_style = uktextnorm::DateStyle::Spoken;
    expect_eq("tts preset",
              normalize_ukrainian("15.06.2026, +380 67 123-45-67, 5-7 кг", uktextnorm::NormalizePreset::TtsFriendly),
              normalize_ukrainian("15.06.2026, +380 67 123-45-67, 5-7 кг", tts_options));
    expect_eq("explicit preset API",
              uktextnorm::normalize_ukrainian_with_preset("OpenAI + ФОП", uktextnorm::NormalizePreset::SearchIndexing),
              "OpenAI + фізична особа підприємець");
    expect_eq("preset helper conservative",
              normalize_ukrainian("OpenAI + ФОП", uktextnorm::NormalizePreset::Conservative),
              "OpenAI + ФОП");
    expect_eq("preset helper search",
              normalize_ukrainian("OpenAI + ФОП", uktextnorm::NormalizePreset::SearchIndexing),
              "OpenAI + фізична особа підприємець");
    expect_eq("oversized standalone number",
              normalize_ukrainian("Номер 123456789012345678901234567890"),
              "Номер один два три чотири п'ять шість сім вісім дев'ять нуль один два три чотири п'ять шість сім вісім "
              "дев'ять нуль один два три чотири п'ять шість сім вісім дев'ять нуль");
    expect_eq("oversized dotted version",
              normalize_ukrainian("Версія 999999999999999999999999.1.2"),
              "Версія дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять "
              "дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять крапка "
              "один крапка два");
    expect_eq(
        "oversized percent",
        normalize_ukrainian("Знижка 999999999999999999999999%"),
        "Знижка дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять "
        "дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять відсотків");
    expect_eq(
        "oversized measurement",
        normalize_ukrainian("Вага 999999999999999999999999 кг"),
        "Вага дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять "
        "дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять дев'ять кілограмів");
    expect_eq("overprecise currency decimal",
              normalize_ukrainian("Сума 1,23456789 грн"),
              "Сума один кома два три чотири п'ять шість сім вісім дев'ять грн");

    const auto uncertain = uktextnorm::flag_uncertain("У 2024 вийшов Foo X.");
    expect_uncertain("uncertain year", uncertain, 0, 2, 6, "2024");
    expect_uncertain("uncertain latin", uncertain, 1, 14, 17, "Foo");
    expect_uncertain_metadata("uncertain latin metadata",
                              uncertain,
                              "Foo",
                              uktextnorm::UncertaintyCategory::ForeignWord,
                              uktextnorm::UncertaintySeverity::Info);
    const auto more_uncertain = uktextnorm::flag_uncertain("Подія 32.13.2024. Див. ст. 5 та FooКиїв IX.");
    expect_uncertain_contains("uncertain invalid date", more_uncertain, "32.13.2024", "numeric date");
    expect_uncertain_metadata("uncertain invalid date metadata",
                              more_uncertain,
                              "32.13.2024",
                              uktextnorm::UncertaintyCategory::Date,
                              uktextnorm::UncertaintySeverity::Error);
    expect_uncertain_contains("uncertain ambiguous abbreviation", more_uncertain, "ст.", "ambiguous abbreviation");
    expect_uncertain_contains(
        "uncertain bare number", uktextnorm::flag_uncertain("Є 7 варіантів."), "7", "bare number");
    expect_uncertain_contains("uncertain mixed word", more_uncertain, "FooКиїв", "mixed-script");
    expect_uncertain_metadata("uncertain mixed metadata",
                              more_uncertain,
                              "FooКиїв",
                              uktextnorm::UncertaintyCategory::MixedScript,
                              uktextnorm::UncertaintySeverity::Error);
    expect_uncertain_contains("uncertain roman", more_uncertain, "IX", "Roman numeral");
    expect_uncertain_metadata("uncertain identifier metadata",
                              uktextnorm::flag_uncertain("справа № 910/1234/24"),
                              "№ 910/1234/24",
                              uktextnorm::UncertaintyCategory::Identifier,
                              uktextnorm::UncertaintySeverity::Info);
    expect_uncertain_metadata("uncertain full card metadata",
                              uktextnorm::flag_uncertain("картка 4149 1234 5678 9012"),
                              "картка 4149 1234 5678 9012",
                              uktextnorm::UncertaintyCategory::Identifier,
                              uktextnorm::UncertaintySeverity::Info);
    expect_uncertain_metadata("uncertain currency metadata",
                              uktextnorm::flag_uncertain("Сума 12 PLN."),
                              "12 PLN",
                              uktextnorm::UncertaintyCategory::Currency,
                              uktextnorm::UncertaintySeverity::Warning);
    expect_uncertain_metadata("uncertain unit metadata",
                              uktextnorm::flag_uncertain("Вага 5 qq."),
                              "5 qq",
                              uktextnorm::UncertaintyCategory::Unit,
                              uktextnorm::UncertaintySeverity::Warning);
    expect_uncertain_metadata("uncertain email metadata",
                              uktextnorm::flag_uncertain("Контакт test@"),
                              "test@",
                              uktextnorm::UncertaintyCategory::Web,
                              uktextnorm::UncertaintySeverity::Warning);
    expect_uncertain_metadata("uncertain url metadata",
                              uktextnorm::flag_uncertain("Перейти на https://"),
                              "https://",
                              uktextnorm::UncertaintyCategory::Web,
                              uktextnorm::UncertaintySeverity::Warning);

    for (int i = 1; i < argc; ++i) {
        run_golden_file(argv[i]);
    }

    return failures == 0 ? 0 : 1;
}
