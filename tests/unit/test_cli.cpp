#include <catch2/catch_test_macros.hpp>
#include "cli.h"

// Build a non-const argv array from a list of string literals.
// parse_args requires char* (libc convention), not const char*.
#define MAKE_ARGV(...)                                         \
    const char* _argv_ptrs[] = { __VA_ARGS__, nullptr };       \
    int _argc = static_cast<int>(                              \
        sizeof(_argv_ptrs) / sizeof(_argv_ptrs[0]) - 1);      \
    char* argv_arr[sizeof(_argv_ptrs)/sizeof(_argv_ptrs[0])]; \
    for (int _i = 0; _i < _argc + 1; ++_i)                   \
        argv_arr[_i] = const_cast<char*>(_argv_ptrs[_i])

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("parse_args: default options") {
    MAKE_ARGV("bzip2pb", "file.txt");
    const auto parsed = parse_args(_argc, argv_arr);

    CHECK(parsed.opts.block_level == 9);
    CHECK(parsed.opts.num_threads == 0);
    CHECK_FALSE(parsed.opts.decompress);
    CHECK_FALSE(parsed.opts.to_stdout);
    CHECK_FALSE(parsed.opts.keep);
    CHECK_FALSE(parsed.opts.force);
    CHECK_FALSE(parsed.opts.verbose);
    CHECK_FALSE(parsed.opts.quiet);
    CHECK_FALSE(parsed.opts.test);
}

TEST_CASE("parse_args: -n sets thread count") {
    MAKE_ARGV("bzip2pb", "-n", "4", "file.txt");
    const auto parsed = parse_args(_argc, argv_arr);
    CHECK(parsed.opts.num_threads == 4u);
}

TEST_CASE("parse_args: --parallel sets thread count") {
    MAKE_ARGV("bzip2pb", "--parallel", "8", "file.txt");
    const auto parsed = parse_args(_argc, argv_arr);
    CHECK(parsed.opts.num_threads == 8u);
}

TEST_CASE("parse_args: -1 through -9 set block level") {
    for (int lvl = 1; lvl <= 9; ++lvl) {
        const std::string flag = "-" + std::to_string(lvl);
        MAKE_ARGV("bzip2pb", "x");
        // Rewrite first positional with level flag
        const char* av2[] = { "bzip2pb", flag.c_str(), "x", nullptr };
        char* argv2[] = {
            const_cast<char*>(av2[0]),
            const_cast<char*>(av2[1]),
            const_cast<char*>(av2[2]),
            nullptr
        };
        const auto parsed = parse_args(3, argv2);
        CHECK(parsed.opts.block_level == lvl);
    }
}

TEST_CASE("parse_args: -d sets decompress") {
    MAKE_ARGV("bzip2pb", "-d", "file.bz2");
    const auto parsed = parse_args(_argc, argv_arr);
    CHECK(parsed.opts.decompress);
}

TEST_CASE("parse_args: --decompress sets decompress") {
    MAKE_ARGV("bzip2pb", "--decompress", "file.bz2");
    const auto parsed = parse_args(_argc, argv_arr);
    CHECK(parsed.opts.decompress);
}

TEST_CASE("parse_args: -c sets to_stdout and keep") {
    MAKE_ARGV("bzip2pb", "-c", "file.txt");
    const auto parsed = parse_args(_argc, argv_arr);
    CHECK(parsed.opts.to_stdout);
    CHECK(parsed.opts.keep);
}

TEST_CASE("parse_args: -k sets keep") {
    MAKE_ARGV("bzip2pb", "-k", "file.txt");
    const auto parsed = parse_args(_argc, argv_arr);
    CHECK(parsed.opts.keep);
}

TEST_CASE("parse_args: -f sets force") {
    MAKE_ARGV("bzip2pb", "-f", "file.txt");
    const auto parsed = parse_args(_argc, argv_arr);
    CHECK(parsed.opts.force);
}

TEST_CASE("parse_args: -q sets quiet") {
    MAKE_ARGV("bzip2pb", "-q", "file.txt");
    const auto parsed = parse_args(_argc, argv_arr);
    CHECK(parsed.opts.quiet);
}

TEST_CASE("parse_args: -v sets verbose") {
    MAKE_ARGV("bzip2pb", "-v", "file.txt");
    const auto parsed = parse_args(_argc, argv_arr);
    CHECK(parsed.opts.verbose);
}

TEST_CASE("parse_args: -t sets test mode") {
    MAKE_ARGV("bzip2pb", "-t", "file.bz2");
    const auto parsed = parse_args(_argc, argv_arr);
    CHECK(parsed.opts.test);
}

TEST_CASE("parse_args: -- stops option parsing") {
    MAKE_ARGV("bzip2pb", "--", "-not-a-flag");
    const auto parsed = parse_args(_argc, argv_arr);
    REQUIRE(parsed.files.size() == 1u);
    CHECK(parsed.files[0] == "-not-a-flag");
}

TEST_CASE("parse_args: multiple files collected") {
    MAKE_ARGV("bzip2pb", "a.txt", "b.txt", "c.txt");
    const auto parsed = parse_args(_argc, argv_arr);
    REQUIRE(parsed.files.size() == 3u);
    CHECK(parsed.files[0] == "a.txt");
    CHECK(parsed.files[1] == "b.txt");
    CHECK(parsed.files[2] == "c.txt");
}

TEST_CASE("parse_args: - is treated as a filename (stdin/stdout)") {
    MAKE_ARGV("bzip2pb", "-");
    const auto parsed = parse_args(_argc, argv_arr);
    REQUIRE(parsed.files.size() == 1u);
    CHECK(parsed.files[0] == "-");
}

TEST_CASE("parse_args: -z sets explicit compress mode") {
    MAKE_ARGV("bzip2pb", "-z", "file.txt");
    const auto parsed = parse_args(_argc, argv_arr);
    CHECK_FALSE(parsed.opts.decompress);
    CHECK(parsed.mode_explicit);
}
