#include "uktextnorm/uktextnorm.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
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

void expect_not_contains(const std::string& name, const std::string& actual, const std::string& unexpected)
{
    if (!actual.contains(unexpected)) {
        return;
    }
    ++failures;
    std::cerr << name << "\nunexpected fragment: " << unexpected << "\nactual: " << actual << "\n";
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

void expect_no_uncertain_metadata(const std::string& name,
                                  const std::vector<uktextnorm::UncertainSpan>& spans,
                                  const std::string& text,
                                  uktextnorm::UncertaintySeverity severity)
{
    for (const auto& span : spans) {
        if (span.text == text && span.severity == severity) {
            ++failures;
            std::cerr << name << "\nunexpected metadata span: " << text << "\n";
            return;
        }
    }
}

void expect_no_uncertain_category(const std::string& name,
                                  const std::vector<uktextnorm::UncertainSpan>& spans,
                                  uktextnorm::UncertaintyCategory category)
{
    for (const auto& span : spans) {
        if (span.category == category) {
            ++failures;
            std::cerr << name << "\nunexpected category span: " << span.text << "\n";
            return;
        }
    }
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
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
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
    expect_eq("ordinal ins", uktextnorm::number_to_ordinal_words(5, "ins"), "п'ятим");
    expect_eq("ordinal ins soft", uktextnorm::number_to_ordinal_words(3, "ins"), "третім");
    expect_eq("ordinal ins fem", uktextnorm::number_to_ordinal_words(2, "ins_f"), "другою");
    expect_eq("ordinal loc fem", uktextnorm::number_to_ordinal_words(40, "loc_f"), "сороковій");
    expect_eq("ordinal ins plural", uktextnorm::number_to_ordinal_words(100, "ins_pl"), "сотими");
    expect_eq("ordinal loc", uktextnorm::number_to_ordinal_words(1000, "loc"), "тисячному");
    expect_eq("case", uktextnorm::number_to_words_case(500, "gen"), "п'ятисот");
    expect_eq("abbr", uktextnorm::normalize_abbreviations("І т. д. і т. ін."), "І так далі і таке інше.");
    expect_eq("acronym", uktextnorm::expand_abbreviations("СБР і НАТО"), "ес бе ер і НАТО");
    expect_eq("transliterate to cyrillic", uktextnorm::transliterate_to_cyrillic("Google shop"), "гугле шоп");
    expect_eq(
        "cyrilize alias", uktextnorm::cyrilize("Google shop"), uktextnorm::transliterate_to_cyrillic("Google shop"));

    expect_eq("date", normalize_ukrainian("01.05.2024"), "перше травня дві тисячі двадцять четвертого року");
    expect_eq("textual date consumes explicit year word",
              normalize_ukrainian("Подію завершили 1 вересня 1969 року."),
              "Подію завершили першого вересня тисяча дев'ятсот шістдесят дев'ятого року.");
    expect_eq("named month year consumes explicit year word",
              normalize_ukrainian("Дані за березень 2009 року."),
              "Дані за березень дві тисячі дев'ятого року.");
    expect_eq("genitive named month year",
              normalize_ukrainian("За даними березня 2009 року."),
              "За даними березня дві тисячі дев'ятого року.");
    expect_eq("instrumental year",
              normalize_ukrainian("Порівняно із 2018 роком."),
              "Порівняно із дві тисячі вісімнадцятим роком.");
    expect_eq("contextual abbreviated year",
              normalize_ukrainian("У 1993 р. оприлюднили звіт."),
              "У тисяча дев'ятсот дев'яносто третьому році оприлюднили звіт.");
    expect_eq("locative month and year",
              normalize_ukrainian("Подію провели в березні 2001."),
              "Подію провели в березні дві тисячі першого року.");
    expect_eq("decade without written suffix",
              normalize_ukrainian("У 1940 роках створили перші системи."),
              "У тисяча дев'ятсот сорокових роках створили перші системи.");
    expect_eq("time", normalize_ukrainian("Зустріч о 06:06"), "Зустріч о шостій годині шість хвилин");
    expect_eq("currency", normalize_ukrainian("Ціна 12.50 грн"), "Ціна дванадцять гривень п'ятдесят копійок");
    expect_eq("measure", normalize_ukrainian("5 кг і 2 хв"), "п'ять кілограмів і дві хвилини");
    expect_eq("latin measurement symbols",
              normalize_ukrainian("5 cm і 2 MHz"),
              "п'ять сантиметрів і два мегагерци");
    expect_eq("micro-unit symbols",
              normalize_ukrainian("3 μL і 2 µg"),
              "три мікролітри і два мікрограми");
    expect_eq("new counted nouns",
              normalize_ukrainian("2 книги і 21 сторінка"),
              "дві книги і двадцять одна сторінка");
    expect_eq("broader counted nouns",
              normalize_ukrainian("2 програми і 5 повідомлень"),
              "дві програми і п'ять повідомлень");
    expect_eq("locative noun forms",
              normalize_ukrainian("У 3 програмах, на 7 сторінках та з 5 документами"),
              "У трьох програмах, на семи сторінках та з п'ятьма документами");
    expect_eq("article count outside legal labels", normalize_ukrainian("2 статті"), "дві статті");
    expect_eq("mixed fraction before counted noun",
              normalize_ukrainian("2 3/4 книги"),
              "дві і три четвертих книги");
    expect_eq("expanded brand and technical readings",
              normalize_ukrainian("Adobe, Firefox, browser та plugin"),
              "адобі, фаєрфокс, браузер та плагін");
    expect_eq("additional brand and technical readings",
              normalize_ukrainian("Figma, Viber, Diia і compiler"),
              "фігма, вайбер, дія і компайлер");
    expect_eq("additional rate units",
              normalize_ukrainian("3 KB/s і 5 µg/m³"),
              "три кілобайти за секунду і п'ять мікрограмів на кубічний метр");
    expect_eq("expanded acronym readings",
              normalize_ukrainian("ШІ та ООН"),
              "Штучний інтелект та організація об'єднаних націй");
    expect_eq("capitalized abbreviation expansion", normalize_ukrainian("Див. табл. 2"), "Дивись таблиця два");
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
    expect_eq("city abbreviation after locative preposition",
              normalize_ukrainian("Офіс у м. Києві."),
              "Офіс у місті Києві.");
    expect_eq("city abbreviation after genitive preposition",
              normalize_ukrainian("Дуга простягається від м. Гаммерфест."),
              "Дуга простягається від міста Гаммерфест.");
    expect_eq("unambiguous genitive city abbreviation",
              normalize_ukrainian("На 15 км проспекту м. Києва."),
              "На п'ятнадцять кілометрів проспекту міста Києва.");
    expect_eq("ordinal with full neuter suffix",
              normalize_ukrainian("Тернопіль займає 1-ше місце."),
              "Тернопіль займає перше місце.");
    expect_eq("ordinal with full second and third suffixes",
              normalize_ukrainian("2-ге і 3-тє місця."),
              "друге і третє місця.");
    expect_eq(
        "year range", normalize_ukrainian("2020-2024 рр."), "дві тисячі двадцятий дві тисячі двадцять четвертий роки.");
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
    expect_eq("ambiguous preposition with quantity", normalize_ukrainian("у 100 разів"), "у сто разів");
    expect_eq("accusative quantity after na", normalize_ukrainian("на 49 кубітів"), "на сорок дев'ять кубітів");
    expect_eq("genitive quantity after sered", normalize_ukrainian("серед 42 творів"), "серед сорока двох творів");
    expect_eq(
        "genitive quantity after z", normalize_ukrainian("приблизно з 50 кубітів"), "приблизно з п'ятдесяти кубітів");
    expect_eq("comparative phrase after governor",
              normalize_ukrainian("після більш ніж 5 років"),
              "після більш ніж п'яти років");
    expect_eq("quantity after ponad", normalize_ukrainian("понад 1200 кубітів"), "понад тисячу двісті кубітів");
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
    expect_eq("time with seconds",
              normalize_ukrainian("Зустріч о 12:34:56"),
              "Зустріч о дванадцять годин тридцять чотири хвилини п'ятдесят шість секунд");
    expect_eq("comma currency",
              normalize_ukrainian("1 234,56 грн"),
              "тисяча двісті тридцять чотири гривні п'ятдесят шість копійок");
    expect_eq("symbol prefix currency",
              normalize_ukrainian("₴1234.56"),
              "тисяча двісті тридцять чотири гривні п'ятдесят шість копійок");
    expect_eq("dot decimal measure", normalize_ukrainian("2.5 кг"), "дві цілих і п'ять десятих кілограма");
    expect_eq("dot decimal percent", normalize_ukrainian("12.5%"), "дванадцять цілих і п'ять десятих відсотка");
    expect_eq("genitive governed percent",
              normalize_ukrainian("Зростання становить близько 67 %."),
              "Зростання становить близько шістдесяти семи відсотків.");
    expect_eq("accusative percent increase",
              normalize_ukrainian("Показник зріс на 33 %."),
              "Показник зріс на тридцять три відсотки.");
    expect_eq("genitive percent upper bound",
              normalize_ukrainian("Частка піднялася до 82 %."),
              "Частка піднялася до вісімдесяти двох відсотків.");
    expect_eq("dot decimal multiplier currency",
              normalize_ukrainian("1.5 млн грн"),
              "одна ціла і п'ять десятих мільйона гривень");
    expect_eq("governed multiplier",
              normalize_ukrainian("близько 327 млн користувачів"),
              "близько трьохсот двадцяти семи мільйонів користувачів");
    expect_eq(
        "terminal multiplier punctuation", normalize_ukrainian("Користувачів 5 млн."), "Користувачів п'ять мільйонів.");
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
        "crypto amount", normalize_ukrainian("0,5 BTC і 2 ETH"), "нуль цілих і п'ять десятих біткоїна і два ефіри");
    expect_eq(
        "exchange pair", normalize_ukrainian("BTC/UAH та USD/UAH"), "біткоїнів до гривень та доларів США до гривень");
    expect_eq("social handle", normalize_ukrainian("@OpenAI"), "акаунт опеней");
    expect_eq("brand exceptions",
              normalize_ukrainian("OpenAI, ChatGPT, GitHub і MacBook"),
              "опеней, чатджипіті, гітхаб і макбук");
    expect_eq("product exceptions",
              normalize_ukrainian("iPhone, YouTube, Docker і Kubernetes"),
              "айфон, ютуб, докер і кубернетіс");
    uktextnorm::NormalizeOptions custom_words;
    custom_words.vocabulary = {{"google", "гуголь"}, {"acme", "акме"}};
    expect_eq("custom vocabulary overrides built-in word", normalize_ukrainian("Google і Acme", custom_words),
              "гуголь і акме");
    expect_eq("custom vocabulary stays local to options", normalize_ukrainian("Google"), "гугл");
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
              "п'ять цілих і п'ять десятих міліграма на мілілітр");
    expect_eq("medical frequency", normalize_ukrainian("2 рази на день"), "два рази на день");
    expect_eq(
        "medical temperature", normalize_ukrainian("37,5°C"), "тридцять сім цілих і п'ять десятих градуса Цельсія");
    expect_eq("dot decimal medical temperature",
              normalize_ukrainian("37.5°C"),
              "тридцять сім цілих і п'ять десятих градуса Цельсія");
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
        "from-to en dash unit range", normalize_ukrainian("5–7 кг", range_options), "від п'яти до семи кілограмів");
    expect_eq(
        "from-to percent range", normalize_ukrainian("10-15%", range_options), "від десяти до п'ятнадцяти відсотків");
    expect_eq("prepositional year range",
              normalize_ukrainian("У 1998—2000 роках.", range_options),
              "У період від тисяча дев'ятсот дев'яносто восьмого до двохтисячного року.");
    expect_eq("prepositional bare year range",
              normalize_ukrainian("У 1950—1951, за рекомендацією, було обране місце.", range_options),
              "У період від тисяча дев'ятсот п'ятдесятого до тисяча дев'ятсот п'ятдесят першого року, за "
              "рекомендацією, було обране місце.");
    expect_eq("range after explicit vid",
              normalize_ukrainian("Енергія менша від 1,5–2 еВ.", range_options),
              "Енергія менша від однієї цілої і п'яти десятих до двох електронвольтів.");
    expect_eq("approximate range after ponad",
              normalize_ukrainian("понад 300—400 рядків", range_options),
              "понад триста чи чотириста рядків");
    for (const auto input : {"5-7 °C",
                             "5–7 °C",
                             "5—7 °C",
                             "5 - 7 °C",
                             "5-7°C",
                             "5–7°C",
                             "5–7 °С",
                             "5–7 °с",
                             "5-7 градусів Цельсія",
                             "5–7 градусів Цельсія",
                             "5-7 градусів цельсія",
                             "5-7 ГРАДУСІВ ЦЕЛЬСІЯ",
                             "5-7 градусів C",
                             "5–7 градусів за Цельсієм"}) {
        expect_eq("from-to temperature range " + std::string(input),
                  normalize_ukrainian(input, range_options),
                  "від п'яти до семи градусів Цельсія");
    }
    expect_eq("decimal temperature range",
              normalize_ukrainian("5,5–7,5 °C", range_options),
              "від п'яти цілих і п'яти десятих до семи цілих і п'яти десятих градуса Цельсія");
    expect_eq("fahrenheit temperature range",
              normalize_ukrainian("5–7 °F", range_options),
              "від п'яти до семи градусів Фаренгейта");
    expect_eq("unicode temperature symbols",
              normalize_ukrainian("5–7 ℃ і 8–9 ℉", range_options),
              "від п'яти до семи градусів Цельсія і від восьми до дев'яти градусів Фаренгейта");
    expect_eq("standalone kelvin", normalize_ukrainian("273 K", range_options), "двісті сімдесят три кельвіни");
    expect_eq("governed kelvin",
              normalize_ukrainian("Речовину нагріли до 300 K."),
              "Речовину нагріли до трьохсот кельвінів.");
    expect_eq("governed celsius",
              normalize_ukrainian("Температура зросла до 500 °С."),
              "Температура зросла до п'ятисот градусів Цельсія.");
    expect_eq("unicode kelvin sign", normalize_ukrainian("273 K", range_options), "двісті сімдесят три кельвіни");
    expect_eq("legacy degree kelvin", normalize_ukrainian("273 °K", range_options), "двісті сімдесят три кельвіни");
    expect_eq("kelvin range",
              normalize_ukrainian("250–300 K", range_options),
              "від двохсот п'ятдесяти до трьохсот кельвінів");
    expect_eq(
        "signed kelvin range", normalize_ukrainian("-5–+7 K", range_options), "від мінус п'яти до плюс семи кельвінів");
    expect_eq("decimal kelvin range",
              normalize_ukrainian("1,5–2,5 K", range_options),
              "від однієї цілої і п'яти десятих до двох цілих і п'яти десятих кельвіна");
    expect_eq("repeated kelvin range",
              normalize_ukrainian("від 250 K до 300 K", range_options),
              "від двохсот п'ятдесяти до трьохсот кельвінів");
    expect_eq("rankine range", normalize_ukrainian("5–7 °R", range_options), "від п'яти до семи градусів Ранкіна");
    expect_eq("reaumur range", normalize_ukrainian("5–7 °Ré", range_options), "від п'яти до семи градусів Реомюра");
    expect_eq("delisle range", normalize_ukrainian("5–7 °De", range_options), "від п'яти до семи градусів Деліля");
    expect_eq("romer range", normalize_ukrainian("5–7 °Rø", range_options), "від п'яти до семи градусів Ремера");
    expect_eq("newton named range",
              normalize_ukrainian("5–7 градусів Ньютона", range_options),
              "від п'яти до семи градусів Ньютона");
    expect_eq("mixed temperature scales",
              normalize_ukrainian("5 °C–7 K", range_options),
              "від п'яти градусів Цельсія до семи кельвінів");
    expect_eq("repeated temperature units",
              normalize_ukrainian("5°C–7°C", range_options),
              "від п'яти до семи градусів Цельсія");
    expect_eq("explicit unsigned temperature range",
              normalize_ukrainian("від 5 до 7 °C", range_options),
              "від п'яти до семи градусів Цельсія");
    expect_eq("explicit repeated named temperature range",
              normalize_ukrainian("від -5 градусів Цельсія до +7 градусів Цельсія", range_options),
              "від мінус п'яти до плюс семи градусів Цельсія");
    expect_eq("negative temperature", normalize_ukrainian("-5 °C", range_options), "мінус п'ять градусів Цельсія");
    expect_eq("unicode minus temperature", normalize_ukrainian("−5 °C", range_options), "мінус п'ять градусів Цельсія");
    expect_eq(
        "en dash unary minus temperature", normalize_ukrainian("–5 °C", range_options), "мінус п'ять градусів Цельсія");
    expect_eq("signed decimal temperature",
              normalize_ukrainian("-5,5 °C", range_options),
              "мінус п'ять цілих і п'ять десятих градуса Цельсія");
    expect_eq("explicit signed temperature range",
              normalize_ukrainian("від -5 до +7 °C", range_options),
              "від мінус п'яти до плюс семи градусів Цельсія");
    expect_eq("descending temperature range",
              normalize_ukrainian("7–5 °C", range_options),
              "від семи до п'яти градусів Цельсія");
    expect_eq(
        "equal temperature range", normalize_ukrainian("5–5 °C", range_options), "від п'яти до п'яти градусів Цельсія");
    expect_not_contains("oversized temperature range",
                        normalize_ukrainian("999999999999999999999999–1000000000000000000000000 °C", range_options),
                        "від нуля до нуля");
    expect_eq("signed unit range", normalize_ukrainian("-5–7 кг", range_options), "від мінус п'яти до семи кілограмів");
    expect_eq("decimal unit range",
              normalize_ukrainian("1,5–2,5 кг", range_options),
              "від однієї цілої і п'яти десятих до двох цілих і п'яти десятих кілограмів");
    expect_eq("repeated unit range", normalize_ukrainian("1 кг–2 кг", range_options), "від одного до двох кілограмів");
    expect_eq("explicit repeated unit range",
              normalize_ukrainian("від 1 кг до 2 кг", range_options),
              "від одного до двох кілограмів");
    expect_eq("repeated decimal percent range",
              normalize_ukrainian("10,5%–15,5%", range_options),
              "від десяти цілих і п'яти десятих до п'ятнадцяти цілих і п'яти десятих відсотків");
    expect_eq("explicit repeated percent range",
              normalize_ukrainian("від 10% до 15%", range_options),
              "від десяти до п'ятнадцяти відсотків");
    expect_eq("currency suffix range", normalize_ukrainian("5–7 грн", range_options), "від п'яти до семи гривень");
    expect_eq("currency prefix range", normalize_ukrainian("$5–$7", range_options), "від п'яти до семи доларів");
    expect_eq("explicit repeated currency range",
              normalize_ukrainian("від 5 грн до 7 грн", range_options),
              "від п'яти до семи гривень");
    expect_eq("bare number range", normalize_ukrainian("5–7", range_options), "від п'яти до семи");
    expect_eq("terminal bare number range", normalize_ukrainian("5–7.", range_options), "від п'яти до семи.");
    expect_eq("range after punctuation dash",
              normalize_ukrainian("У Європі — 300—330, 380—400 кВ.", range_options),
              "У Європі — від трьохсот до трьохсот тридцяти, від трьохсот вісімдесяти до чотирьохсот кіловольт.");
    expect_eq("time range",
              normalize_ukrainian("10:30–12:45", range_options),
              "від десятої години тридцяти хвилин до дванадцятої години сорока п'яти хвилин");
    expect_eq("fraction range", normalize_ukrainian("1/2–3/4", range_options), "від однієї другої до трьох четвертих");
    expect_eq("page range", normalize_ukrainian("стор. 5–7", range_options), "від п'ятої до сьомої сторінки");
    expect_eq("short page range", normalize_ukrainian("с. 5–7", range_options), "від п'ятої до сьомої сторінки");
    expect_eq("uppercase page range", normalize_ukrainian("Стор. 5—7", range_options), "від п'ятої до сьомої сторінки");
    expect_eq("English bibliographic page range",
              normalize_ukrainian("P. 1227–1246.", range_options),
              "від тисяча двісті двадцять сьомої до тисяча двісті сорок шостої сторінки.");
    expect_eq("legal article range", normalize_ukrainian("ст. 5–7", range_options), "від п'ятої до сьомої статті");
    expect_eq("legal point range", normalize_ukrainian("п. 2-4", range_options), "від другого до четвертого пункту");
    expect_eq("year month", normalize_ukrainian("2026-09", range_options), "вересень дві тисячі двадцять шостого року");
    expect_not_contains("invalid year month is not a range", normalize_ukrainian("2026-13", range_options), "від");
    expect_eq("short DMY date",
              normalize_ukrainian("14.09.26", range_options),
              "чотирнадцяте вересня дві тисячі двадцять шостого року");
    expect_eq("ISO datetime",
              normalize_ukrainian("2026-09-14T10:30:00Z", range_options),
              "чотирнадцяте вересня дві тисячі двадцять шостого року о десять годин тридцять хвилин за всесвітнім "
              "координованим часом");
    expect_eq("ISO duration",
              normalize_ukrainian("P1Y2M3DT4H5M6S"),
              "один рік два місяці три дні чотири години п'ять хвилин шість секунд");
    expect_eq("ISO week date",
              normalize_ukrainian("2026-W37-1"),
              "перший день тридцять сьомого тижня дві тисячі двадцять шостого року");
    expect_eq("ISO ordinal date",
              normalize_ukrainian("2024-366"),
              "триста шістдесят шостий день дві тисячі двадцять четвертого року");
    expect_eq(
        "IANA timezone", normalize_ukrainian("10:30 Europe/Kyiv"), "десять годин тридцять хвилин за київським часом");
    expect_eq("PM time", normalize_ukrainian("10:30 PM"), "десять годин тридцять хвилин вечора");
    expect_eq("midnight", normalize_ukrainian("00:00"), "опівночі");
    expect_eq("ratio", normalize_ukrainian("16:9"), "шістнадцять до дев'яти");
    expect_eq("scientific e notation", normalize_ukrainian("1e-3"), "один помножити на десять у степені мінус три");
    expect_eq("scientific superscript",
              normalize_ukrainian("6.02×10²³"),
              "шість цілих і дві сотих помножити на десять у степені двадцять три");
    expect_eq("negative fraction", normalize_ukrainian("-1/2"), "мінус одна друга");
    expect_eq("zero denominator preserved", normalize_ukrainian("1/0"), "один/нуль");
    expect_eq("signed prefix currency", normalize_ukrainian("-$5"), "мінус п'ять доларів");
    expect_eq("accounting currency", normalize_ukrainian("(100 грн)"), "мінус сто гривень");
    expect_eq("currency code prefix", normalize_ukrainian("USD 10"), "десять доларів");
    expect_eq("repeated currency amounts", normalize_ukrainian("5 грн і 6 грн"), "п'ять гривень і шість гривень");
    expect_eq("additional fiat", normalize_ukrainian("2 KRW"), "дві вони");
    expect_eq("additional crypto", normalize_ukrainian("0.5 DOGE"), "нуль цілих і п'ять десятих доджкоїна");
    expect_eq(
        "fuel economy", normalize_ukrainian("6.5 L/100km"), "шість цілих і п'ять десятих літра на сто кілометрів");
    expect_eq("imperial unit", normalize_ukrainian("12 oz"), "дванадцять унцій");
    expect_eq("torque unit", normalize_ukrainian("10 Н·м"), "десять ньютон-метрів");
    expect_eq("composed force unit", normalize_ukrainian("20 кг·м/с²"), "двадцять ньютонів");
    expect_eq("viscosity unit", normalize_ukrainian("0,5 Па·с"), "нуль цілих і п'ять десятих паскаль-секунди");
    expect_eq(
        "EV energy unit", normalize_ukrainian("18 кВт·год/100 км"), "вісімнадцять кіловат-годин на сто кілометрів");
    expect_eq("composable unit fallback",
              normalize_ukrainian("7 кг·м/с³"),
              "сім кілограмів помножити на метр поділити на секунду у кубі");
    expect_eq("compact measurement tolerance",
              normalize_ukrainian("5±0,2 кг"),
              "п'ять плюс мінус нуль цілих і дві десятих кілограма");
    expect_eq("percentage tolerance", normalize_ukrainian("5 кг ± 2%"), "п'ять кілограмів плюс мінус два відсотки");
    expect_eq("IPv4 endpoint",
              normalize_ukrainian("192.168.1.1:8080"),
              "ай пі сто дев'яносто два сто шістдесят вісім один один порт вісім тисяч вісімдесят");
    expect_eq("IPv4 CIDR", normalize_ukrainian("10.0.0.0/24"), "ай пі десять нуль нуль нуль префікс двадцять чотири");
    expect_eq("MAC address",
              normalize_ukrainian("AA:BB:CC:DD:EE:FF"),
              "мак адреса ей ей двокрапка бі бі двокрапка сі сі двокрапка ді ді двокрапка і і двокрапка еф еф");
    expect_eq("decimal coordinates",
              normalize_ukrainian("50.4501 N, 30.5234 E"),
              "п'ятдесят цілих і чотири тисячі п'ятсот одна десятитисячна градуса північної широти, тридцять цілих і "
              "п'ять тисяч двісті тридцять чотири десятитисячних градуса східної довготи");
    expect_not_contains("invalid decimal coordinates", normalize_ukrainian("90.1 N"), "північної широти");
    expect_not_contains("invalid DMS coordinates", normalize_ukrainian("50°99′00″N"), "північної широти");
    expect_eq("geo URI",
              normalize_ukrainian("geo:-33.8688,151.2093,58"),
              "географічні координати: тридцять три цілих і вісім тисяч шістсот вісімдесят вісім десятитисячних "
              "градуса південної широти, сто п'ятдесят одна ціла і дві тисячі дев'яносто три десятитисячних градуса "
              "східної довготи, висота п'ятдесят вісім метрів");
    expect_eq("decimal minute coordinate",
              normalize_ukrainian("50°27,5′N"),
              "п'ятдесят градусів двадцять сім цілих і п'ять десятих хвилини північної широти");
    expect_eq("UUID",
              normalize_ukrainian("550e8400-e29b-41d4-a716-446655440000"),
              "ю у ай ді п'ять п'ять нуль і вісім чотири нуль нуль дефіс і два дев'ять бі дефіс чотири один ді чотири "
              "дефіс ей сім один шість дефіс чотири чотири шість шість п'ять п'ять чотири чотири нуль нуль нуль нуль");
    expect_eq("ISBN",
              normalize_ukrainian("ISBN 978-617-123-456-7"),
              "ай ес бі ен дев'ять сім вісім шість один сім один два три чотири п'ять шість сім");
    expect_eq("ISSN", normalize_ukrainian("ISSN 1234-567X"), "ай ес ес ен один два три чотири п'ять шість сім екс");
    expect_eq("VIN",
              normalize_ukrainian("VIN WVWZZZ1JZXW000001"),
              "він номер дабл ю ві дабл ю зед зед зед один джей зед екс дабл ю нуль нуль нуль нуль нуль один");
    expect_eq("SWIFT", normalize_ukrainian("SWIFT DEUTDEFF500"), "свіфт код ді і ю ті ді і еф еф п'ять нуль нуль");
    expect_eq("foreign IBAN",
              normalize_ukrainian("DE89 3704 0044 0532 0130 00"),
              "айбан ді і вісім дев'ять три сім нуль чотири нуль нуль чотири чотири нуль п'ять три два нуль один три "
              "нуль нуль нуль");
    expect_eq("international access phone",
              normalize_ukrainian("0044 20 7946 0958 ext 5", range_options),
              "плюс сорок чотири двадцять сім дев'ять чотири шість нуль дев'ять п'ять вісім додатковий п'ять");
    expect_eq("FTP arbitrary TLD",
              normalize_ukrainian("ftp://example.dev/a#b"),
              "фтп двокрапка слеш слеш ексампле крапка дев слеш а решітка б");
    expect_eq("Ukrainian domain label",
              normalize_ukrainian("Сайт ts.kiev.ua працює."),
              "Сайт ц крапка кіев крапка ю ей працює.");
    expect_eq("standalone Ukrainian ASCII domain",
              normalize_ukrainian("Домен .UA делеговано."),
              "Домен крапка ю ей делеговано.");
    expect_eq("standalone Ukrainian IDN domain",
              normalize_ukrainian("Домен .укр делеговано."),
              "Домен крапка укр делеговано.");
    expect_eq("SSML preserved", normalize_ukrainian("<speak>5 кг</speak>"), "<speak>п'ять кілограмів</speak>");
    expect_eq("inline code preserved", normalize_ukrainian("Код `x=5`, вага 2 кг"), "Код `x=5`, вага два кілограми");
    expect_eq("MediaWiki display math preserved",
              normalize_ukrainian(R"(Формула {\displaystyle E=mc^{2}}, вага 5 кг.)"),
              R"(Формула {\displaystyle E=mc^{2}}, вага п'ять кілограмів.)");
    expect_eq("IPA preserved", normalize_ukrainian("OS МФА: [oʊˈɛs]"), "оу ес МФА: [oʊˈɛs]");
    expect_eq("Latin diacritics transliterated",
              normalize_ukrainian("Plankalkül, Vigenère, computār"),
              "планкалкюл, вігенере, компутар");
    expect_eq("isolated Latin diacritic transliterated", normalize_ukrainian("Квáнтовий"), "Квантовий");
    expect_eq("Markdown destination and entity preserved",
              normalize_ukrainian("[5 кг](https://example.com/a?x=1&amp;y=2)"),
              "[п'ять кілограмів](https://example.com/a?x=1&amp;y=2)");
    uktextnorm::NormalizeOptions compact_range_options;
    expect_eq("compact temperature range",
              normalize_ukrainian("-5–-3 °F", compact_range_options),
              "мінус п'ять мінус три градусів Фаренгейта");
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
    spoken_dates.range_style = uktextnorm::RangeStyle::FromTo;
    expect_eq("spoken en dash day range",
              normalize_ukrainian("15–16 вересня 2026", spoken_dates),
              "від п'ятнадцятого до шістнадцятого вересня дві тисячі двадцять шостого року");
    expect_eq("spoken full date range from-to",
              normalize_ukrainian("15.06.2026–16.06.2026", spoken_dates),
              "від п'ятнадцятого червня дві тисячі двадцять шостого року до шістнадцятого червня дві тисячі "
              "двадцять шостого року");
    expect_eq("year range from-to",
              normalize_ukrainian("2020–2024 рр.", spoken_dates),
              "від дві тисячі двадцятого до дві тисячі двадцять четвертого року.");
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
    {
        uktextnorm::NormalizeOptions ambiguity;
        ambiguity.colon_style = uktextnorm::ColonStyle::Ratio;
        expect_eq("forced colon ratio", normalize_ukrainian("10:30", ambiguity), "десять до тридцяти");
        ambiguity.colon_style = uktextnorm::ColonStyle::Clock;
        expect_eq("forced colon clock", normalize_ukrainian("10:30", ambiguity), "десять годин тридцять хвилин");
        ambiguity.numeric_date_order = uktextnorm::NumericDateOrder::MonthDayYear;
        expect_eq("month day year policy",
                  normalize_ukrainian("03/04/2026", ambiguity),
                  "четверте березня дві тисячі двадцять шостого року");
        ambiguity.numeric_date_order = uktextnorm::NumericDateOrder::PreserveAmbiguous;
        expect_eq("preserve ambiguous numeric date", normalize_ukrainian("03/04/2026", ambiguity), "03/04/2026");
        ambiguity.currency_symbol_policy = uktextnorm::CurrencySymbolPolicy::PreserveAmbiguous;
        expect_eq("preserve ambiguous currency symbols",
                  normalize_ukrainian("$12 і ¥500", ambiguity),
                  "$дванадцять і ¥п'ятсот");
    }
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
    expect_eq("version dotted quad",
              normalize_ukrainian("Версія 1.2.3.4"),
              "Версія один крапка два крапка три крапка чотири");
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
                              uktextnorm::UncertaintySeverity::Error);
    expect_no_uncertain_metadata("valid card checksum",
                                 uktextnorm::flag_uncertain("картка 4111 1111 1111 1111"),
                                 "картка 4111 1111 1111 1111",
                                 uktextnorm::UncertaintySeverity::Error);
    expect_no_uncertain_metadata("valid ISBN-13 checksum",
                                 uktextnorm::flag_uncertain("ISBN 978-0-306-40615-7"),
                                 "ISBN 978-0-306-40615-7",
                                 uktextnorm::UncertaintySeverity::Error);
    expect_uncertain_metadata("invalid ISBN-13 checksum",
                              uktextnorm::flag_uncertain("ISBN 978-0-306-40615-8"),
                              "ISBN 978-0-306-40615-8",
                              uktextnorm::UncertaintyCategory::Identifier,
                              uktextnorm::UncertaintySeverity::Error);
    expect_no_uncertain_metadata("valid ISBN-10 checksum",
                                 uktextnorm::flag_uncertain("ISBN-10 0-306-40615-2"),
                                 "ISBN-10 0-306-40615-2",
                                 uktextnorm::UncertaintySeverity::Error);
    expect_no_uncertain_metadata("valid ISSN checksum",
                                 uktextnorm::flag_uncertain("ISSN 0317-8471"),
                                 "ISSN 0317-8471",
                                 uktextnorm::UncertaintySeverity::Error);
    expect_uncertain_metadata("invalid ISSN checksum",
                              uktextnorm::flag_uncertain("ISSN 0317-8472"),
                              "ISSN 0317-8472",
                              uktextnorm::UncertaintyCategory::Identifier,
                              uktextnorm::UncertaintySeverity::Error);
    expect_no_uncertain_metadata("valid IBAN checksum",
                                 uktextnorm::flag_uncertain("DE89 3704 0044 0532 0130 00"),
                                 "DE89 3704 0044 0532 0130 00",
                                 uktextnorm::UncertaintySeverity::Error);
    expect_uncertain_metadata("invalid IBAN checksum",
                              uktextnorm::flag_uncertain("DE88 3704 0044 0532 0130 00"),
                              "DE88 3704 0044 0532 0130 00",
                              uktextnorm::UncertaintyCategory::Identifier,
                              uktextnorm::UncertaintySeverity::Error);
    expect_no_uncertain_metadata("valid VIN checksum",
                                 uktextnorm::flag_uncertain("VIN 1M8GDM9AXKP042788"),
                                 "VIN 1M8GDM9AXKP042788",
                                 uktextnorm::UncertaintySeverity::Error);
    expect_uncertain_metadata("invalid VIN checksum",
                              uktextnorm::flag_uncertain("VIN 1M8GDM9A1KP042788"),
                              "VIN 1M8GDM9A1KP042788",
                              uktextnorm::UncertaintyCategory::Identifier,
                              uktextnorm::UncertaintySeverity::Error);
    expect_no_uncertain_metadata("valid UUID version and variant",
                                 uktextnorm::flag_uncertain("550e8400-e29b-41d4-a716-446655440000"),
                                 "550e8400-e29b-41d4-a716-446655440000",
                                 uktextnorm::UncertaintySeverity::Error);
    expect_uncertain_metadata("invalid UUID variant",
                              uktextnorm::flag_uncertain("550e8400-e29b-41d4-0716-446655440000"),
                              "550e8400-e29b-41d4-0716-446655440000",
                              uktextnorm::UncertaintyCategory::Identifier,
                              uktextnorm::UncertaintySeverity::Error);
    expect_no_uncertain_metadata("valid hash length",
                                 uktextnorm::flag_uncertain("MD5 d41d8cd98f00b204e9800998ecf8427e"),
                                 "MD5 d41d8cd98f00b204e9800998ecf8427e",
                                 uktextnorm::UncertaintySeverity::Error);
    expect_uncertain_metadata("invalid hash length",
                              uktextnorm::flag_uncertain("MD5 d41d8cd98f00b204"),
                              "MD5 d41d8cd98f00b204",
                              uktextnorm::UncertaintyCategory::Identifier,
                              uktextnorm::UncertaintySeverity::Error);
    expect_no_uncertain_category("supported ISO currency metadata",
                                 uktextnorm::flag_uncertain("Сума 12 AED."),
                                 uktextnorm::UncertaintyCategory::Currency);
    expect_no_uncertain_category("generic finance ticker is not an unknown unit",
                                 uktextnorm::flag_uncertain("Сума 5 XYZ."),
                                 uktextnorm::UncertaintyCategory::Unit);
    expect_uncertain_metadata("ambiguous numeric date metadata",
                              uktextnorm::flag_uncertain("Дата 03/04/2026"),
                              "03/04/2026",
                              uktextnorm::UncertaintyCategory::Date,
                              uktextnorm::UncertaintySeverity::Warning);
    expect_uncertain_metadata("ambiguous colon metadata",
                              uktextnorm::flag_uncertain("Значення 10:30"),
                              "10:30",
                              uktextnorm::UncertaintyCategory::Time,
                              uktextnorm::UncertaintySeverity::Warning);
    expect_uncertain_metadata("ambiguous currency symbol metadata",
                              uktextnorm::flag_uncertain("Сума $12"),
                              "$12",
                              uktextnorm::UncertaintyCategory::Currency,
                              uktextnorm::UncertaintySeverity::Warning);
    expect_uncertain_metadata("uncertain unit metadata",
                              uktextnorm::flag_uncertain("Вага 5 qq."),
                              "5 qq",
                              uktextnorm::UncertaintyCategory::Unit,
                              uktextnorm::UncertaintySeverity::Warning);
    expect_no_uncertain_category("UTF-8 tonne abbreviation is a known unit",
                                 uktextnorm::flag_uncertain("Енциклопедія у 3 т."),
                                 uktextnorm::UncertaintyCategory::Unit);
    expect_no_uncertain_category("a year followed by a preposition is not a unit",
                                 uktextnorm::flag_uncertain("2016 у Wayback Machine"),
                                 uktextnorm::UncertaintyCategory::Unit);
    expect_no_uncertain_category("a compound data rate is a known unit",
                                 uktextnorm::flag_uncertain("100 Мбіт/с"),
                                 uktextnorm::UncertaintyCategory::Unit);
    expect_no_uncertain_category("a mixed-script data rate is a known unit",
                                 uktextnorm::flag_uncertain("10 Гбіт/c"),
                                 uktextnorm::UncertaintyCategory::Unit);
    expect_no_uncertain_category("a month after a date is ordinary prose",
                                 uktextnorm::flag_uncertain("1 січня"),
                                 uktextnorm::UncertaintyCategory::Unit);
    expect_no_uncertain_category("a chemical formula is not a number followed by a unit",
                                 uktextnorm::flag_uncertain("H2O"),
                                 uktextnorm::UncertaintyCategory::Unit);
    expect_no_uncertain_category("3G is a network generation, not an unknown unit",
                                 uktextnorm::flag_uncertain("мережа 3G"),
                                 uktextnorm::UncertaintyCategory::Unit);
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

    {
        uktextnorm::NormalizeOptions conservative =
            uktextnorm::options_for_preset(uktextnorm::NormalizePreset::Conservative);
        expect_eq("homoglyphs off in conservative", normalize_ukrainian("Пoлтaвa", conservative), "Пoлтaвa");
        uktextnorm::NormalizeOptions no_validation;
        no_validation.validate_dates = false;
        expect_eq("invalid date rejected", normalize_ukrainian("тридцять 30.02.2024"), "тридцять 30.02.2024");
        expect_eq("invalid date accepted when validation off",
                  normalize_ukrainian("30.02.2024", no_validation),
                  "тридцяте лютого дві тисячі двадцять четвертого року");
        uktextnorm::NormalizeOptions strip_quotes;
        strip_quotes.quote_style = uktextnorm::QuoteStyle::Strip;
        expect_eq("quote strip", normalize_ukrainian("Слово «тест» тут", strip_quotes), "Слово тест тут");
        uktextnorm::NormalizeOptions straight_quotes;
        straight_quotes.quote_style = uktextnorm::QuoteStyle::Straight;
        expect_eq("quote straight", normalize_ukrainian("Слово «тест» тут", straight_quotes), "Слово \"тест\" тут");
        uktextnorm::NormalizeOptions no_network = conservative;
        no_network.normalize_network_addresses = false;
        expect_eq("ip network opt-out",
                  normalize_ukrainian("IP 192.168.100.200", no_network),
                  "IP сто дев'яносто два крапка сто шістдесят вісім крапка сто крапка двісті");
    }
    expect_uncertain_metadata("uncertain invalid date metadata",
                              uktextnorm::flag_uncertain("Дата 30.02.2024"),
                              "30.02.2024",
                              uktextnorm::UncertaintyCategory::InvalidDate,
                              uktextnorm::UncertaintySeverity::Error);
    expect_uncertain_metadata("uncertain number grouping metadata",
                              uktextnorm::flag_uncertain("Сума 1,234"),
                              "1,234",
                              uktextnorm::UncertaintyCategory::AmbiguousNumberGrouping,
                              uktextnorm::UncertaintySeverity::Warning);
    expect_uncertain_metadata("uncertain unknown oblique agreement metadata",
                              uktextnorm::flag_uncertain("у 4 фларбах"),
                              "4 фларбах",
                              uktextnorm::UncertaintyCategory::Agreement,
                              uktextnorm::UncertaintySeverity::Info);
    expect_uncertain_metadata("invalid time metadata",
                              uktextnorm::flag_uncertain("Час 99:30"),
                              "99:30",
                              uktextnorm::UncertaintyCategory::Time,
                              uktextnorm::UncertaintySeverity::Error);
    expect_uncertain_metadata("invalid AM PM metadata",
                              uktextnorm::flag_uncertain("Час 13:30 PM"),
                              "13:30 PM",
                              uktextnorm::UncertaintyCategory::Time,
                              uktextnorm::UncertaintySeverity::Error);
    expect_uncertain_metadata("invalid ISO date metadata",
                              uktextnorm::flag_uncertain("Дата 2026-13-01"),
                              "2026-13-01",
                              uktextnorm::UncertaintyCategory::InvalidDate,
                              uktextnorm::UncertaintySeverity::Error);
    expect_uncertain_metadata("invalid ISO week metadata",
                              uktextnorm::flag_uncertain("Дата 2026-W54-8"),
                              "2026-W54-8",
                              uktextnorm::UncertaintyCategory::InvalidDate,
                              uktextnorm::UncertaintySeverity::Error);
    expect_uncertain_metadata("invalid ISO ordinal metadata",
                              uktextnorm::flag_uncertain("Дата 2025-366"),
                              "2025-366",
                              uktextnorm::UncertaintyCategory::InvalidDate,
                              uktextnorm::UncertaintySeverity::Error);
    expect_uncertain_metadata("invalid timezone offset metadata",
                              uktextnorm::flag_uncertain("Час UTC+24:00"),
                              "UTC+24:00",
                              uktextnorm::UncertaintyCategory::Time,
                              uktextnorm::UncertaintySeverity::Error);
    expect_uncertain_metadata("invalid timezone minute metadata",
                              uktextnorm::flag_uncertain("Час UTC+02:99"),
                              "UTC+02:99",
                              uktextnorm::UncertaintyCategory::Time,
                              uktextnorm::UncertaintySeverity::Error);
    expect_uncertain_metadata("unknown contextual IANA timezone metadata",
                              uktextnorm::flag_uncertain("Час 10:30 Europe/Paris"),
                              "Europe/Paris",
                              uktextnorm::UncertaintyCategory::Time,
                              uktextnorm::UncertaintySeverity::Warning);
    expect_no_uncertain_category("IANA timezone is not an unknown unit",
                                 uktextnorm::flag_uncertain("Час 10:30 Europe/Paris"),
                                 uktextnorm::UncertaintyCategory::Unit);
    expect_no_uncertain_category("URL is not an IANA timezone",
                                 uktextnorm::flag_uncertain("https://example.com/a"),
                                 uktextnorm::UncertaintyCategory::Time);
    expect_uncertain_metadata("invalid geo URI metadata",
                              uktextnorm::flag_uncertain("geo:91.2,181.0"),
                              "geo:91.2,181.0",
                              uktextnorm::UncertaintyCategory::Coordinate,
                              uktextnorm::UncertaintySeverity::Error);
    expect_uncertain_metadata("invalid DMS coordinate metadata",
                              uktextnorm::flag_uncertain("50°99′00″N"),
                              "50°99′00″N",
                              uktextnorm::UncertaintyCategory::Coordinate,
                              uktextnorm::UncertaintySeverity::Error);
    expect_uncertain_metadata("zero denominator metadata",
                              uktextnorm::flag_uncertain("Частка 1/0"),
                              "1/0",
                              uktextnorm::UncertaintyCategory::Fraction,
                              uktextnorm::UncertaintySeverity::Error);
    expect_uncertain_metadata("invalid network metadata",
                              uktextnorm::flag_uncertain("IP 999.1.1.1/40"),
                              "999.1.1.1/40",
                              uktextnorm::UncertaintyCategory::Network,
                              uktextnorm::UncertaintySeverity::Error);
    expect_uncertain_metadata("malformed scientific metadata",
                              uktextnorm::flag_uncertain("Значення 1e+"),
                              "1e+",
                              uktextnorm::UncertaintyCategory::Scientific,
                              uktextnorm::UncertaintySeverity::Warning);

    const auto audit_options = uktextnorm::options_for_preset(uktextnorm::NormalizePreset::TtsFriendly);
    expect_eq("technical power with Unicode minus",
              normalize_ukrainian("10−9 м", audit_options),
              "десять у степені мінус дев'ять метрів");
    expect_eq("technical power with a multiplier",
              normalize_ukrainian("2x10−6 м", audit_options),
              "два помножити на десять у степені мінус шість метри");
    expect_eq("range of technical powers",
              normalize_ukrainian("10−15—10−12 секунди", audit_options),
              "від десяти у степені мінус п'ятнадцять до десяти у степені мінус дванадцять секунд");
    const auto greek_power = normalize_ukrainian("−0,0419·10−3ρh", audit_options);
    expect_eq("power next to a Greek variable",
              greek_power,
              "мінус нуль цілих і чотириста дев'ятнадцять десятитисячних помножити на десять у степені мінус три ро "
              "аш");
    expect_eq("Greek-variable power is idempotent", normalize_ukrainian(greek_power, audit_options), greek_power);
    expect_eq("inverse Celsius scientific unit",
              normalize_ukrainian("0,6 × 10−6°C−1", audit_options),
              "нуль цілих і шість десятих помножити на десять у степені мінус шість на градус Цельсія");
    expect_eq("Greek coefficient with an inverse unit",
              normalize_ukrainian("з α = 0,6 × 10−6°C−1", audit_options),
              "з альфою, що дорівнює нуль цілих і шість десятих помножити на десять у степені мінус шість на градус "
              "Цельсія");
    expect_eq("ordinary hyphen still denotes a range",
              normalize_ukrainian("10-12 м", audit_options),
              "від десяти до дванадцяти метрів");
    expect_eq("Cyrillic Roman century", normalize_ukrainian("У ХХ ст.", audit_options), "У двадцятому столітті");
    expect_eq("Cyrillic Roman century after a genitive cue",
              normalize_ukrainian("до початку ХХІ століття", audit_options),
              "до початку двадцять першого століття");
    expect_eq("Cyrillic Roman century range",
              normalize_ukrainian("В Х—ХІ ст.", audit_options),
              "В десятому–одинадцятому століттях");
    expect_eq("inflected Cyrillic Roman century",
              normalize_ukrainian("У ХХІ столітті", audit_options),
              "У двадцять першому столітті");
    expect_eq("capitalized month in a full date",
              normalize_ukrainian("12 Січня 2013", audit_options),
              "дванадцятого січня дві тисячі тринадцятого року");
    expect_eq("unambiguous US slash date",
              normalize_ukrainian("04/29/02", audit_options),
              "двадцять дев'ятого квітня дві тисячі другого року");
    expect_eq("ambiguous slash date keeps local order",
              normalize_ukrainian("04/05/02", audit_options),
              "четвертого травня дві тисячі другого року");
    expect_eq("genitive ordinal class", normalize_ukrainian("мережа 1 класу", audit_options), "мережа першого класу");
    expect_eq("regional dollar with multiplier",
              normalize_ukrainian("US$2,9 трлн", audit_options),
              "дві цілих і дев'ять десятих трильйона доларів");
    expect_eq("range after to", normalize_ukrainian("до 7-8 доларів", audit_options), "до семи–восьми доларів");
    expect_eq(
        "range after from", normalize_ukrainian("від 100…200 °С", audit_options), "від ста–двохсот градусів Цельсія");
    expect_eq("range after on", normalize_ukrainian("на 60-80%", audit_options), "на шістдесят–вісімдесят відсотків");
    expect_eq("range after near",
              normalize_ukrainian("близько 10%-15%", audit_options),
              "близько десяти–п'ятнадцяти відсотків");
    expect_eq("range after in", normalize_ukrainian("в 2—3 лінії", audit_options), "в дві–три лінії");
    expect_eq("range of school grades in locative",
              normalize_ukrainian("в 9-10 класах", audit_options),
              "в дев'ятих–десятих класах");
    expect_eq("school grades after pupils are ordinal",
              normalize_ukrainian("Довідник для учнів 9-11 класів", audit_options),
              "Довідник для учнів дев'ятих–одинадцятих класів");
    expect_eq("a count of classes remains cardinal",
              normalize_ukrainian("Школа має 9-11 класів", audit_options),
              "Школа має від дев'яти до одинадцяти класів");
    expect_eq("abbreviated year range",
              normalize_ukrainian("У 1946–47 роках", audit_options),
              "У період від тисяча дев'ятсот сорок шостого до тисяча дев'ятсот сорок сьомого року");
    expect_eq("abbreviated decade range",
              normalize_ukrainian("1970-80-х роках", audit_options),
              "сімдесятих–вісімдесятих роках двадцятого століття");
    expect_eq("year range after on",
              normalize_ukrainian("на 2007—2010 роки", audit_options),
              "на період від дві тисячі сьомого до дві тисячі десятого року");
    expect_eq("date range after on",
              normalize_ukrainian("планували на 21-24 вересня 2020 р.", audit_options),
              "планували на період від двадцять першого до двадцять четвертого вересня дві тисячі двадцятого року");
    expect_eq("date range without a year",
              normalize_ukrainian("планували на 21-24 вересня", audit_options),
              "планували на період від двадцять першого до двадцять четвертого вересня");
    expect_eq("Bible chapter and verse do not become clock times",
              normalize_ukrainian("(Ісая 40:22, 40:28, 41:9)", audit_options),
              "(Ісая розділ сорок, вірш двадцять два, розділ сорок, вірш двадцять вісім, розділ сорок один, вірш "
              "дев'ять)");
    expect_eq("unicode minus fraction", normalize_ukrainian("−1/2", audit_options), "мінус одна друга");
    expect_eq("signed compound measurement",
              normalize_ukrainian("-2,5 м/с²", audit_options),
              "мінус дві цілих і п'ять десятих метра за секунду в квадраті");
    expect_eq("signed percent", normalize_ukrainian("-5%", audit_options), "мінус п'ять відсотків");
    expect_eq("latin SI product", normalize_ukrainian("3 N*m", audit_options), "три ньютони помножити на метр");
    expect_eq("latin radiative flux unit", normalize_ukrainian("3 W/m²", audit_options), "три вати на квадратний метр");
    expect_eq("ISO week duration", normalize_ukrainian("P2W", audit_options), "два тижні");
    expect_eq(
        "fractional ISO duration", normalize_ukrainian("PT1.5H", audit_options), "одна ціла і п'ять десятих години");
    expect_eq("fractional ISO day", normalize_ukrainian("P0.5D", audit_options), "нуль цілих і п'ять десятих дня");
    expect_eq("ISO duration feminine agreement",
              normalize_ukrainian("PT1H30.5M", audit_options),
              "одна година тридцять цілих і п'ять десятих хвилини");
    expect_eq("malformed leading-dot ISO duration preserved", normalize_ukrainian("PT.5H", audit_options), "PT.5H");
    expect_eq("invalid ISO duration preserved", normalize_ukrainian("P1DT", audit_options), "P1DT");
    expect_eq("malformed scientific preserved", normalize_ukrainian("1e+", audit_options), "1e+");
    expect_eq("invalid ISBN preserved",
              normalize_ukrainian("ISBN 978-617-57-40-11-4", audit_options),
              "ISBN 978-617-57-40-11-4");
    expect_eq("HTML code contents preserved",
              normalize_ukrainian("Формула <code>x = 5</code>, маса 2 кг.", audit_options),
              "Формула <code>x = 5</code>, маса два кілограми.");
    expect_eq("mathematical comparisons are not HTML",
              normalize_ukrainian("l/h = 2…10 між (l/h < 2) і (l/h > 10).", audit_options),
              "л/г дорівнює від двох до десяти між (л/г менше два) і (л/г більше десять).");
    expect_eq("stripped adjacent quotes keep word boundaries",
              normalize_ukrainian("дисертацію«Методична система»на тему", audit_options),
              "дисертацію Методична система на тему");
    expect_eq("address abbreviation does not match inside word",
              normalize_ukrainian("пресс. сторінка двісті.", audit_options),
              "пресс. сторінка двісті.");
    expect_eq("measurement abbreviation is not a city",
              normalize_ukrainian(normalize_ukrainian("1 т. о. м. = 1 кг", audit_options), audit_options),
              "одна тонна. о. м. дорівнює один кілограм");
    expect_eq("acute apostrophe is canonicalized in an identifier",
              normalize_ukrainian("ICREPQ´04", audit_options),
              "ай сі ар і пі к'ю'нуль чотири");
    expect_eq("high precision decimal is not a phone number",
              normalize_ukrainian("0,000000001 км", audit_options),
              "нуль кома нуль нуль нуль нуль нуль нуль нуль нуль один кілометра");
    expect_eq("measurement after duration governor",
              normalize_ukrainian("протягом 4 хвилин", audit_options),
              "протягом чотирьох хвилин");
    expect_eq("measurement after vprodovzh governor",
              normalize_ukrainian("Впродовж 15 хвилин очікуємо відбій тривоги.", audit_options),
              "Впродовж п'ятнадцяти хвилин очікуємо відбій тривоги.");
    expect_eq("measurement after uprodovzh governor",
              normalize_ukrainian("упродовж 3 днів", audit_options),
              "упродовж трьох днів");
    expect_eq("clock time after o takes locative",
              normalize_ukrainian("Зустріч о 10:30", audit_options),
              "Зустріч о десятій годині тридцять хвилин");
    expect_eq("bare dot decimal",
              normalize_ukrainian("Коефіцієнт 0.9996.", audit_options),
              "Коефіцієнт нуль цілих і дев'ять тисяч дев'ятсот дев'яносто шість десятитисячних.");
    expect_eq("named compact version",
              normalize_ukrainian("версії 2.6 і v0.9", audit_options),
              "версії два крапка шість і ві нуль крапка дев'ять");
    expect_eq("single-letter standard",
              normalize_ukrainian("Стандарт E.214.", audit_options),
              "Стандарт і крапка двісті чотирнадцять.");
    expect_eq("lettered construction standard",
              normalize_ukrainian("ДБН В.2.5-23:2010", audit_options),
              "де бе ен ве крапка два крапка п'ять дефіс двадцять три двокрапка дві тисячі десять");
    expect_eq("numeric construction standard",
              normalize_ukrainian("ГОСТ 16483.17–81", audit_options),
              "ГОСТ шістнадцять тисяч чотириста вісімдесят три крапка сімнадцять дефіс вісімдесят один");
    const auto corpus_standards = normalize_ukrainian(
        "IEEE 802 .22; ISO 8512-1:1990; ДСТУ ISO 80000-1:2016; ISO / IEC 7812; ISO-8859-1; ДНАОП 0.00-1.32-01.",
        audit_options);
    expect_eq(
        "Wikipedia technical standards",
        corpus_standards,
        "ай і і і вісімсот два крапка двадцять два; ай ес оу вісім тисяч п'ятсот дванадцять дефіс один двокрапка "
        "тисяча дев'ятсот дев'яносто; ДСТУ ай ес оу вісімдесят тисяч дефіс один двокрапка дві тисячі шістнадцять; ай "
        "ес оу слеш ай і сі сім тисяч вісімсот дванадцять; ай ес оу дефіс вісім тисяч вісімсот п'ятдесят дев'ять дефіс "
        "один; ДНАОП нуль крапка нуль нуль дефіс один крапка тридцять два дефіс нуль один.");
    expect_eq(
        "technical standards are idempotent", normalize_ukrainian(corpus_standards, audit_options), corpus_standards);
    expect_eq("standard delimiter does not consume prose",
              normalize_ukrainian("IEC 61970/61968 — загальна модель.", audit_options),
              "ай і сі шістдесят одна тисяча дев'ятсот сімдесят слеш шістдесят одна тисяча дев'ятсот шістдесят вісім — "
              "загальна модель.");
    expect_eq("Cyrillic technical identifiers",
              normalize_ukrainian("К145ІК512П; АТ1; О2; СО2; ТіО2; 38С2; Р-405м; БІО-100.", audit_options),
              "ка сто сорок п'ять і ка п'ятсот дванадцять пе; а те один; о два; ес о два; те і о два; тридцять вісім "
              "ес два; ер дефіс чотириста п'ять ем; бе і о дефіс сто.");
    expect_eq("compound Cyrillic technical codes",
              normalize_ukrainian("Плита 1-0-1000х630; ВМ-23/25/27/32/1230.", audit_options),
              "Плита один дефіс нуль дефіс тисяча помножити на шістсот тридцять; ве ем дефіс двадцять три слеш "
              "двадцять п'ять слеш двадцять сім слеш тридцять два слеш тисяча двісті тридцять.");
    expect_eq("spaced Cyrillic dimensions",
              normalize_ukrainian("розмірами 1000 х 630 мм", audit_options),
              "розмірами тисяча помножити на шістсот тридцять міліметрів");
    expect_eq("scientific notation without caret and with unit",
              normalize_ukrainian("1,76× 10-19 Дж", audit_options),
              "одна ціла і сімдесят шість сотих помножити на десять у степені мінус дев'ятнадцять джоуля");
    expect_eq("named month consumes abbreviated year suffix",
              normalize_ukrainian("У липні 2011 р.", audit_options),
              "У липні дві тисячі одинадцятого року");
    expect_eq("zero ordinal", normalize_ukrainian("0-го класу", audit_options), "нульового класу");
    expect_eq("spaced abbreviation punctuation",
              normalize_ukrainian("і т.д .; Corp. створено", audit_options),
              "і так далі; корп. створено");
    const auto foreign_slash = normalize_ukrainian("Index locorum / Seznam krajev", audit_options);
    expect_eq("foreign slash spacing", foreign_slash, "індекс локорум/сезнам краджев");
    expect_eq("foreign slash spacing is idempotent", normalize_ukrainian(foreign_slash, audit_options), foreign_slash);
    expect_eq("abbreviation boundaries inside identifiers",
              normalize_ukrainian("ІЕР-01 і КР-005", audit_options),
              "і е ер дефіс нуль один і ка ер дефіс нуль нуль п'ять");
    expect_eq("spaced rate units",
              normalize_ukrainian("Швидкість 18 Мбіт / с. Затримка 160 мс; сигнал −116 дБм.", audit_options),
              "Швидкість вісімнадцять мегабітів за секунду. Затримка сто шістдесят мілісекунд; сигнал мінус сто "
              "шістнадцять децибел-міліват.");
    expect_eq("technical acronyms are not Roman numerals",
              normalize_ukrainian("Підфрейм DL, інтерфейс DVI та елемент III групи.", audit_options),
              "Підфрейм ді ел, інтерфейс ді ві ай та елемент третьої групи.");
    expect_eq("dotted standard with letter suffix",
              normalize_ukrainian("Wi-Fi 6 (802.11ax)", audit_options),
              "ві-фі шість (вісімсот два крапка одинадцять ей екс)");
    expect_eq("bare IEEE revisions are identifiers, not numeric ranges",
              normalize_ukrainian("802.16-2005 (802.16e, 802.16m).", audit_options),
              "вісімсот два крапка шістнадцять дефіс дві тисячі п'ять (вісімсот два крапка шістнадцять і, "
              "вісімсот два крапка шістнадцять ем).");
    expect_eq("bare IEEE revision before sentence period",
              normalize_ukrainian("Стандарт 802.16m.", audit_options),
              "Стандарт вісімсот два крапка шістнадцять ем.");
    expect_eq("bare IEEE revision with en dash",
              normalize_ukrainian("802.16–2005", audit_options),
              "вісімсот два крапка шістнадцять дефіс дві тисячі п'ять");
    expect_no_uncertain_category("IEEE revision suffix is not an unknown unit",
                                 uktextnorm::flag_uncertain("802.16e"),
                                 uktextnorm::UncertaintyCategory::Unit);
    expect_no_uncertain_category("IEEE revision suffix is not malformed scientific notation",
                                 uktextnorm::flag_uncertain("802.16e"),
                                 uktextnorm::UncertaintyCategory::Scientific);
    expect_eq("classification code is not an invalid date",
              normalize_ukrainian("за спеціальністю 13.00.02", audit_options),
              "за спеціальністю тринадцять крапка нуль нуль крапка нуль два");
    expect_eq("dissertation speciality code after sciences label",
              normalize_ukrainian("Дисертація доктора технічних наук: 05.24.01 / університет.", audit_options),
              "Дисертація доктора технічних наук: нуль п'ять крапка двадцять чотири крапка нуль один / "
              "університет.");
    expect_eq("Wikipedia page citation metadata is not spoken",
              normalize_ukrainian("Результат узгоджено з експериментом.:33–34:39–43 Так само виміряли густину.",
                                  audit_options),
              "Результат узгоджено з експериментом. Так само виміряли густину.");
    expect_eq("speed-of-light variable is not a village abbreviation",
              normalize_ukrainian("значення швидкості світла у вакуумі с. Перетворення статсіменса", audit_options),
              "значення швидкості світла у вакуумі с. Перетворення статсіменса");
    expect_eq("numeric date consumes explicit year word",
              normalize_ukrainian("Подію завершили 25.06.1986 року.", audit_options),
              "Подію завершили двадцять п'ятого червня тисяча дев'ятсот вісімдесят шостого року.");
    expect_eq("explicit year span",
              normalize_ukrainian("З 1986 по 1991 рр. тривала програма.", audit_options),
              "З тисяча дев'ятсот вісімдесят шостого до тисяча дев'ятсот дев'яносто першого року тривала програма.");
    expect_eq("coordinate direction is not repeated",
              normalize_ukrainian("Точка лежить на 174°E довготи.", audit_options),
              "Точка лежить на сто сімдесят чотири градуси східної довготи.");
    expect_eq("governed coordinate bounds",
              normalize_ukrainian("від 180° довготи до 174° W довготи", audit_options),
              "від ста вісімдесяти градусів довготи до ста сімдесяти чотирьох градусів західної довготи");
    const auto normalized_doi = normalize_ukrainian("doi:10.22059/jitm.2024.99052", audit_options);
    expect_eq("DOI normalization",
              normalized_doi,
              "ді оу ай десять крапка двадцять дві тисячі п'ятдесят дев'ять слеш джітм крапка дві тисячі "
              "двадцять чотири крапка дев'яносто дев'ять тисяч п'ятдесят два");
    expect_eq("DOI normalization is idempotent", normalize_ukrainian(normalized_doi, audit_options), normalized_doi);
    expect_eq("bracketed IPv6 endpoint",
              normalize_ukrainian("[2001:db8::1]:443", audit_options),
              "ай пі версії шість два нуль нуль один двокрапка ді бі вісім двокрапка скорочення нулів двокрапка один "
              "порт чотириста сорок три");
    expect_eq("invalid IPv6 CIDR preserved", normalize_ukrainian("2001:db8::1/129", audit_options), "2001:db8::1/129");
    expect_eq("balanced Markdown destination",
              normalize_ukrainian("[5 кг](https://example.com/a_(b)?x=1)", audit_options),
              "[п'ять кілограмів](https://example.com/a_(b)?x=1)");
    expect_eq("double backtick code", normalize_ukrainian("``x=`5` ``", audit_options), "``x=`5` ``");
    expect_eq("unicode hyphen temperature range",
              normalize_ukrainian("5‐7 °C", audit_options),
              "від п'яти до семи градусів Цельсія");
    expect_eq("temperature range punctuation",
              normalize_ukrainian("5-7 °C.", audit_options),
              "від п'яти до семи градусів Цельсія.");
    expect_eq("measurement terminal punctuation",
              normalize_ukrainian("Відстань становить 100 км.", audit_options),
              "Відстань становить сто кілометрів.");
    expect_eq("governed abbreviated measurement with punctuation",
              normalize_ukrainian("Відстань становить до 2000 м.", audit_options),
              "Відстань становить до двох тисяч метрів.");
    expect_eq("bibliographic page count",
              normalize_ukrainian("Монографія. — 279 с.: іл.", audit_options),
              "Монографія. — двісті сімдесят дев'ять сторінок: іл.");
    expect_eq("bibliographic volume count",
              normalize_ukrainian("Енциклопедія: у 2 т. / ред. Іваненко.", audit_options),
              "Енциклопедія: у двох томах / ред. Іваненко.");
    expect_eq("bibliographic singular volume",
              normalize_ukrainian("Довідник: в 1 т / ред. Іваненко.", audit_options),
              "Довідник: в одному томі / ред. Іваненко.");
    expect_eq("single bibliographic page",
              normalize_ukrainian("Монографія. — С. 896.", audit_options),
              "Монографія. — сторінка вісімсот дев'яносто шість.");
    expect_eq("mediawiki question heading",
              normalize_ukrainian("==== Чи може машина мислити? ====\nТекст відповіді.", audit_options),
              "Чи може машина мислити?\nТекст відповіді.");
    expect_eq("compound measurement terminal punctuation",
              normalize_ukrainian("Швидкість становить 100 Мбіт/с.", audit_options),
              "Швидкість становить сто мегабітів за секунду.");
    expect_eq("mixed-script data-rate denominator",
              normalize_ukrainian("Швидкість до 10 Гбіт/c.", audit_options),
              "Швидкість до десяти гігабітів за секунду.");
    expect_eq("progressive video resolution after a quality label",
              normalize_ukrainian("Передача з 1080p-якістю.", audit_options),
              "Передача з якістю тисяча вісімдесят пі.");
    expect_eq("bare progressive video resolution",
              normalize_ukrainian("Відео 720p.", audit_options),
              "Відео сімсот двадцять пі.");
    expect_eq("capitalized kilobit unit",
              normalize_ukrainian("Швидкість становить 144 Кбіт/с.", audit_options),
              "Швидкість становить сто сорок чотири кілобіти за секунду.");
    expect_eq("English tonne unit",
              normalize_ukrainian("Маса становить 30 tonnes.", audit_options),
              "Маса становить тридцять тонн.");
    expect_eq("variable ratio",
              normalize_ukrainian("Розгалужувач має відношення 1:n.", audit_options),
              "Розгалужувач має відношення один до ен.");
    expect_eq("locative number before adjective",
              normalize_ukrainian("Дані зберігають у 51 публічному домені.", audit_options),
              "Дані зберігають у п'ятдесяти одному публічному домені.");
    expect_eq("mediawiki heading delimiters",
              normalize_ukrainian("== Історія ==\nПерший комп'ютер створили давно.", audit_options),
              "Історія\nПерший комп'ютер створили давно.");
    const auto technical_identifiers = normalize_ukrainian(
        "Протоколи IPv4 і IPv6 працюють у мережі 5G на x86; машини Z3 використовували RC4.", audit_options);
    expect_eq("technical alphanumeric identifiers",
              technical_identifiers,
              "Протоколи ай пі версії чотири і ай пі версії шість працюють у мережі п'ять джі на ікс вісімдесят "
              "шість; машини зед три використовували ар сі чотири.");
    expect_eq("technical alphanumeric identifiers are idempotent",
              normalize_ukrainian(technical_identifiers, audit_options),
              technical_identifiers);
    expect_eq("single latin initial is stable",
              normalize_ukrainian(normalize_ukrainian("Andrew S.", audit_options), audit_options),
              normalize_ukrainian("Andrew S.", audit_options));
    expect_eq("mixed vulgar fraction", normalize_ukrainian("2½", audit_options), "дві цілих і одна друга");
    expect_eq("measured mixed vulgar fraction",
              normalize_ukrainian("2½ кг", audit_options),
              "дві цілих і одна друга кілограма");
    expect_eq("measured fraction", normalize_ukrainian("3/4 кг", audit_options), "три четвертих кілограма");
    expect_eq("signed leading-dot measurement",
              normalize_ukrainian("-.5 кг", audit_options),
              "мінус нуль цілих і п'ять десятих кілограма");
    expect_eq("temperature tolerance",
              normalize_ukrainian("5±0,2 °C", audit_options),
              "п'ять плюс мінус нуль цілих і дві десятих градуса Цельсія");
    expect_eq("bare Celsius range", normalize_ukrainian("5-7 C", audit_options), "від п'яти до семи градусів Цельсія");
    expect_eq("midnight AM", normalize_ukrainian("12:00 AM", audit_options), "опівночі");
    auto short_clock_options = audit_options;
    short_clock_options.colon_style = uktextnorm::ColonStyle::Clock;
    expect_eq("short-minute clock", normalize_ukrainian("10:5", short_clock_options), "десять годин п'ять хвилин");
    expect_eq("invalid timezone preserved", normalize_ukrainian("10:30 UTC+14:30", audit_options), "10:30 UTC+14:30");
    expect_eq("invalid calendar date preserved", normalize_ukrainian("29.02.2023", audit_options), "29.02.2023");
    expect_eq(
        "out-of-range geo URI preserved", normalize_ukrainian("geo:90.0001,180", audit_options), "geo:90.0001,180");
    expect_eq(
        "coordinate beats Newton symbol", normalize_ukrainian("3°N", audit_options), "три градуси північної широти");
    expect_eq("spaced Newton symbol", normalize_ukrainian("3 °N", audit_options), "три градуси Ньютона");
    expect_eq("legal article words", normalize_ukrainian("статті 5—7", audit_options), "від п'ятої до сьомої статті");
    expect_eq("basis points not address", normalize_ukrainian("10 б.п.", audit_options), "десять базисних пунктів");
    expect_eq("grouped symbol currency",
              normalize_ukrainian("$1,234.56", audit_options),
              "тисяча двісті тридцять чотири долари п'ятдесят шість центів");
    expect_eq("regional currency", normalize_ukrainian("CA$5", audit_options), "п'ять канадських доларів");
    expect_eq(
        "case-insensitive regional currency", normalize_ukrainian("ca$5", audit_options), "п'ять канадських доларів");
    expect_eq("single grouped currency",
              normalize_ukrainian("$1,234", audit_options),
              "тисяча двісті тридцять чотири долари");
    expect_eq("accounting currency", normalize_ukrainian("($5)", audit_options), "мінус п'ять доларів");
    expect_eq("ISO currency", normalize_ukrainian("5 AED", audit_options), "п'ять дирхамів ОАЕ");
    expect_eq("three-digit currency minor unit",
              normalize_ukrainian("1.234 BHD", audit_options),
              "один бахрейнський динар двісті тридцять чотири філси");
    expect_eq("four-digit currency minor unit",
              normalize_ukrainian("1.2345 CLF", audit_options),
              "одна чилійська розрахункова одиниця дві тисячі триста сорок п'ять десятитисячних частин");
    expect_eq(
        "zero-digit currency decimal", normalize_ukrainian("1.5 JPY", audit_options), "одна ціла і п'ять десятих єн");
    expect_eq("fiat pair", normalize_ukrainian("AED/USD", audit_options), "дирхамів ОАЕ до доларів США");
    expect_eq("named cryptocurrency", normalize_ukrainian("2 AVAX", audit_options), "два аваланчі");
    expect_eq("lowercase named cryptocurrency", normalize_ukrainian("2 avax", audit_options), "два аваланчі");
    expect_eq("prefixed cryptocurrency", normalize_ukrainian("BTC 2", audit_options), "два біткоїни");
    expect_eq("grouped cryptocurrency", normalize_ukrainian("1,000 BTC", audit_options), "тисяча біткоїнів");
    expect_eq("localized grouped cryptocurrency",
              normalize_ukrainian("1.000,25 ETH", audit_options),
              "тисяча цілих і двадцять п'ять сотих ефіра");
    expect_eq(
        "bitcoin symbol prefix", normalize_ukrainian("₿0.5", audit_options), "нуль цілих і п'ять десятих біткоїна");
    expect_eq(
        "bitcoin symbol suffix", normalize_ukrainian("0,5 ₿", audit_options), "нуль цілих і п'ять десятих біткоїна");
    expect_eq("generic cryptocurrency ticker",
              normalize_ukrainian("0.25 NEWCOIN", audit_options),
              "нуль цілих і двадцять п'ять сотих ен і дабл ю сі оу ай ен");
    expect_eq("generic cryptocurrency pair",
              normalize_ukrainian("NEWCOIN/USDT", audit_options),
              "ен і дабл ю сі оу ай ен до тезерів");
    expect_eq("technical slash acronyms are not finance pairs",
              normalize_ukrainian("Протоколи TCP/IP та IPX/SPX.", audit_options),
              "Протоколи ті сі пі слеш ай пі та ай пі екс слеш ес пі екс.");
    expect_eq("technical acronym numbers keep their order",
              normalize_ukrainian("Стандарти ISO 3166 та IEEE 802.3; мова ALGOL 58.", audit_options),
              "Стандарти ай ес оу три тисячі сто шістдесят шість та ай і і і вісімсот два крапка три; мова ей ел "
              "джі оу ел п'ятдесят вісім.");
    expect_eq(
        "lowercase known cryptocurrency pair", normalize_ukrainian("btc/eth", audit_options), "біткоїнів до ефірів");

    std::istringstream iso_codes(
        "AFN EUR ALL DZD USD AOA XCD XAD ARS AMD AWG AUD AZN BSD BHD BDT BBD BYN BZD XOF BMD INR BTN BOB BOV BAM "
        "BWP NOK BRL BND BIF CVE KHR XAF CAD KYD CLP CLF CNY COP COU KMF CDF NZD CRC CUP XCG CZK DKK DJF DOP EGP "
        "SVC ERN SZL ETB FKP FJD XPF GMD GEL GHS GIP GTQ GBP GNF GYD HTG HNL HKD HUF ISK IDR XDR IRR IQD ILS "
        "JMD JPY JOD KZT KES KPW KRW KWD KGS LAK LBP LSL ZAR LRD LYD CHF MOP MKD MGA MWK MYR MVR MRU MUR XUA "
        "MXN MXV MDL MNT MAD MZN MMK NAD NPR NIO NGN OMR PKR PAB PGK PYG PEN PHP PLN QAR RON RUB RWF SHP WST "
        "STN SAR RSD SCR SLE SGD XSU SBD SOS SSP LKR SDG SRD SEK CHE CHW SYP TWD TJS TZS THB TOP TTD TND TRY "
        "TMT UGX UAH AED USN UYU UYI UYW UZS VUV VES VED VND YER ZMW ZWG XBA XBB XBC XBD XTS XXX XAU XPD XPT "
        "XAG");
    for (std::string code; iso_codes >> code;) {
        expect_not_contains("ISO 4217 coverage " + code, normalize_ukrainian("2 " + code, audit_options), code);
    }
    expect_eq("invalid bracketed IPv6 port preserved",
              normalize_ukrainian("[2001:db8::1]:65536", audit_options),
              "[2001:db8::1]:65536");
    expect_eq("invalid bare timezone preserved", normalize_ukrainian("10:30 +14:01", audit_options), "10:30 +14:01");
    expect_eq("Cisco MAC",
              normalize_ukrainian("aabb.ccdd.eeff", audit_options),
              "мак адреса ей ей двокрапка бі бі двокрапка сі сі двокрапка ді ді двокрапка і і двокрапка еф еф");

    for (int i = 1; i < argc; ++i) {
        run_golden_file(argv[i]);
    }

    return failures == 0 ? 0 : 1;
}
