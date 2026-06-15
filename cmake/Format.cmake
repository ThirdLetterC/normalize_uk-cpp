cmake_path(SET PROJECT_ROOT NORMALIZE "${CMAKE_CURRENT_LIST_DIR}/..")

find_program(CLANG_FORMAT_EXE
    NAMES
        clang-format
        clang-format-22
        clang-format-21
        clang-format-20
        clang-format-19
        clang-format-18
        clang-format-17
        clang-format-16
        clang-format-15
        clang-format-14
)

if(NOT CLANG_FORMAT_EXE)
    message(FATAL_ERROR "clang-format was not found. Install clang-format and run the format target again.")
endif()

set(FORMAT_FILES
    benchmarks/uktextnorm_benchmark.cpp
    bindings/python/normalize_uk.cpp
    include/rozpodil/rozpodil.hpp
    include/uktextnorm/uktextnorm.hpp
    src/common/utf8.hpp
    src/rozpodil/rozpodil.cpp
    src/uktextnorm/uktextnorm.cpp
    tests/rozpodil_tests.cpp
    tests/uktextnorm_robustness_tests.cpp
    tests/uktextnorm_tests.cpp
    tools/rozpodil_cli.cpp
    tools/uktextnorm_cli.cpp
)

set(ABS_FORMAT_FILES)
foreach(FORMAT_FILE IN LISTS FORMAT_FILES)
    set(ABS_FORMAT_FILE "${PROJECT_ROOT}/${FORMAT_FILE}")
    if(NOT EXISTS "${ABS_FORMAT_FILE}")
        message(FATAL_ERROR "format input does not exist: ${FORMAT_FILE}")
    endif()
    list(APPEND ABS_FORMAT_FILES "${ABS_FORMAT_FILE}")
endforeach()

execute_process(
    COMMAND "${CLANG_FORMAT_EXE}" -i ${ABS_FORMAT_FILES}
    WORKING_DIRECTORY "${PROJECT_ROOT}"
    RESULT_VARIABLE FORMAT_RESULT
)

if(NOT FORMAT_RESULT EQUAL 0)
    message(FATAL_ERROR "clang-format failed with exit code ${FORMAT_RESULT}.")
endif()
