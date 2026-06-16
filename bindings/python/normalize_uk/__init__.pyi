from __future__ import annotations

from enum import Enum
from typing import overload


class UncertaintyCategory(Enum):
    AmbiguousAbbreviation: UncertaintyCategory
    BareNumber: UncertaintyCategory
    Currency: UncertaintyCategory
    Date: UncertaintyCategory
    Identifier: UncertaintyCategory
    ForeignWord: UncertaintyCategory
    MixedScript: UncertaintyCategory
    RomanNumeral: UncertaintyCategory
    Unit: UncertaintyCategory
    Web: UncertaintyCategory


class UncertaintySeverity(Enum):
    Info: UncertaintySeverity
    Warning: UncertaintySeverity
    Error: UncertaintySeverity


class RangeStyle(Enum):
    Compact: RangeStyle
    FromTo: RangeStyle


class PhoneStyle(Enum):
    Grouped: PhoneStyle
    DigitByDigit: PhoneStyle


class SymbolStyle(Enum):
    Expand: SymbolStyle
    Preserve: SymbolStyle


class DateStyle(Enum):
    Formal: DateStyle
    Spoken: DateStyle


class NormalizePreset(Enum):
    Default: NormalizePreset
    TtsFriendly: NormalizePreset
    Conservative: NormalizePreset
    SearchIndexing: NormalizePreset


class UncertainSpan:
    @property
    def start(self) -> int: ...
    @property
    def stop(self) -> int: ...
    @property
    def text(self) -> str: ...
    @property
    def reason(self) -> str: ...
    @property
    def category(self) -> UncertaintyCategory: ...
    @property
    def severity(self) -> UncertaintySeverity: ...
    def __eq__(self, other: object) -> bool: ...
    def __repr__(self) -> str: ...


class NormalizeOptions:
    expand_known_acronyms: bool
    spell_unknown_acronyms: bool
    normalize_english_words: bool
    transliterate_latin: bool
    range_style: RangeStyle
    phone_style: PhoneStyle
    symbol_style: SymbolStyle
    date_style: DateStyle

    def __init__(self, preset: NormalizePreset = NormalizePreset.Default) -> None: ...


class Substring:
    @property
    def start(self) -> int: ...
    @property
    def stop(self) -> int: ...
    @property
    def text(self) -> str: ...
    def __eq__(self, other: object) -> bool: ...
    def __repr__(self) -> str: ...


def options_for_preset(preset: NormalizePreset) -> NormalizeOptions: ...
def number_to_words(n: int) -> str: ...
def number_to_words_digit_by_digit(digits: str) -> str: ...
def number_to_ordinal_words(n: int, form: str = "nom_m") -> str: ...
def number_to_words_case(n: int, grammatical_case: str) -> str: ...
def normalize_abbreviations(text: str) -> str: ...
def expand_abbreviations(text: str) -> str: ...
def transliterate_to_cyrillic(text: str) -> str: ...
def cyrilize(text: str) -> str: ...
def cyrrilize(text: str) -> str: ...


@overload
def normalize_ukrainian(text: str) -> str: ...


@overload
def normalize_ukrainian(text: str, options: NormalizeOptions) -> str: ...


@overload
def normalize_ukrainian(text: str, preset: NormalizePreset) -> str: ...


def normalize_ukrainian_with_preset(text: str, preset: NormalizePreset = NormalizePreset.Default) -> str: ...
def flag_uncertain(text: str) -> list[UncertainSpan]: ...
def split_sentences(text: str) -> list[Substring]: ...
def sentenize(text: str) -> list[Substring]: ...
def tokenize(text: str) -> list[Substring]: ...
