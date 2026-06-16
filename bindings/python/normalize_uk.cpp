#include "rozpodil/rozpodil.hpp"
#include "uktextnorm/uktextnorm.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <string>
#include <string_view>
#include <vector>

namespace py = pybind11;

namespace {

struct Substring {
    std::size_t start = 0;
    std::size_t stop = 0;
    std::string text;

    friend bool operator==(const Substring&, const Substring&) = default;
};

std::string span_repr(std::string_view type_name, std::size_t start, std::size_t stop, const std::string& text)
{
    return "<" + std::string(type_name) + " start=" + std::to_string(start) + " stop=" + std::to_string(stop) +
           " text=" + py::repr(py::str(text)).cast<std::string>() + ">";
}

std::vector<Substring> copy_substrings(const std::vector<rozpodil::Substring>& chunks)
{
    std::vector<Substring> out;
    out.reserve(chunks.size());
    for (const auto& chunk : chunks) {
        out.push_back({chunk.start, chunk.stop, std::string(chunk.text)});
    }
    return out;
}

std::vector<Substring> split_sentences(std::string_view text)
{
    return copy_substrings(rozpodil::split_sentences(text));
}

std::vector<Substring> sentenize(std::string_view text)
{
    return split_sentences(text);
}

std::vector<Substring> tokenize(std::string_view text)
{
    return copy_substrings(rozpodil::tokenize(text));
}

} // namespace

