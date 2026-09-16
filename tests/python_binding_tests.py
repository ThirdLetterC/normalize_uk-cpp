import copy
import pickle
import tempfile
import unittest
from concurrent.futures import ThreadPoolExecutor
from enum import Enum
from pathlib import Path
from typing import Any, cast

import normalize_uk as nuk
from normalize_uk import _normalize_uk as native


class NormalizeUkBindingTests(unittest.TestCase):
    def setUp(self) -> None:
        self.options = nuk.NormalizeOptions(preset=nuk.NormalizePreset.TtsFriendly)
        self.options.range_style = nuk.RangeStyle.FromTo

    def test_temperature_range(self) -> None:
        self.assertEqual(
            nuk.normalize_ukrainian("5–7 °C", self.options),
            "від п'яти до семи градусів Цельсія",
        )

    def test_signed_temperature_range(self) -> None:
        self.assertEqual(
            nuk.normalize_ukrainian("від -5 до +7 °C", self.options),
            "від мінус п'яти до плюс семи градусів Цельсія",
        )

    def test_decimal_and_unicode_temperature_range(self) -> None:
        self.assertEqual(
            nuk.normalize_ukrainian("5,5–7,5 ℃", self.options),
            "від п'яти цілих і п'яти десятих до семи цілих і п'яти десятих градуса Цельсія",
        )

    def test_temperature_scales_beyond_celsius_and_fahrenheit(self) -> None:
        cases = {
            "250–300 K": "від двохсот п'ятдесяти до трьохсот кельвінів",
            "5–7 °R": "від п'яти до семи градусів Ранкіна",
            "5–7 °Ré": "від п'яти до семи градусів Реомюра",
        }
        for source, expected in cases.items():
            with self.subTest(source=source):
                self.assertEqual(
                    nuk.normalize_ukrainian(source, self.options), expected
                )

    def test_dates_times_and_scientific_notation(self) -> None:
        cases = {
            "2026-09": "вересень дві тисячі двадцять шостого року",
            "14.09.26": "чотирнадцятого вересня дві тисячі двадцять шостого року",
            "2026-09-14T10:30:00Z": (
                "чотирнадцятого вересня дві тисячі двадцять шостого року "
                "о десять годин тридцять хвилин за всесвітнім координованим часом"
            ),
            "10:30 PM": "десять годин тридцять хвилин вечора",
            "Зустріч о 10:30": "Зустріч о десятій годині тридцять хвилин",
            "Впродовж 15 хвилин": "Впродовж п'ятнадцяти хвилин",
            "6.02×10²³": "шість цілих і дві сотих помножити на десять у степені двадцять три",
        }
        for source, expected in cases.items():
            with self.subTest(source=source):
                self.assertEqual(
                    nuk.normalize_ukrainian(source, self.options), expected
                )

    def test_units_finance_and_structured_data(self) -> None:
        cases = {
            "6.5 L/100km": "шість цілих і п'ять десятих літра на сто кілометрів",
            "12 oz": "дванадцять унцій",
            "0.5 DOGE": "нуль цілих і п'ять десятих доджкоїна",
            "-$5": "мінус п'ять доларів",
            "10.0.0.0/24": "ай пі десять нуль нуль нуль префікс двадцять чотири",
            "ст. 5–7": "від п'ятої до сьомої статті",
        }
        for source, expected in cases.items():
            with self.subTest(source=source):
                self.assertEqual(
                    nuk.normalize_ukrainian(source, self.options), expected
                )

    def test_ambiguity_policies(self) -> None:
        options = nuk.NormalizeOptions()
        options.colon_style = nuk.ColonStyle.Ratio
        self.assertEqual(
            nuk.normalize_ukrainian("10:30", options), "десять до тридцяти"
        )
        options.numeric_date_order = nuk.NumericDateOrder.MonthDayYear
        self.assertEqual(
            nuk.normalize_ukrainian("03/04/2026", options),
            "четверте березня дві тисячі двадцять шостого року",
        )
        options.currency_symbol_policy = nuk.CurrencySymbolPolicy.PreserveAmbiguous
        self.assertEqual(nuk.normalize_ukrainian("$12", options), "$дванадцять")

    def test_uncertainty_categories(self) -> None:
        cases = {
            "99:30": nuk.UncertaintyCategory.Time,
            "1/0": nuk.UncertaintyCategory.Fraction,
            "999.1.1.1/40": nuk.UncertaintyCategory.Network,
            "1e+": nuk.UncertaintyCategory.Scientific,
        }
        for source, category in cases.items():
            with self.subTest(source=source):
                self.assertTrue(
                    any(
                        span.category == category for span in nuk.flag_uncertain(source)
                    )
                )

    def test_public_helpers_and_tokenization(self) -> None:
        self.assertEqual(nuk.number_to_words(21), "двадцять один")
        self.assertEqual(nuk.number_to_ordinal_words(3, "nom_f"), "третя")
        self.assertEqual(nuk.number_to_words_case(5, "gen"), "п'яти")
        text = "Привіт. Світ!"
        sentences = nuk.split_sentences(text)
        self.assertEqual([part.text for part in sentences], ["Привіт.", "Світ!"])
        for sentence in sentences:
            self.assertEqual(sentence.text, text[sentence.start : sentence.stop])
        with self.assertWarns(DeprecationWarning):
            self.assertEqual(nuk.sentenize(text), sentences)
        tokens = nuk.tokenize("Два слова")
        self.assertTrue(tokens)
        for token in tokens:
            self.assertEqual(token.text, "Два слова"[token.start : token.stop])

    def test_python_api_contracts(self) -> None:
        self.assertIsInstance(nuk.NormalizePreset.Default, Enum)
        options = nuk.NormalizeOptions(
            preset=nuk.NormalizePreset.TtsFriendly,
            range_style=nuk.RangeStyle.Compact,
            validate_dates=False,
        )
        self.assertEqual(options.range_style, nuk.RangeStyle.Compact)
        self.assertFalse(options.validate_dates)
        with self.assertRaises(TypeError):
            nuk.NormalizeOptions(unknown_option=True)  # type: ignore[call-arg]
        with self.assertRaises(TypeError):
            nuk.NormalizeOptions(validate_dates=1)  # type: ignore[arg-type]
        with self.assertRaises(TypeError):
            nuk.NormalizeOptions(range_style=0)  # type: ignore[arg-type]
        with self.assertRaises(TypeError):
            options.validate_dates = 1  # type: ignore[assignment]

        for field, member in (
            ("range_style", nuk.RangeStyle.Compact),
            ("phone_style", nuk.PhoneStyle.Grouped),
            ("symbol_style", nuk.SymbolStyle.Expand),
            ("date_style", nuk.DateStyle.Formal),
            ("colon_style", nuk.ColonStyle.Contextual),
            ("numeric_date_order", nuk.NumericDateOrder.DayMonthYear),
            ("currency_symbol_policy", nuk.CurrencySymbolPolicy.AssumeCommon),
            ("quote_style", nuk.QuoteStyle.Keep),
        ):
            with self.subTest(field=field):
                setattr(options, field, member)
                with self.assertRaisesRegex(
                    TypeError, f"{field} must be a {type(member).__name__} value"
                ):
                    setattr(options, field, 0)
                self.assertEqual(getattr(options, field), member)

        sentence = nuk.split_sentences("Привіт. Світ!")[1]
        self.assertFalse(sentence == object())
        unicode_text = "🙂 Привіт. 🌍 Світ!"
        for scanner in (nuk.split_sentences, nuk.tokenize):
            for chunk in scanner(unicode_text):
                self.assertEqual(chunk.text, unicode_text[chunk.start : chunk.stop])
        uncertain_text = "🙂 Версія XXI і сума 10 PLN; дата 05/06/2024 🌍"
        for span in nuk.flag_uncertain(uncertain_text):
            self.assertEqual(span.text, uncertain_text[span.start : span.stop])
            self.assertFalse(span == object())

        self.assertEqual(nuk.number_to_words_digit_by_digit("001"), "нуль нуль один")
        for bad_digits in ("", "12x3", "１２", "١٢"):
            with self.subTest(digits=bad_digits), self.assertRaises(ValueError):
                nuk.number_to_words_digit_by_digit(bad_digits)
        for number in (-1, 10**18, 2**64):
            with self.subTest(number=number), self.assertRaises(ValueError):
                nuk.number_to_words(number)
        with self.assertRaises(TypeError):
            nuk.number_to_words(True)
        with self.assertRaises(ValueError):
            nuk.number_to_ordinal_words(3, "unknown")  # type: ignore[arg-type]
        with self.assertRaises(ValueError):
            nuk.number_to_words_case(3, "unknown")  # type: ignore[arg-type]

        self.assertEqual(
            nuk.normalize_ukrainian("5 кг", preset=nuk.NormalizePreset.TtsFriendly),
            nuk.normalize_ukrainian_with_preset(
                "5 кг", nuk.NormalizePreset.TtsFriendly
            ),
        )
        with self.assertRaises(ValueError):
            cast(Any, nuk.normalize_ukrainian)(
                "5 кг", options=options, preset=nuk.NormalizePreset.Default
            )

    def test_text_functions_require_str(self) -> None:
        encoded = "🙂 Привіт. Світ!".encode()
        for function in (
            nuk.normalize_ukrainian,
            nuk.flag_uncertain,
            nuk.split_sentences,
            nuk.tokenize,
            nuk.normalize_abbreviations,
            nuk.expand_abbreviations,
            nuk.transliterate_to_cyrillic,
            native.normalize_ukrainian,
            native.flag_uncertain,
            native.split_sentences,
            native.tokenize,
        ):
            with self.subTest(function=function.__name__):
                with self.assertRaises(TypeError):
                    cast(Any, function)(encoded)
        with self.assertRaises(TypeError):
            cast(Any, nuk.normalize_ukrainian)(encoded, options=self.options)
        with self.assertRaises(TypeError):
            cast(Any, nuk.flag_uncertain)(encoded, preset=nuk.NormalizePreset.Default)
        self.assertIn("text: str", native.split_sentences.__doc__ or "")

    def test_policy_aware_uncertainty(self) -> None:
        text = "10:30, $12, 03/04/2026, 99:30"
        default_reasons = {span.reason for span in nuk.flag_uncertain(text)}
        self.assertIn("ambiguous colon pair (clock time or ratio)", default_reasons)
        self.assertIn(
            "ambiguous currency symbol (currency depends on locale)", default_reasons
        )
        self.assertIn(
            "ambiguous numeric date order (day/month or month/day)", default_reasons
        )

        options = nuk.NormalizeOptions(
            colon_style=nuk.ColonStyle.Ratio,
            numeric_date_order=nuk.NumericDateOrder.DayMonthYear,
            currency_symbol_policy=nuk.CurrencySymbolPolicy.AssumeCommon,
        )
        selected_reasons = {
            span.reason for span in nuk.flag_uncertain(text, options=options)
        }
        self.assertNotIn("ambiguous colon pair (clock time or ratio)", selected_reasons)
        self.assertNotIn(
            "ambiguous currency symbol (currency depends on locale)", selected_reasons
        )
        self.assertNotIn(
            "ambiguous numeric date order (day/month or month/day)", selected_reasons
        )
        self.assertIn("invalid clock time", selected_reasons)
        with self.assertRaises(ValueError):
            cast(Any, nuk.flag_uncertain)(
                text, options=options, preset=nuk.NormalizePreset.Default
            )

    def test_parallel_native_calls(self) -> None:
        source = "15.06.2026, +380 67 123-45-67, 5–7 кг. " * 10
        expected = nuk.normalize_ukrainian(source, self.options)
        with ThreadPoolExecutor(max_workers=4) as executor:
            outputs = list(
                executor.map(
                    lambda _: nuk.normalize_ukrainian(source, self.options), range(8)
                )
            )
        self.assertEqual(outputs, [expected] * 8)

    def test_batch_normalization(self) -> None:
        texts = ["5–7 °C", "Привіт.", "🙂 10 кг", ""] * 20
        expected = [nuk.normalize_ukrainian(text, self.options) for text in texts]
        self.assertEqual(
            nuk.normalize_ukrainian_many(iter(texts), self.options), expected
        )
        self.assertEqual(
            nuk.normalize_ukrainian_many(texts, preset=nuk.NormalizePreset.TtsFriendly),
            [
                nuk.normalize_ukrainian(text, preset=nuk.NormalizePreset.TtsFriendly)
                for text in texts
            ],
        )
        self.assertEqual(nuk.normalize_ukrainian_many([]), [])
        self.assertEqual(
            nuk.normalize_ukrainian_many(texts),
            [nuk.normalize_ukrainian(text) for text in texts],
        )
        with self.assertRaises(TypeError):
            cast(Any, nuk.normalize_ukrainian_many)(["Привіт", b"world"])
        with self.assertRaises(ValueError):
            cast(Any, nuk.normalize_ukrainian_many)(
                texts, options=self.options, preset=nuk.NormalizePreset.Default
            )

    def test_custom_vocabulary(self) -> None:
        path = Path(__file__).parent / "data" / "custom_vocabulary.tsv"
        words = nuk.load_vocabulary_tsv(str(path))
        self.assertEqual(words, {"google": "гуголь", "acme": "акме"})
        options = nuk.NormalizeOptions(vocabulary=words)
        self.assertEqual(
            nuk.normalize_ukrainian("Google і Acme", options), "гуголь і акме"
        )
        self.assertEqual(
            nuk.normalize_ukrainian("Google і Acme", vocabulary=words),
            "гуголь і акме",
        )
        self.assertEqual(
            nuk.normalize_ukrainian("Google і Acme", words), "гуголь і акме"
        )
        self.assertEqual(
            nuk.normalize_ukrainian(
                "Google і Acme",
                preset=nuk.NormalizePreset.TtsFriendly,
                vocabulary=words,
            ),
            "гуголь і акме",
        )
        self.assertEqual(nuk.normalize_ukrainian("Google"), "гугл")
        disabled = nuk.NormalizeOptions(
            vocabulary=words, normalize_english_words=False, transliterate_latin=False
        )
        self.assertEqual(
            nuk.normalize_ukrainian("Google і Acme", disabled), "Google і Acme"
        )
        self.assertEqual(
            nuk.normalize_ukrainian_many(["Acme", "Google"], options),
            ["акме", "гуголь"],
        )
        self.assertEqual(
            nuk.normalize_ukrainian_many(["Acme", "Google"], vocabulary=words),
            ["акме", "гуголь"],
        )
        merged = nuk.NormalizeOptions(vocabulary={"acme": "старе", "other": "інше"})
        self.assertEqual(
            nuk.normalize_ukrainian(
                "Acme і Other", options=merged, vocabulary={"Acme": "акме"}
            ),
            "акме і інше",
        )
        self.assertEqual(merged.vocabulary, {"acme": "старе", "other": "інше"})
        self.assertFalse(
            any(
                span.category == nuk.UncertaintyCategory.ForeignWord
                for span in nuk.flag_uncertain("Acme", options=options)
            )
        )
        self.assertFalse(
            any(
                span.category == nuk.UncertaintyCategory.ForeignWord
                for span in nuk.flag_uncertain("Acme", vocabulary=words)
            )
        )
        for clone in (copy.copy(options), pickle.loads(pickle.dumps(options))):
            self.assertEqual(clone.vocabulary, words)
            clone.vocabulary = {"acme": "інше"}
            self.assertEqual(options.vocabulary, words)
        with self.assertRaises(ValueError):
            nuk.NormalizeOptions(vocabulary={"Acme": "акме", "acme": "інше"})
        with self.assertRaises(TypeError):
            cast(Any, nuk.NormalizeOptions)(vocabulary={"acme": 1})
        with self.assertRaises(TypeError):
            cast(Any, nuk.normalize_ukrainian)("Acme", vocabulary=["acme"])
        with tempfile.TemporaryDirectory() as directory:
            invalid_path = Path(directory) / "duplicates.tsv"
            invalid_path.write_text(
                "latin\tcyrillic\nAcme\tакме\nACME\tінше\n", encoding="utf-8"
            )
            with self.assertRaises(ValueError):
                nuk.load_vocabulary_tsv(str(invalid_path))

    def test_value_copy_and_pickle(self) -> None:
        options = nuk.NormalizeOptions(
            preset=nuk.NormalizePreset.TtsFriendly,
            expand_known_acronyms=False,
            spell_unknown_acronyms=False,
            normalize_english_words=False,
            transliterate_latin=False,
            repair_homoglyphs=False,
            range_style=nuk.RangeStyle.FromTo,
            phone_style=nuk.PhoneStyle.DigitByDigit,
            symbol_style=nuk.SymbolStyle.Preserve,
            date_style=nuk.DateStyle.Spoken,
            colon_style=nuk.ColonStyle.Ratio,
            numeric_date_order=nuk.NumericDateOrder.MonthDayYear,
            currency_symbol_policy=nuk.CurrencySymbolPolicy.PreserveAmbiguous,
            quote_style=nuk.QuoteStyle.Guillemets,
            validate_dates=False,
            parse_thousand_separators=False,
            normalize_network_addresses=False,
        )
        fields = (
            "expand_known_acronyms",
            "spell_unknown_acronyms",
            "normalize_english_words",
            "transliterate_latin",
            "repair_homoglyphs",
            "validate_dates",
            "parse_thousand_separators",
            "normalize_network_addresses",
            "range_style",
            "phone_style",
            "symbol_style",
            "date_style",
            "colon_style",
            "numeric_date_order",
            "currency_symbol_policy",
            "quote_style",
        )
        for clone in (
            copy.copy(options),
            copy.deepcopy(options),
            pickle.loads(pickle.dumps(options)),
        ):
            self.assertIsNot(clone, options)
            for field in fields:
                self.assertEqual(getattr(clone, field), getattr(options, field))
            clone.range_style = nuk.RangeStyle.Compact
            self.assertEqual(options.range_style, nuk.RangeStyle.FromTo)

        substring = nuk.tokenize("🙂 Привіт")[0]
        uncertain = nuk.flag_uncertain("10:30")[0]
        for value in (substring, uncertain):
            for clone in (
                copy.copy(value),
                copy.deepcopy(value),
                pickle.loads(pickle.dumps(value)),
            ):
                self.assertIsNot(clone, value)
                self.assertEqual(clone, value)

    def test_options_snapshot_during_concurrent_mutation(self) -> None:
        source = "5–7 °C. " * 20
        compact = nuk.NormalizeOptions(range_style=nuk.RangeStyle.Compact)
        from_to = nuk.NormalizeOptions(range_style=nuk.RangeStyle.FromTo)
        expected = {
            nuk.normalize_ukrainian(source, compact),
            nuk.normalize_ukrainian(source, from_to),
        }
        shared = nuk.NormalizeOptions(range_style=nuk.RangeStyle.Compact)

        def mutate() -> None:
            for index in range(100):
                shared.range_style = (
                    nuk.RangeStyle.FromTo if index % 2 else nuk.RangeStyle.Compact
                )

        def normalize() -> list[str]:
            return [nuk.normalize_ukrainian(source, shared) for _ in range(20)]

        with ThreadPoolExecutor(max_workers=2) as executor:
            writer = executor.submit(mutate)
            reader = executor.submit(normalize)
            writer.result()
            outputs = reader.result()
        self.assertTrue(outputs)
        self.assertTrue(all(output in expected for output in outputs))

    def test_markup_is_preserved(self) -> None:
        self.assertEqual(
            nuk.normalize_ukrainian("<speak>5 кг</speak>", self.options),
            "<speak>п'ять кілограмів</speak>",
        )
        self.assertEqual(
            nuk.normalize_ukrainian(
                "[5 кг](https://example.com?a=1&amp;b=2)", self.options
            ),
            "[п'ять кілограмів](https://example.com?a=1&amp;b=2)",
        )

    def test_regressions_for_mixed_structured_text(self) -> None:
        cases = {
            "−1/2": "мінус одна друга",
            "-2,5 м/с²": "мінус дві цілих і п'ять десятих метра за секунду в квадраті",
            "PT1.5H": "одна ціла і п'ять десятих години",
            "[2001:db8::1]:443": (
                "ай пі версії шість два нуль нуль один двокрапка ді бі вісім "
                "двокрапка скорочення нулів двокрапка один порт чотириста сорок три"
            ),
            "$1,234.56": "тисяча двісті тридцять чотири долари п'ятдесят шість центів",
            "CA$5": "п'ять канадських доларів",
        }
        for source, expected in cases.items():
            with self.subTest(source=source):
                self.assertEqual(
                    nuk.normalize_ukrainian(source, self.options), expected
                )

        self.assertEqual(
            nuk.normalize_ukrainian("[5 кг](https://example.com/a_(b))", self.options),
            "[п'ять кілограмів](https://example.com/a_(b))",
        )
        self.assertEqual(
            nuk.normalize_ukrainian("``код `5 кг` тут``", self.options),
            "``код `5 кг` тут``",
        )

    def test_iso_currencies_and_open_cryptocurrency_tickers(self) -> None:
        cases = {
            "5 AED": "п'ять дирхамів ОАЕ",
            "1.234 BHD": "один бахрейнський динар двісті тридцять чотири філси",
            "1.2345 CLF": (
                "одна чилійська розрахункова одиниця "
                "дві тисячі триста сорок п'ять десятитисячних частин"
            ),
            "1.5 JPY": "одна ціла і п'ять десятих єн",
            "2 AVAX": "два аваланчі",
            "BTC 2": "два біткоїни",
            "1,000 BTC": "тисяча біткоїнів",
            "1.000,25 ETH": "тисяча цілих і двадцять п'ять сотих ефіра",
            "₿0.5": "нуль цілих і п'ять десятих біткоїна",
            "0.25 NEWCOIN": "нуль цілих і двадцять п'ять сотих ен і дабл ю сі оу ай ен",
            "NEWCOIN/USDT": "ен і дабл ю сі оу ай ен до тезерів",
            "btc/eth": "біткоїнів до ефірів",
        }
        for source, expected in cases.items():
            with self.subTest(source=source):
                self.assertEqual(
                    nuk.normalize_ukrainian(source, self.options), expected
                )


if __name__ == "__main__":
    unittest.main()
