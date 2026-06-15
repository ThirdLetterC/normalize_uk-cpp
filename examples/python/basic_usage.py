#!/usr/bin/env python3
"""Small tour of normalize_uk's most common Python calls."""

import normalize_uk as nuk


def main() -> None:
    print(nuk.number_to_words(1234567))
    print(nuk.number_to_ordinal_words(21, "nom_f"))
    print(nuk.normalize_ukrainian("01.05.2024, +380 67 123-45-67, 5 кг"))

    text = "Пишіть на test.user@example.com. Зустріч о 10:30."
    print([sentence.text for sentence in nuk.sentenize(text)])
    print([token.text for token in nuk.tokenize("П'ять зв'язків.")])


if __name__ == "__main__":
    main()