PYBIND11_MODULE(_normalize_uk, m)
{
    m.doc() = "Python bindings for Ukrainian text normalization and tokenization utilities.";

    py::enum_<uktextnorm::UncertaintyCategory>(m, "UncertaintyCategory")
        .value("AmbiguousAbbreviation", uktextnorm::UncertaintyCategory::AmbiguousAbbreviation)
        .value("BareNumber", uktextnorm::UncertaintyCategory::BareNumber)
        .value("Currency", uktextnorm::UncertaintyCategory::Currency)
        .value("Date", uktextnorm::UncertaintyCategory::Date)
        .value("Identifier", uktextnorm::UncertaintyCategory::Identifier)
        .value("ForeignWord", uktextnorm::UncertaintyCategory::ForeignWord)
        .value("MixedScript", uktextnorm::UncertaintyCategory::MixedScript)
        .value("RomanNumeral", uktextnorm::UncertaintyCategory::RomanNumeral)
        .value("Unit", uktextnorm::UncertaintyCategory::Unit)
        .value("Web", uktextnorm::UncertaintyCategory::Web);

    py::enum_<uktextnorm::UncertaintySeverity>(m, "UncertaintySeverity")
        .value("Info", uktextnorm::UncertaintySeverity::Info)
        .value("Warning", uktextnorm::UncertaintySeverity::Warning)
        .value("Error", uktextnorm::UncertaintySeverity::Error);

    py::enum_<uktextnorm::RangeStyle>(m, "RangeStyle")
        .value("Compact", uktextnorm::RangeStyle::Compact)
        .value("FromTo", uktextnorm::RangeStyle::FromTo);

    py::enum_<uktextnorm::PhoneStyle>(m, "PhoneStyle")
        .value("Grouped", uktextnorm::PhoneStyle::Grouped)
        .value("DigitByDigit", uktextnorm::PhoneStyle::DigitByDigit);

    py::enum_<uktextnorm::SymbolStyle>(m, "SymbolStyle")
        .value("Expand", uktextnorm::SymbolStyle::Expand)
        .value("Preserve", uktextnorm::SymbolStyle::Preserve);

    py::enum_<uktextnorm::DateStyle>(m, "DateStyle")
        .value("Formal", uktextnorm::DateStyle::Formal)
        .value("Spoken", uktextnorm::DateStyle::Spoken);

    py::enum_<uktextnorm::NormalizePreset>(m, "NormalizePreset")
        .value("Default", uktextnorm::NormalizePreset::Default)
        .value("TtsFriendly", uktextnorm::NormalizePreset::TtsFriendly)
        .value("Conservative", uktextnorm::NormalizePreset::Conservative)
        .value("SearchIndexing", uktextnorm::NormalizePreset::SearchIndexing);

    py::class_<uktextnorm::UncertainSpan>(m, "UncertainSpan")
        .def_readonly("start", &uktextnorm::UncertainSpan::start)
        .def_readonly("stop", &uktextnorm::UncertainSpan::stop)
        .def_readonly("text", &uktextnorm::UncertainSpan::text)
        .def_readonly("reason", &uktextnorm::UncertainSpan::reason)
        .def_readonly("category", &uktextnorm::UncertainSpan::category)
        .def_readonly("severity", &uktextnorm::UncertainSpan::severity)
        .def(
            "__eq__",
            [](const uktextnorm::UncertainSpan& left, const uktextnorm::UncertainSpan& right) { return left == right; })
        .def("__repr__", [](const uktextnorm::UncertainSpan& span) {
            return span_repr("UncertainSpan", span.start, span.stop, span.text);
        });

    py::class_<uktextnorm::NormalizeOptions>(m, "NormalizeOptions")
        .def(py::init([](uktextnorm::NormalizePreset preset) { return uktextnorm::options_for_preset(preset); }),
             py::arg("preset") = uktextnorm::NormalizePreset::Default)
        .def_readwrite("expand_known_acronyms", &uktextnorm::NormalizeOptions::expand_known_acronyms)
        .def_readwrite("spell_unknown_acronyms", &uktextnorm::NormalizeOptions::spell_unknown_acronyms)
        .def_readwrite("normalize_english_words", &uktextnorm::NormalizeOptions::normalize_english_words)
        .def_readwrite("transliterate_latin", &uktextnorm::NormalizeOptions::transliterate_latin)
        .def_readwrite("range_style", &uktextnorm::NormalizeOptions::range_style)
        .def_readwrite("phone_style", &uktextnorm::NormalizeOptions::phone_style)
        .def_readwrite("symbol_style", &uktextnorm::NormalizeOptions::symbol_style)
        .def_readwrite("date_style", &uktextnorm::NormalizeOptions::date_style);

    py::class_<Substring>(m, "Substring")
        .def_readonly("start", &Substring::start)
        .def_readonly("stop", &Substring::stop)
        .def_readonly("text", &Substring::text)
        .def("__eq__", [](const Substring& left, const Substring& right) { return left == right; })
        .def("__repr__",
             [](const Substring& span) { return span_repr("Substring", span.start, span.stop, span.text); });

    m.def("options_for_preset", &uktextnorm::options_for_preset, py::arg("preset"));
    m.def("number_to_words", &uktextnorm::number_to_words, py::arg("n"));
    m.def("number_to_words_digit_by_digit", &uktextnorm::number_to_words_digit_by_digit, py::arg("digits"));
    m.def("number_to_ordinal_words", &uktextnorm::number_to_ordinal_words, py::arg("n"), py::arg("form") = "nom_m");
    m.def("number_to_words_case", &uktextnorm::number_to_words_case, py::arg("n"), py::arg("grammatical_case"));
    m.def("normalize_abbreviations", &uktextnorm::normalize_abbreviations, py::arg("text"));
    m.def("expand_abbreviations", &uktextnorm::expand_abbreviations, py::arg("text"));
    m.def("transliterate_to_cyrillic", &uktextnorm::transliterate_to_cyrillic, py::arg("text"));
    m.def("cyrilize", &uktextnorm::cyrilize, py::arg("text"));
    m.def("cyrrilize", &uktextnorm::cyrrilize, py::arg("text"));
    m.def(
        "normalize_ukrainian",
        [](std::string_view text) { return uktextnorm::normalize_ukrainian(text); },
        py::arg("text"));
    m.def(
        "normalize_ukrainian",
        [](std::string_view text, const uktextnorm::NormalizeOptions& options) {
            return uktextnorm::normalize_ukrainian(text, options);
        },
        py::arg("text"),
        py::arg("options"));
    m.def(
        "normalize_ukrainian",
        [](std::string_view text, uktextnorm::NormalizePreset preset) {
            return uktextnorm::normalize_ukrainian(text, preset);
        },
        py::arg("text"),
        py::arg("preset"));
    m.def(
        "normalize_ukrainian_with_preset",
        [](std::string_view text, uktextnorm::NormalizePreset preset) {
            return uktextnorm::normalize_ukrainian_with_preset(text, preset);
        },
        py::arg("text"),
        py::arg("preset") = uktextnorm::NormalizePreset::Default);
    m.def("flag_uncertain", &uktextnorm::flag_uncertain, py::arg("text"));
    m.def("split_sentences", &split_sentences, py::arg("text"));
    m.def("sentenize", &sentenize, py::arg("text"));
    m.def("tokenize", &tokenize, py::arg("text"));
}
