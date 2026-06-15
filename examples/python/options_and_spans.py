#!/usr/bin/env python3
"""Normalize with options and inspect uncertain spans."""

import normalize_uk as nuk


def describe_spans(text: str) -> None:
    for span in nuk.flag_uncertain(text):
        print(f"{span.start}:{span.stop} {span.text!r} {span.category.name}/{span.severity.name}: {span.reason}")


def main() -> None:
    options = nuk.NormalizeOptions()
    options.range_style = nuk.RangeStyle.FromTo
    options.phone_style = nuk.PhoneStyle.DigitByDigit
    options.date_style = nuk.DateStyle.Spoken

    text = "15.06.2026, +380 67 123-45-67, 5-7 кг"
    print(nuk.normalize_ukrainian(text, options))
    print(nuk.normalize_ukrainian("OpenAI + ФОП", nuk.NormalizePreset.SearchIndexing))

    describe_spans("Версія XXI і сума 10 PLN")


if __name__ == "__main__":
    main()
