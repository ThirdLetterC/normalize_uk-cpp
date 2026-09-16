cmake_minimum_required(VERSION 3.20)

project(
    normalize_uk_cpp
    VERSION 0.4.7
    DESCRIPTION "Ukrainian text normalization and tokenization utilities"
    LANGUAGES CXX
)

if(MSVC)
    add_compile_options(/utf-8)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        add_compile_options(/clang:-ftemplate-depth=2048)
    else()
        add_compile_options(-ftemplate-depth=2048)
    endif()
endif()

include(GNUInstallDirs)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(NORMALIZE_UK_CPP_DEFAULT_BUILD_TESTS ON)
else()
    set(NORMALIZE_UK_CPP_DEFAULT_BUILD_TESTS OFF)
endif()

option(NORMALIZE_UK_CPP_BUILD_TOOLS "Build command-line tools" ON)
option(NORMALIZE_UK_CPP_BUILD_TESTS "Build C++ tests" ${NORMALIZE_UK_CPP_DEFAULT_BUILD_TESTS})
option(NORMALIZE_UK_CPP_BUILD_PYTHON "Build Python 3.10+ bindings" OFF)
option(NORMALIZE_UK_CPP_ENABLE_RELEASE_IPO "Enable IPO for release Python builds when supported" ON)
option(NORMALIZE_UK_CPP_REQUIRE_CLANG "Require a Clang-family C++ compiler" OFF)
option(NORMALIZE_UK_CPP_BUILD_FUZZER "Build the libFuzzer normalization harness" OFF)
option(NORMALIZE_UK_CPP_FETCH_CTRE "Fetch CTRE when it is not already available" ON)
option(NORMALIZE_UK_CPP_FETCH_PYBIND11 "Fetch pybind11 when it is not already available" ON)

if(NORMALIZE_UK_CPP_REQUIRE_CLANG AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "Clang is required, but CMake selected ${CMAKE_CXX_COMPILER_ID}")
endif()

find_package(ctre CONFIG QUIET)

if(NOT ctre_FOUND)
    if(NOT NORMALIZE_UK_CPP_FETCH_CTRE)
        message(FATAL_ERROR "ctre was not found. Install ctre or enable NORMALIZE_UK_CPP_FETCH_CTRE.")
    endif()
    include(FetchContent)
    FetchContent_Declare(
        ctre
        GIT_REPOSITORY https://github.com/hanickadot/compile-time-regular-expressions.git
        GIT_TAG dab09336d6a221299159a5ddc76cdcf6967ba9dd
    )
    FetchContent_GetProperties(ctre)
    if(NOT ctre_POPULATED)
        FetchContent_Populate(ctre)
    endif()
    if(NOT TARGET ctre::ctre)
        add_library(ctre INTERFACE)
        add_library(ctre::ctre ALIAS ctre)
        target_include_directories(ctre INTERFACE $<BUILD_INTERFACE:${ctre_SOURCE_DIR}/include>)
    endif()
endif()

# Regenerate the lexicon header from data/lexicons/*.tsv when Python is available.
# A pre-generated copy is committed so builds work without Python.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    file(GLOB NORMALIZE_UK_CPP_LEXICON_TSVS ${CMAKE_CURRENT_SOURCE_DIR}/data/lexicons/*.tsv)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/src/uktextnorm/generated/uktextnorm_lexicons.hpp
        COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gen_lexicon.py
                ${CMAKE_CURRENT_SOURCE_DIR}/data/lexicons
                ${CMAKE_CURRENT_SOURCE_DIR}/src/uktextnorm/generated/uktextnorm_lexicons.hpp
        DEPENDS ${NORMALIZE_UK_CPP_LEXICON_TSVS} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gen_lexicon.py
        COMMENT "Generating uktextnorm lexicons from data/lexicons"
        VERBATIM
    )
endif()

