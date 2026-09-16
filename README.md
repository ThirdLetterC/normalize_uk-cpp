# normalize-uk-cpp

[![CI](https://github.com/ThirdLetterC/normalize_uk-cpp/actions/workflows/ci.yml/badge.svg)](https://github.com/ThirdLetterC/normalize_uk-cpp/actions/workflows/ci.yml)
[![Release](https://github.com/ThirdLetterC/normalize_uk-cpp/actions/workflows/release.yml/badge.svg)](https://github.com/ThirdLetterC/normalize_uk-cpp/actions/workflows/release.yml)
[![PyPI](https://img.shields.io/pypi/v/normalize-uk.svg)](https://pypi.org/project/normalize-uk/)

C++23 Ukrainian text normalization and tokenization utilities with optional Python 3.10+ bindings.

## CMake

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

Enable Python bindings explicitly when building with CMake:

```sh
cmake -S . -B build-python -DNORMALIZE_UK_CPP_BUILD_PYTHON=ON
cmake --build build-python
```

A regular CMake install includes the C++ library, headers, and an exported CMake target file.
Python wheels contain only the Python package and compiled extension.

## Python

[Install `uv`](https://docs.astral.sh/uv/getting-started/installation/) first. To run
the published package from PyPI without using this checkout's project environment:

```sh
uv run --no-project --with normalize-uk python
```

To build and work with this checkout, sync its locked dependencies and start Python:

```sh
uv sync
uv run python
```

In either Python session, try:

```python
import normalize_uk as nuk

print(nuk.number_to_words(123))
print(nuk.normalize_ukrainian("01.05.2024"))
print(nuk.normalize_ukrainian("01.05.2024", preset=nuk.NormalizePreset.TtsFriendly))
print(nuk.normalize_ukrainian_many(["01.05.2024", "5 кг"]))
print([sentence.text for sentence in nuk.split_sentences("П'ять зв'язків. Два.")])
print([token.text for token in nuk.tokenize("П'ять зв'язків.")])
```

More examples live in `examples/python/`.

Tags matching the version in `pyproject.toml` (for example, `v0.4.7`) trigger
wheel builds for supported Python versions. The workflow uploads the wheels to
GitHub Release Assets, then downloads those Assets and publishes them to PyPI.

`NormalizeOptions` accepts a preset and named overrides at construction time:

```python
options = nuk.NormalizeOptions(
    preset=nuk.NormalizePreset.TtsFriendly,
    range_style=nuk.RangeStyle.Compact,
    numeric_date_order=nuk.NumericDateOrder.DayMonthYear,
)
result = nuk.normalize_ukrainian("5–7 кг", options=options)
spans = nuk.flag_uncertain("10:30, $12", options=options)
```

Pass either `options=` or `preset=` to `normalize_ukrainian` and `flag_uncertain`.
`normalize_ukrainian_many()` accepts any iterable of Python strings and applies one
options snapshot to the entire batch. It returns a list in input order and raises
`TypeError` if an item is not a string. It accepts the same `options=` and `preset=`
selection as `normalize_ukrainian`. Identical strings within one batch are
normalized once and their result is reused.
The older positional options/preset calls and `normalize_ukrainian_with_preset()` remain available.
Without options, `flag_uncertain()` reports all ambiguity candidates. With explicit options
or a preset, it omits warnings for ambiguous dates, colon pairs, and currency symbols
when the selected policy resolves them; invalid-value diagnostics remain.

`Substring.start`/`stop` and `UncertainSpan.start`/`stop` are Python `str` indexes,
with `stop` exclusive: `span.text == source[span.start:span.stop]`. They count Unicode
code points, matching Python slicing, rather than UTF-8 bytes.

`number_to_words()`, `number_to_ordinal_words()`, and `number_to_words_case()` accept
integers from 0 through `999999999999999999`. Values outside that range raise
`ValueError`; non-integers raise `TypeError`. `number_to_words_digit_by_digit()` accepts
a nonempty string of ASCII digits only and preserves leading zeroes. Empty strings or
strings containing other characters raise `ValueError`; non-strings raise `TypeError`.
Ordinal forms are `nom_m`, `nom_n`, `nom_f`, `nom_pl`, `gen`, `dat`,
`prep`, `loc`, `pl`, `loc_pl`, `acc_f`, `gen_f`, `ins`, `ins_f`, `ins_pl`, and `loc_f`.
Cardinal cases are `gen`, `dat`, `instr`, and `prep`. Unknown forms raise `ValueError`.

The legacy spellings `sentenize()`, `cyrilize()`, and `cyrrilize()` remain available
but issue `DeprecationWarning`; use `split_sentences()` and
`transliterate_to_cyrillic()` in new code.

`NormalizeOptions`, `Substring`, and `UncertainSpan` support `copy.copy()`,
`copy.deepcopy()`, and `pickle` serialization. Copies are independent value objects.

## Custom vocabulary

Pass a Python dict directly when no file is needed:

```python
words = {"Acme": "акме", "Google": "гуголь"}
print(nuk.normalize_ukrainian("Google і Acme", vocabulary=words))  # гуголь і акме
```

`normalize_ukrainian_many()` and `flag_uncertain()` accept the same
`vocabulary=` keyword. A dict can also be the second positional argument.
When passed alongside `options=`, its entries override matching words for that
call while leaving the options object unchanged.

Save user-supplied word readings as a UTF-8 TSV file with the same columns as
`data/lexicons/brands.tsv` and `data/lexicons/english_words.tsv`:

```text
latin	cyrillic
Acme	акме
Google	гуголь
```

Load the file into an options value and pass it to the normalizer:

```python
words = nuk.load_vocabulary_tsv("my_words.tsv")
options = nuk.NormalizeOptions(vocabulary=words)
print(nuk.normalize_ukrainian("Google і Acme", options=options))  # гуголь і акме
```

The same file works in C++:

```cpp
uktextnorm::NormalizeOptions options;
options.vocabulary = uktextnorm::load_vocabulary_tsv("my_words.tsv");
auto text = uktextnorm::normalize_ukrainian("Google і Acme", options);
```

For the CLI, run `uktextnorm --vocabulary my_words.tsv "Google і Acme"`.
Latin keys are single ASCII words, matched without regard to case. Uploaded
entries override built-in brand and English-word readings only for calls using
those options. The `normalize_english_words` switch also controls custom readings.

## Currency and cryptocurrency coverage

Normalization covers 178 ISO 4217 List One codes from the 2026-01-01 data
snapshot, including their 0-, 2-, 3-, or 4-digit minor-unit rules. More than 70
common cryptocurrency and finance tickers have natural Ukrainian readings. Other
2–10 character uppercase alphanumeric tickers are spelled out after amounts and
when paired with a known asset, so newly introduced assets do not require an
immediate library release.
Prefix and suffix
amounts, localized thousands separators, signs, decimals, and the `₿` symbol
are supported.

## Ambiguity controls

`NormalizeOptions` keeps backward-compatible defaults while allowing callers to resolve ambiguous input explicitly:

- `colon_style`: contextual clock/ratio detection, forced clock, or forced ratio.
- `numeric_date_order`: day-month-year, month-day-year, or preservation of dates where both fields are at most 12.
- `currency_symbol_policy`: assume the common currency for `$` and `¥`, or preserve those ambiguous symbols.

The CLI exposes the same controls through `--colon-style`, `--date-order`, and
`--preserve-ambiguous-currency`.

## Benchmarks and fuzzing

Build and run the native benchmark in Release mode:

```sh
cmake -S . -B build-bench-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-bench-release --target uktextnorm_benchmark --parallel
./build-bench-release/uktextnorm_benchmark . --no-per-case --preset Default --target-bytes 262144
```

For Python binding timings from this checkout, run
`uv run python benchmarks/python_binding_benchmark.py`. It compares scalar and
batched normalization, including unique inputs, and span-returning calls.

Measured on 2026-09-15 with an Intel Core i9-9900K (Linux, GCC 13.3 Release
build with `-O3`, Python 3.14.7). These are medians of three native runs or the
Python benchmark's three `timeit` repeats:

| Benchmark | Measured speed |
| --- | ---: |
| C++ `Default`: 27-case corpus, 1,900 UTF-8 bytes per pass | 271 normalizations/s; 18.6 KiB/s |
| Python scalar: 1,000 short strings (4 unique) | 1.04 s per 1,000 strings |
| Python batch: same 1,000 short strings | 4.36 ms per 1,000 strings (238× faster) |
| Python scalar: 100 unique strings | 104.7 ms per 100 strings |
| Python batch: same 100 unique strings | 102.4 ms per 100 strings |

The C++ run processed 260,300 input bytes across 137 corpus passes. Batch
normalization reuses results for repeated strings; unique inputs show little
speed difference. Timings vary with hardware, build settings, and input text.

With Clang and libFuzzer support, build the normalization harness with sanitizers:

```sh
cmake -S . -B build-fuzz -DCMAKE_CXX_COMPILER=clang++ -DNORMALIZE_UK_CPP_BUILD_FUZZER=ON
cmake --build build-fuzz --target uktextnorm_fuzzer
./build-fuzz/uktextnorm_fuzzer -max_total_time=60 tests/data
```

## Development

Install `uv` and `just` for development. Run `just` to see all recipes. `just format` applies Ruff fixes and Python formatting; `just lint` runs the Python static checks; `just check` also runs Python and C++ tests.

```sh
just setup
just format
just lint
just check
just wheel 3.15
```

The project includes a `.clang-format` file and a CMake formatting target. Install `clang-format`, then run:

```sh
cmake --build build --target format
```
