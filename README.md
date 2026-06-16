# normalize-uk-cpp

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

## Python

```sh
python -m pip install .
```

```python
import normalize_uk as n2w

print(n2w.number_to_words(123))
print(n2w.normalize_ukrainian("01.05.2024"))
print(n2w.normalize_ukrainian_with_preset("01.05.2024", n2w.NormalizePreset.TtsFriendly))
print([token.text for token in n2w.tokenize("П'ять зв'язків.")])
```

More examples live in `examples/python/`.

## Formatting

The project includes a `.clang-format` file and a CMake formatting target. Install `clang-format`, then run:

```sh
cmake --build build --target format
```
