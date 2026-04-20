cmake_minimum_required(VERSION 3.20)
project(bzip2pb VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(BZip2 REQUIRED)
find_package(Threads REQUIRED)

# Platform string embedded in the binary via version.h
if(WIN32)
    set(BZIP2PB_PLATFORM "Windows")
elseif(APPLE)
    set(BZIP2PB_PLATFORM "macOS")
else()
    set(BZIP2PB_PLATFORM "Linux")
endif()

# Architecture string — CMAKE_GENERATOR_PLATFORM covers MSVC cross-compiles (e.g. ARM64)
if(CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
    set(BZIP2PB_ARCH "ARM64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|arm64|aarch64")
    set(BZIP2PB_ARCH "ARM64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
    set(BZIP2PB_ARCH "x64")
else()
    set(BZIP2PB_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
endif()

# Generate version header into the build tree so the source tree stays clean
configure_file(src/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/src/version.h @ONLY)

# macOS universal binary — must be set before add_executable
if(APPLE)
    option(BZIP2PB_UNIVERSAL "Build universal binary (macOS x86_64 + arm64)" OFF)
    if(BZIP2PB_UNIVERSAL)
        set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
    endif()
endif()

# ---------------------------------------------------------------------------
# bzip2pb_lib — all core sources; shared by the executable and the test suite
# ---------------------------------------------------------------------------
add_library(bzip2pb_lib STATIC
    src/cli.cpp
    src/compress.cpp
    src/decompress.cpp
    src/thread_pool.cpp
    src/bitscanner.cpp
    src/bitstream.cpp
)

target_include_directories(bzip2pb_lib PUBLIC
    src
    ${CMAKE_CURRENT_BINARY_DIR}/src
)
target_link_libraries(bzip2pb_lib PUBLIC BZip2::BZip2 Threads::Threads)

if(MSVC)
    target_compile_options(bzip2pb_lib PRIVATE /W4 /WX /O2 /EHsc)
    if(CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
        target_compile_definitions(bzip2pb_lib PRIVATE BZIP2PB_ARCH_ARM64)
    endif()
elseif(APPLE)
    target_compile_options(bzip2pb_lib PRIVATE -Wall -Wextra -Werror -O2)
else()
    target_compile_options(bzip2pb_lib PRIVATE -Wall -Wextra -Werror -O2)
    option(BZIP2PB_STATIC "Link statically (Linux portable binary)" OFF)
    if(BZIP2PB_STATIC)
        target_link_options(bzip2pb_lib PRIVATE -static-libgcc -static-libstdc++)
    endif()
endif()

# ---------------------------------------------------------------------------
# bzip2pb — the main executable (only main.cpp on top of the library)
# ---------------------------------------------------------------------------
add_executable(bzip2pb src/main.cpp)
target_link_libraries(bzip2pb PRIVATE bzip2pb_lib)

if(MSVC)
    target_compile_options(bzip2pb PRIVATE /W4 /WX /O2 /EHsc)
elseif(APPLE)
    target_compile_options(bzip2pb PRIVATE -Wall -Wextra -Werror -O2)
else()
    target_compile_options(bzip2pb PRIVATE -Wall -Wextra -Werror -O2)
endif()

install(TARGETS bzip2pb
    RUNTIME DESTINATION bin
    BUNDLE  DESTINATION bin
)

# ---------------------------------------------------------------------------
# Tests (skipped on ARM64 cross-compile and when Catch2 is absent)
# ---------------------------------------------------------------------------
# catch_discover_tests runs the test binary at post-build time to list tests.
# An ARM64 binary cannot execute on the x64 CI host, so exclude the test
# target entirely for that configuration.
find_package(Catch2 3 QUIET)
if(Catch2_FOUND AND NOT (MSVC AND CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64"))
    include(CTest)
    include(Catch)
    enable_testing()

    add_executable(bzip2pb_tests
        tests/unit/test_bitscanner.cpp
        tests/unit/test_compress.cpp
        tests/unit/test_decompress.cpp
        tests/unit/test_cli.cpp
        tests/integration/test_roundtrip.cpp
        tests/integration/test_compatibility.cpp
        tests/integration/test_edge_cases.cpp
    )
    target_link_libraries(bzip2pb_tests PRIVATE
        bzip2pb_lib
        Catch2::Catch2WithMain
    )
    target_include_directories(bzip2pb_tests PRIVATE tests)

    catch_discover_tests(bzip2pb_tests)

    add_custom_target(check
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -C $<CONFIG>
        DEPENDS bzip2pb_tests
    )
elseif(NOT Catch2_FOUND)
    message(STATUS "Catch2 not found — test target skipped")
else()
    message(STATUS "Catch2 found — test target skipped (ARM64 cross-compile)")
endif()

# ---------------------------------------------------------------------------
# Benchmarks
# ---------------------------------------------------------------------------
add_executable(bzip2pb_bench benchmarks/bench.cpp)
target_link_libraries(bzip2pb_bench PRIVATE bzip2pb_lib)
# benchmarks/ for bench.h; tests/ for testdata.h; src/ inherited from bzip2pb_lib
target_include_directories(bzip2pb_bench PRIVATE benchmarks tests)

add_custom_target(benchmark
    COMMAND $<TARGET_FILE:bzip2pb_bench> --csv ${CMAKE_BINARY_DIR}/benchmark_results.csv
    DEPENDS bzip2pb_bench
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running bzip2pb benchmarks (this may take several minutes)"
)