add_library(normalize_uk_cpp
    src/rozpodil/rozpodil.cpp
    src/rozpodil/rozpodil_tokenize.cpp
    src/uktextnorm/generated/uktextnorm_lexicons.hpp
    src/uktextnorm/internal.cpp
    src/uktextnorm/internal_readers.cpp
    src/uktextnorm/internal_validation.cpp
    src/uktextnorm/numbers.cpp
    src/uktextnorm/numeric_helpers.cpp
    src/uktextnorm/passes_lexical.cpp
    src/uktextnorm/passes_lexical_abbreviations.cpp
    src/uktextnorm/passes_lexical_alphanumeric.cpp
    src/uktextnorm/passes_numeric.cpp
    src/uktextnorm/passes_numeric_ranges.cpp
    src/uktextnorm/passes_numeric_context.cpp
    src/uktextnorm/passes_numeric_units.cpp
    src/uktextnorm/passes_structural.cpp
    src/uktextnorm/passes_structural_identifiers.cpp
    src/uktextnorm/pipeline.cpp
    src/uktextnorm/uncertainty.cpp
    src/uktextnorm/vocabulary.cpp
)
add_library(normalize_uk_cpp::normalize_uk_cpp ALIAS normalize_uk_cpp)
set_target_properties(normalize_uk_cpp PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_compile_features(normalize_uk_cpp PUBLIC cxx_std_23)
target_link_libraries(normalize_uk_cpp PRIVATE $<BUILD_INTERFACE:ctre::ctre>)
target_include_directories(normalize_uk_cpp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(MSVC)
    target_compile_options(normalize_uk_cpp PRIVATE /W4)
else()
    target_compile_options(normalize_uk_cpp PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_custom_target(format
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Format.cmake
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Formatting C++ sources with clang-format"
    VERBATIM
)

if(NORMALIZE_UK_CPP_BUILD_TOOLS)
    add_executable(uktextnorm tools/uktextnorm_cli.cpp)
    target_link_libraries(uktextnorm PRIVATE normalize_uk_cpp::normalize_uk_cpp)
    target_compile_definitions(uktextnorm PRIVATE NORMALIZE_UK_CPP_VERSION="${PROJECT_VERSION}")

    add_executable(rozpodil tools/rozpodil_cli.cpp)
    target_link_libraries(rozpodil PRIVATE normalize_uk_cpp::normalize_uk_cpp)

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include/ruaccent/ruaccent.hpp")
        add_executable(ruaccent tools/ruaccent_cli.cpp)
        target_link_libraries(ruaccent PRIVATE normalize_uk_cpp::normalize_uk_cpp)
    endif()
endif()

if(NORMALIZE_UK_CPP_BUILD_TESTS)
    enable_testing()

    add_executable(uktextnorm_tests tests/uktextnorm_tests.cpp)
    target_link_libraries(uktextnorm_tests PRIVATE normalize_uk_cpp::normalize_uk_cpp)
    add_test(NAME uktextnorm_tests COMMAND uktextnorm_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/uktextnorm_golden.tsv
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/uktextnorm_domain_golden.tsv
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/uktextnorm_robust_golden.tsv
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/uktextnorm_morphology_golden.tsv
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/uktextnorm_coverage_golden.tsv
    )

    add_executable(uktextnorm_robustness_tests tests/uktextnorm_robustness_tests.cpp)
    target_link_libraries(uktextnorm_robustness_tests PRIVATE normalize_uk_cpp::normalize_uk_cpp)
    add_test(NAME uktextnorm_robustness_tests COMMAND uktextnorm_robustness_tests)

    add_executable(uktextnorm_sentence_tests tests/uktextnorm_sentence_tests.cpp)
    target_link_libraries(uktextnorm_sentence_tests PRIVATE normalize_uk_cpp::normalize_uk_cpp)
    add_test(NAME uktextnorm_sentence_tests COMMAND uktextnorm_sentence_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/uktextnorm_sentence_golden.tsv
    )

    add_executable(rozpodil_tests tests/rozpodil_tests.cpp)
    target_link_libraries(rozpodil_tests PRIVATE normalize_uk_cpp::normalize_uk_cpp)
    add_test(NAME rozpodil_tests COMMAND rozpodil_tests)

    if(TARGET uktextnorm)
        add_test(NAME uktextnorm_version COMMAND uktextnorm --version)
        set_tests_properties(uktextnorm_version PROPERTIES
            PASS_REGULAR_EXPRESSION "uktextnorm ${PROJECT_VERSION}"
        )
        add_test(NAME uktextnorm_cli_temperature
            COMMAND uktextnorm --preset tts-friendly "5–7 °C"
        )
        set_tests_properties(uktextnorm_cli_temperature PROPERTIES
            PASS_REGULAR_EXPRESSION "від п'яти до семи градусів Цельсія"
        )
        add_test(NAME uktextnorm_cli_uncertainty
            COMMAND uktextnorm --uncertain-json "Час 99:30"
        )
        set_tests_properties(uktextnorm_cli_uncertainty PROPERTIES
            PASS_REGULAR_EXPRESSION "invalid clock time"
        )
        add_test(NAME uktextnorm_cli_colon_policy
            COMMAND uktextnorm --colon-style ratio "10:30"
        )
        set_tests_properties(uktextnorm_cli_colon_policy PROPERTIES
            PASS_REGULAR_EXPRESSION "десять до тридцяти"
        )
        add_test(NAME uktextnorm_cli_date_policy
            COMMAND uktextnorm --date-order mdy "03/04/2026"
        )
        set_tests_properties(uktextnorm_cli_date_policy PROPERTIES
            PASS_REGULAR_EXPRESSION "четверте березня"
        )
        add_test(NAME uktextnorm_cli_vocabulary
            COMMAND uktextnorm --vocabulary ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/custom_vocabulary.tsv "Google і Acme"
        )
        set_tests_properties(uktextnorm_cli_vocabulary PROPERTIES
            PASS_REGULAR_EXPRESSION "гуголь і акме"
        )
    endif()
endif()

add_executable(uktextnorm_benchmark EXCLUDE_FROM_ALL benchmarks/uktextnorm_benchmark.cpp)
target_link_libraries(uktextnorm_benchmark PRIVATE normalize_uk_cpp::normalize_uk_cpp)

if(NORMALIZE_UK_CPP_BUILD_FUZZER)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "NORMALIZE_UK_CPP_BUILD_FUZZER requires Clang")
    endif()
    target_compile_options(normalize_uk_cpp PRIVATE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
    add_executable(uktextnorm_fuzzer fuzz/uktextnorm_fuzzer.cpp)
    target_link_libraries(uktextnorm_fuzzer PRIVATE normalize_uk_cpp::normalize_uk_cpp)
    target_compile_options(uktextnorm_fuzzer PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(uktextnorm_fuzzer PRIVATE -fsanitize=fuzzer,address,undefined)
endif()

if(NORMALIZE_UK_CPP_BUILD_PYTHON)
    find_package(Python 3.10 REQUIRED COMPONENTS Interpreter Development.Module)
    set(PYBIND11_FINDPYTHON ON)
    find_package(pybind11 3.0 CONFIG QUIET)

    if(NOT pybind11_FOUND)
        if(NOT NORMALIZE_UK_CPP_FETCH_PYBIND11)
            message(FATAL_ERROR "pybind11 3.0 or newer was not found. Install pybind11 or enable NORMALIZE_UK_CPP_FETCH_PYBIND11.")
        endif()
        include(FetchContent)
        FetchContent_Declare(
            pybind11
            GIT_REPOSITORY https://github.com/pybind/pybind11.git
            GIT_TAG v3.0.1
        )
        FetchContent_MakeAvailable(pybind11)
    endif()

    pybind11_add_module(normalize_uk_python MODULE bindings/python/normalize_uk.cpp)
    target_link_libraries(normalize_uk_python PRIVATE normalize_uk_cpp::normalize_uk_cpp)
    target_compile_features(normalize_uk_python PRIVATE cxx_std_23)
    if(NORMALIZE_UK_CPP_ENABLE_RELEASE_IPO)
        include(CheckIPOSupported)
        check_ipo_supported(RESULT normalize_uk_cpp_ipo_supported OUTPUT normalize_uk_cpp_ipo_output LANGUAGES CXX)
        if(normalize_uk_cpp_ipo_supported)
            set_property(TARGET normalize_uk_cpp PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
        else()
            message(STATUS "Release IPO is unavailable: ${normalize_uk_cpp_ipo_output}")
        endif()
    endif()
    # Keep each multi-config build in an importable package directory. Without
    # the explicit $<CONFIG> component, Visual Studio appends the configuration
    # below normalize_uk and leaves the package root without __init__.py.
    set(normalize_uk_python_package_dir "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/normalize_uk")
    set_target_properties(normalize_uk_python PROPERTIES
        OUTPUT_NAME _normalize_uk
        LIBRARY_OUTPUT_DIRECTORY "${normalize_uk_python_package_dir}"
        RUNTIME_OUTPUT_DIRECTORY "${normalize_uk_python_package_dir}"
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
    add_custom_command(TARGET normalize_uk_python POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/bindings/python/normalize_uk/__init__.py
            ${CMAKE_CURRENT_SOURCE_DIR}/bindings/python/normalize_uk/__init__.pyi
            ${CMAKE_CURRENT_SOURCE_DIR}/bindings/python/normalize_uk/_normalize_uk.pyi
            ${CMAKE_CURRENT_SOURCE_DIR}/bindings/python/normalize_uk/py.typed
            $<TARGET_FILE_DIR:normalize_uk_python>
        VERBATIM
    )

    if(NORMALIZE_UK_CPP_BUILD_TESTS)
        add_test(NAME normalize_uk_python_tests
            COMMAND Python::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/tests/python_binding_tests.py
        )
        set_tests_properties(normalize_uk_python_tests PROPERTIES
            ENVIRONMENT "PYTHONPATH=$<TARGET_FILE_DIR:normalize_uk_python>/.."
        )
    endif()

    install(TARGETS normalize_uk_python
        LIBRARY DESTINATION normalize_uk COMPONENT python
        RUNTIME DESTINATION normalize_uk COMPONENT python
    )
    install(FILES
        bindings/python/normalize_uk/__init__.py
        bindings/python/normalize_uk/__init__.pyi
        bindings/python/normalize_uk/_normalize_uk.pyi
        bindings/python/normalize_uk/py.typed
        DESTINATION normalize_uk
        COMPONENT python
    )
endif()

install(TARGETS normalize_uk_cpp
    EXPORT normalize_uk_cppTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT cpp
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT cpp
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT cpp
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT cpp)
install(EXPORT normalize_uk_cppTargets
    NAMESPACE normalize_uk_cpp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/normalize_uk_cpp
    COMPONENT cpp
)
