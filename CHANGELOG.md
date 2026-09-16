# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and the project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.4.7] - 2026-09-16

### Fixed

- `впродовж` and `упродовж` now govern the genitive case like `протягом`, so
  counts use genitive readings («Впродовж 15 хвилин» → «Впродовж п'ятнадцяти хвилин»).
- Clock times after `о` / `об` now use the locative hour («о 10:30» →
  «о десятій годині тридцять хвилин»); minutes and seconds are unchanged.

## [0.4.6] - 2026-09-15

### Changed

- Python package metadata now links to the repository, issue tracker, and
  changelog, and describes its Ukrainian text-processing focus on PyPI.

## [0.4.5] - 2026-09-15

### Added

- Expanded built-in readings for common brands, English technical terms,
  Ukrainian acronyms and abbreviations, counted nouns, and Latin measurement
  symbols, including micro-unit variants.
- Added many more noun families, product and technical readings, and compound
  measurement aliases. Instrumental and locative noun forms now live in a
  dedicated lexicon table, with new forms for both earlier and newly added nouns.

### Fixed

- Counted noun readings no longer consume the denominator of a mixed fraction,
  and expanded abbreviations preserve an initial capital letter.
- Article counts retain feminine agreement in ordinary prose without changing
  legal labels such as `частина 2 стаття 19`.

## [0.4.4] - 2026-09-15

### Added

- Callers can load a UTF-8 Latin-to-Ukrainian vocabulary TSV at runtime and
  attach its readings to `NormalizeOptions` in C++, Python, or the CLI. Custom
  entries override built-in brand and English-word readings per options value.
- Python callers can pass a vocabulary dict directly to normalization and
  uncertainty calls; call-specific entries override matching option entries
  without changing the supplied options object.

### Changed

- Decomposed large normalization and tokenization modules into focused source
  files while preserving their public APIs and behavior.

## [0.4.3] - 2026-09-15

### Changed

- The README now documents installation from the published PyPI package and links
  its version badge. It also clarifies the CMake install contents, Python digit
  input errors, and the currency-data snapshot date.

## [0.4.2] - 2026-09-15

### Changed

- Python wheels now contain only the Python package and extension; C++ archives, headers, and CMake exports remain in the
  standalone CMake installation. Release Python builds enable IPO for the C++ core when the toolchain supports it.

- Python text APIs now require `str` and reject UTF-8 `bytes`, so returned character offsets always index the input.
- Python sentence and token span offsets now index Unicode characters in `str`, matching uncertainty spans and Python
  slicing; callers using UTF-8 byte offsets must adjust their slicing code.
- Python enums are native `enum.Enum` types. Python builds require pybind11 3.0 or newer, including the CMake fallback.
- The Python number helpers reject unsupported values and invalid digit strings or grammatical forms instead of
  silently changing their meaning. Legacy `sentenize`, `cyrilize`, and `cyrrilize` calls now emit `DeprecationWarning`.
- Sentence splitting, tokenization, and uncertainty scanning avoid full-text offset tables and unnecessary copies.

### Added

- `NormalizeOptions` accepts named field overrides at construction, and `normalize_ukrainian` and `flag_uncertain`
  accept explicit `options=` or `preset=` keyword arguments. Policy-aware uncertainty scanning omits ambiguity
  warnings resolved by the supplied policy while retaining invalid-value diagnostics.
- `normalize_ukrainian_many` normalizes an iterable of Python strings with one options snapshot, preserving input
  order and reusing results for identical strings within a batch. It accepts the same `options=` and `preset=`
  selection as `normalize_ukrainian`.
- `NormalizeOptions`, `Substring`, and `UncertainSpan` now support `copy.copy`, `copy.deepcopy`, and `pickle`.
- A Python binding benchmark and regression tests cover batched normalization, value-object serialization, and
  concurrent normalization while `NormalizeOptions` is updated.

### Fixed

- Assigning an invalid enum to `NormalizeOptions` now names the affected field and expected enum type in its `TypeError`.
- Python span equality with unrelated types now returns `False` instead of raising a pybind11 argument error.
- Governed percentage ranges such as `на 60-80%` no longer read a dangling string view; the sanitizer test now passes.

## [0.4.1] - 2026-09-15

### Fixed

- Contextual ranges after `до`, `на`, `в`/`у`, and `близько` no longer insert an incompatible `від`; abbreviated
  year and decade ranges now expand the omitted century instead of reading the second bound as a bare number;
  date ranges after a preposition and day ranges without a year now have natural spoken forms. Prepositions are
  matched as whole words, and school-grade ranges use ordinals without changing ordinary counts of classes.
- Tightly joined scientific powers such as `10−9` and `10−15—10−12` no longer turn into numeric ranges, while
  ordinary hyphen ranges retain their existing reading; adjacent `ρh` variables and inverse Celsius powers no
  longer interrupt the scientific reading.
- Cyrillic Roman-century glyphs such as `ХХ` and `ХХІ`, capitalized Ukrainian month names in dates, and ordinal
  class labels such as `1 класу` now receive context-appropriate spoken forms.
- Regional currency prefixes such as `US$` are resolved before multiplier normalization; unambiguous slash dates
  in month/day order are recognized when day/month order is impossible, while ambiguous dates keep the selected policy.
- Biblical chapter-and-verse references are read before invalid-clock protection and clock normalization.
- Bare IEEE 802 revisions, including letter suffixes and year-like revisions, are read as identifiers rather than
  decimals, measurement units, malformed scientific notation, or numeric ranges.
- Dissertation speciality codes after a `... наук:` label are read as dotted codes rather than invalid dates.
- Year spans after `У`/`В` retain the preposition and gain a grammatical `період від ... до ... року` reading, with
  or without an abbreviated year suffix.
- Wikipedia page-reference markers such as `.:33–34:39–43` are removed before clock validation so their digits are
  not partially spoken; city abbreviations now use `місті` or `міста` after locative or genitive prepositions, and
  the unambiguous form `м. Києва` uses `міста`.
- The speed-of-light variable `с.` after `у вакуумі` is no longer interpreted as a village abbreviation.
- Ordinal suffixes such as `1-ше`, `2-ге`, and `3-тє` now produce ordinal words, while mixed-script data rates such as
  `10 Гбіт/c` read as gigabits per second.
- Progressive-scan video resolutions such as `720p` and `1080p-якістю` now retain the spoken `пі` suffix instead of
  joining a homoglyph to the number.
- Uncertainty checks now read complete UTF-8 Cyrillic unit tokens, avoiding false unknown-unit warnings for known
  abbreviations, compound rates, ordinary prepositions after years, dates followed by month names, chemical formulas,
  and mobile-network generations.

## [0.4.0] - 2026-09-14

### Added

- Temperature normalization for Kelvin, Rankine, Réaumur, Delisle, and Rømer scales, including symbols, names,
  signed values, decimal values, and ranges.
- Date support for `DD-MM-YYYY`, two-digit years, `YYYY/MM/DD`, `YYYY-MM`, month-year expressions, dates without a
  year, cross-month ranges, and ISO date-time values.
- Time support for seconds, AM/PM suffixes, UTC/GMT offsets, midnight, and numeric ratios.
- Explicit ambiguity policies for colon-delimited clock/ratio values, local numeric date order, and ambiguous currency
  symbols, exposed through the C++, Python, and CLI APIs.
- ISO 8601 duration, week-date, and ordinal-date readings, selected IANA timezone names, and validation diagnostics
  for invalid week dates, ordinal dates, and UTC/GMT offsets.
- Scientific notation support for `e` notation, multiplication by powers of ten, ordinary powers, and superscript
  exponents.
- Broader measurement coverage, including SI and IEC data units, imperial units, pressure, energy, power, frequency,
  acceleration, density, data rates, fuel economy, revolutions, decibels, parts per million, basis points, DPI, and
  frame rates.
- Compound engineering and medical measurements for force, torque, viscosity, irradiance, molarity, dosage,
  particulate concentration, and electric-vehicle energy use, plus a composable fallback for products, quotients,
  powers, and compact or percentage tolerances.
- Complete active ISO 4217 List One coverage (178 currency, fund, metal, and reserved codes as published by SIX on
  2026-01-01), including correct 0-, 2-, 3-, and 4-digit minor-unit handling and additional unambiguous symbols.
- Named readings for more than 70 established fiat/crypto finance tickers, plus a conservative fallback that spells a new
  2–10 character uppercase alphanumeric code after an amount or when paired with a recognized asset.
- Bitcoin amounts written with the `₿` symbol, both before and after the amount.
- Crypto amounts with prefix or suffix tickers, localized thousands separators, signs, decimals, and
  case-insensitive known market pairs.
- Structured-data normalization for IPv4 ports and CIDR prefixes, IPv6 CIDR and bracketed endpoints, MAC addresses,
  UUIDs, ISBNs, ISSNs, VINs, SWIFT/BIC codes, and non-Ukrainian IBANs.
- Phone-number support for international `00` prefixes and extension markers such as `доб.`, `дод.`, `ext`, and `x`.
- Decimal and DMS coordinates with Latin or Ukrainian hemisphere markers and coordinate validation.
- Geo URI coordinates, labeled latitude/longitude pairs, optional altitude, and degrees with decimal minutes.
- Legal ranges for articles, parts, points, subpoints, paragraphs, chapters, tables, and figures.
- URL support for FTP, arbitrary top-level domains, fragments, Unicode email addresses, and punycode-like labels.
- Preservation of HTML/SSML tags, comments, fenced code blocks, and inline code during normalization.
- Preservation of balanced MediaWiki `\displaystyle` TeX expressions during surrounding prose normalization.
- Preservation of Markdown link destinations, reference URLs, tilde fences, and HTML character entities while visible
  link text remains normalizable.
- Uncertainty categories for invalid times, fractions, network values, and scientific notation in the C++, Python,
  and CLI APIs.
- Cross-platform CI for Linux, macOS, and Windows, plus Clang AddressSanitizer and UndefinedBehaviorSanitizer checks.
- A Clang/libFuzzer harness covering every preset and uncertainty scanning, with a sanitizer CI smoke test.
- CLI integration tests, expanded Python binding tests, and normalization idempotence coverage.
- Full-sentence TTS golden tests with 56 cases across 28 normalization categories and an idempotence assertion for every
  sentence.
- Corpus-derived regression tests for technical standards, Cyrillic model and chemical identifiers, engineering
  dimensions, scientific notation, data-rate units, Roman-numeral collisions, and punctuation-adjacent ranges.

### Changed

- Measurement and finance lexicons now define a dedicated decimal agreement form.
- Finance normalization is generated from the finance lexicon instead of using a fixed ticker list.
- Unicode token detection now compares complete code points instead of individual UTF-8 bytes.
- Common Latin diacritics are handled by approximate Cyrillic transliteration, while bracketed IPA remains opaque.
- Numeric dates governed by `від`, `до`, `з`, `із`, `після`, or `станом на` now use the Ukrainian genitive day form.
- Sentence-initial numeric governors such as `До`, `Від`, and `Близько` now apply the same grammatical cases as their
  lowercase forms.
- IPv4 addresses, CIDR blocks, and endpoints are recognized immediately before sentence-ending punctuation.
- Golden TSV fixtures are pinned to LF in Git and their readers also accept CRLF checkouts on Windows.
- IPv6 zero compression is pronounced explicitly as `скорочення нулів`.
- Soft-stem ordinal inflection now produces forms such as `третя` and `третього`.
- Technical acronym/version expressions and protocol names such as `ISO 3166`, `IEEE 802.3`, `ALGOL 58`, and `TCP/IP`
  are no longer misclassified as financial amounts or market pairs.
- Ukrainian domain names, bibliographic volume counts, locative numeric phrases, variable ratios, common English tonne
  spellings, and capitalization variants of bit-rate units now receive context-appropriate readings.

### Fixed

- Temperature and measurement ranges now handle hyphen-minus, minus, en dash, and em dash consistently with
  `RangeStyle.FromTo`.
- Standalone and ranged negative temperatures now pronounce their signs naturally.
- Decimal measurements, multipliers, and financial amounts now use grammatically correct agreement, for example
  `2,5 кг` → `дві цілих і п'ять десятих кілограма`.
- Decimal fractions ending in one now use the singular denominator form.
- Signed, symbol-prefixed, code-prefixed, accounting-style, and repeated currency amounts are normalized correctly.
- Signed and mixed fractions are supported, while fractions with a zero denominator are preserved and flagged.
- Structured dates are normalized before generic numeric ranges, preventing date components from being interpreted as
  ranges.
- Invalid clock values, AM/PM values, dates, coordinates, IP addresses, CIDR prefixes, and ports are rejected or
  reported as uncertain instead of receiving misleading readings.
- Multiple occurrences of the same currency in one input are all normalized.
- ISBN-10/13, ISSN, IBAN, payment-card, VIN, UUID, and labeled hash candidates now receive checksum, length, version,
  or variant validation and error-level uncertainty metadata when invalid.
- Unicode minus and Unicode hyphen variants are canonicalized without producing malformed UTF-8, including in
  signed fractions, percentages, measurements, and temperature ranges.
- Signed and leading-dot compound measurements, Latin SI aliases, parenthesized denominators, tolerances, and
  measured fractions now normalize consistently.
- Fractional and week-based ISO durations are supported; malformed scientific notation and invalid structured values
  are preserved instead of being partially normalized.
- Bracketed IPv6 endpoints, Cisco-style MAC addresses, compact labeled UUIDs, ISSN-L values, hyphenated IBANs, and
  labeled 12–19 digit payment-card numbers are recognized without cross-parser collisions.
- Regional currency symbols, lowercase currency codes, locale-grouped amounts, and symbol-prefixed accounting values
  now resolve to the intended currency and sign.
- Markdown destinations with balanced parentheses and double-backtick code spans remain opaque during normalization.
- Legal ranges with word labels, dotted subpoints, and paragraph symbols now honor `RangeStyle.FromTo`.
- Python binding tests now resolve the configuration-specific extension directory correctly with multi-configuration
  generators such as Visual Studio on Windows.
- The Windows CLI now reads command-line arguments as UTF-16 and converts them to UTF-8, preserving Ukrainian text,
  degree symbols, and Unicode dashes passed directly on the command line.
- Mathematical comparisons containing both `<` and `>` are no longer mistaken for HTML tags, and stripping adjacent
  quotation marks no longer joins neighboring words.
- Address abbreviations no longer match suffixes inside ordinary words or reinterpret generated measurement
  abbreviations on a second normalization pass.
- High-precision decimals are no longer parsed as phone numbers; unsupported decimal precision now falls back to a
  digit-by-digit fractional reading without dropping the associated unit.
- Dot decimals, compact and named versions, single-letter recommendations, classification codes, and common technical
  standard designations now receive stable spoken readings.
- Numeric dates consume an already written `року`/`р.` suffix, coordinate directions do not duplicate
  `широти`/`довготи`, and governed coordinate bounds use the genitive case.
- Native Windows normalization no longer exhausts the default executable stack while matching ordinal and Roman
  numeral expressions.
- Technical standards such as `ISO 8512-1:1990`, `ISO/IEC 7812`, `ISO-8859-1`, `IEEE 802 .22`, and
  `ДНАОП 0.00-1.32-01` now preserve component boundaries, leading zeroes, and valid UTF-8 without consuming a
  following prose dash.
- Cyrillic alphanumeric identifiers and dimensions such as `К145ІК512П`, `ВМ-23/25/27/32/1230`, `1-0-1000х630`,
  and `1000 х 630 мм` now receive stable spoken readings instead of colliding with dates, ranges, or temperatures.
- Scientific notation written as `1,76×10-19 Дж`, abbreviated month-year suffixes, zero ordinals, and spaced
  abbreviation punctuation now normalize on the first pass and remain idempotent.
- Spaced rate units, milliseconds, decibel-milliwatts, and cubic centimetres per hour are normalized as measurements;
  a trailing `/ с.` is no longer reinterpreted as the address abbreviation for a village.
- `DVI`, `DL`, and `CLI` are no longer treated as bare Roman numerals, while Roman numerals before `група` or
  `групи` receive the appropriate feminine ordinal form.
- Bare ranges before sentence punctuation, ranges following a punctuation dash, and English `P.`/`pp.` page ranges
  now honor `RangeStyle.FromTo`.

[0.4.7]: https://github.com/ThirdLetterC/normalize_uk-cpp/releases/tag/v0.4.7
[0.4.6]: https://github.com/ThirdLetterC/normalize_uk-cpp/releases/tag/v0.4.6
[0.4.5]: https://github.com/ThirdLetterC/normalize_uk-cpp/releases/tag/v0.4.5
[0.4.4]: https://github.com/ThirdLetterC/normalize_uk-cpp/releases/tag/v0.4.4
[0.4.3]: https://github.com/ThirdLetterC/normalize_uk-cpp/releases/tag/v0.4.3
[0.4.2]: https://github.com/ThirdLetterC/normalize_uk-cpp/releases/tag/v0.4.2
[0.4.1]: https://github.com/ThirdLetterC/normalize_uk-cpp/releases/tag/v0.4.1
[0.4.0]: https://github.com/ThirdLetterC/normalize_uk-cpp/releases/tag/v0.4.0
