cmake_minimum_required(VERSION 3.20)

project(
    normalize_uk_cpp
    VERSION 0.1.0
    DESCRIPTION "Ukrainian text normalization and tokenization utilities"
    LANGUAGES CXX
)

include(GNUInstallDirs)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(NORMALIZE_UK_CPP_DEFAULT_BUILD_TESTS ON)
else()
    set(NORMALIZE_UK_CPP_DEFAULT_BUILD_TESTS OFF)
endif()

option(NORMALIZE_UK_CPP_BUILD_TOOLS "Build command-line tools" ON)
option(NORMALIZE_UK_CPP_BUILD_TESTS "Build C++ tests" ${NORMALIZE_UK_CPP_DEFAULT_BUILD_TESTS})
option(NORMALIZE_UK_CPP_BUILD_PYTHON "Build Python 3.10+ bindings" OFF)
option(NORMALIZE_UK_CPP_FETCH_PYBIND11 "Fetch pybind11 when it is not already available" ON)

add_library(normalize_uk_cpp
    src/rozpodil/rozpodil.cpp
    src/uktextnorm/uktextnorm.cpp
)
add_library(normalize_uk_cpp::normalize_uk_cpp ALIAS normalize_uk_cpp)
set_target_properties(normalize_uk_cpp PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_compile_features(normalize_uk_cpp PUBLIC cxx_std_20)
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
    add_test(NAME uktextnorm_tests COMMAND uktextnorm_tests ${CMAKE_CURRENT_SOURCE_DIR})

    add_executable(uktextnorm_robustness_tests tests/uktextnorm_robustness_tests.cpp)
    target_link_libraries(uktextnorm_robustness_tests PRIVATE normalize_uk_cpp::normalize_uk_cpp)
    add_test(NAME uktextnorm_robustness_tests COMMAND uktextnorm_robustness_tests)

    add_executable(rozpodil_tests tests/rozpodil_tests.cpp)
    target_link_libraries(rozpodil_tests PRIVATE normalize_uk_cpp::normalize_uk_cpp)
    add_test(NAME rozpodil_tests COMMAND rozpodil_tests)
endif()

add_executable(uktextnorm_benchmark EXCLUDE_FROM_ALL benchmarks/uktextnorm_benchmark.cpp)
target_link_libraries(uktextnorm_benchmark PRIVATE normalize_uk_cpp::normalize_uk_cpp)

if(NORMALIZE_UK_CPP_BUILD_PYTHON)
    find_package(Python 3.10 REQUIRED COMPONENTS Interpreter Development.Module)
    find_package(pybind11 CONFIG QUIET)

    if(NOT pybind11_FOUND)
        if(NOT NORMALIZE_UK_CPP_FETCH_PYBIND11)
            message(FATAL_ERROR "pybind11 was not found. Install pybind11 or enable NORMALIZE_UK_CPP_FETCH_PYBIND11.")
        endif()
        include(FetchContent)
        FetchContent_Declare(
            pybind11
            GIT_REPOSITORY https://github.com/pybind/pybind11.git
            GIT_TAG v2.13.6
        )
        FetchContent_MakeAvailable(pybind11)
    endif()

    pybind11_add_module(normalize_uk_python MODULE bindings/python/normalize_uk.cpp)
    target_link_libraries(normalize_uk_python PRIVATE normalize_uk_cpp::normalize_uk_cpp)
    target_compile_features(normalize_uk_python PRIVATE cxx_std_20)
    set_target_properties(normalize_uk_python PROPERTIES
        OUTPUT_NAME _normalize_uk
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/normalize_uk
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/normalize_uk
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
    add_custom_command(TARGET normalize_uk_python POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/bindings/python/normalize_uk/__init__.py
            ${CMAKE_CURRENT_SOURCE_DIR}/bindings/python/normalize_uk/__init__.pyi
            ${CMAKE_CURRENT_SOURCE_DIR}/bindings/python/normalize_uk/py.typed
            $<TARGET_FILE_DIR:normalize_uk_python>
        VERBATIM
    )

    install(TARGETS normalize_uk_python
        LIBRARY DESTINATION normalize_uk
        RUNTIME DESTINATION normalize_uk
    )
    install(FILES
        bindings/python/normalize_uk/__init__.py
        bindings/python/normalize_uk/__init__.pyi
        bindings/python/normalize_uk/py.typed
        DESTINATION normalize_uk
    )
endif()

install(TARGETS normalize_uk_cpp
    EXPORT normalize_uk_cppTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT normalize_uk_cppTargets
    NAMESPACE normalize_uk_cpp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/normalize_uk_cpp
)
