#include "cli.h"
#include "version.h"
#include <cstdlib>
#include <cstring>
#include <iostream>

static void print_usage(const char* prog) {
    std::cerr <<
        "Usage: " << prog << " [options] [file...]\n"
        "\n"
        "Options:\n"
        "  -z, --compress      Force compress (default)\n"
        "  -d, --decompress    Decompress\n"
        "  -t, --test          Test integrity; no output\n"
        "  -c, --stdout        Write to stdout, keep input\n"
        "  -k, --keep          Keep input file(s)\n"
        "  -f, --force         Overwrite existing output\n"
        "  -n N, --parallel N  Worker threads (default: all cores)\n"
        "  -1 .. -9            Block size 1-9 x 100 KB (default: 9)\n"
        "  -v, --verbose       Verbose output\n"
        "  -q, --quiet         Suppress warnings\n"
        "  -L, --license       Print license and exit\n"
        "  -V, --version       Print version and exit\n"
        "      --help          Show this help\n"
        "\n"
        "With no files, reads stdin and writes stdout.\n"
        "Use '-' as a filename to read stdin / write stdout.\n"
        "Use '--' to end option processing.\n"
        "\n"
        "Output naming (decompress):\n"
        "  file.bz2  -> file\n"
        "  file.tbz2 -> file.tar\n"
        "  file.tbz  -> file.tar\n"
        "  other     -> other.out\n";
}

static void print_license() {
    std::cout <<
        "bzip2pb " BZIP2PB_VERSION_STRING "\n"
        "\n"
        "Copyright (C) 2024 Pavel Bezdek\n"
        "\n"
        "Permission is hereby granted, free of charge, to any person obtaining a\n"
        "copy of this software and associated documentation files (the \"Software\"),\n"
        "to deal in the Software without restriction, including without limitation\n"
        "the rights to use, copy, modify, merge, publish, distribute, sublicense,\n"
        "and/or sell copies of the Software, and to permit persons to whom the\n"
        "Software is furnished to do so, subject to the following conditions:\n"
        "\n"
        "The above copyright notice and this permission notice shall be included in\n"
        "all copies or substantial portions of the Software.\n"
        "\n"
        "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS\n"
        "OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
        "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
        "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
        "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING\n"
        "FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER\n"
        "DEALINGS IN THE SOFTWARE.\n";
}

static unsigned parse_uint_arg(const char* s, const char* flag) {
    char* end = nullptr;
    long v = std::strtol(s, &end, 10);
    if (!end || *end != '\0' || v <= 0) {
        std::cerr << "bzip2pb: invalid argument for " << flag << ": " << s << "\n";
        std::exit(2);
    }
    return static_cast<unsigned>(v);
}

static int parse_block_arg(const char* s) {
    char* end = nullptr;
    long v = std::strtol(s, &end, 10);
    if (!end || *end != '\0' || v < 1 || v > 9) {
        std::cerr << "bzip2pb: block size must be 1-9, got: " << s << "\n";
        std::exit(2);
    }
    return static_cast<int>(v);
}

ParsedArgs parse_args(int argc, char* argv[]) {
    ParsedArgs result;
    bool end_of_options = false;

    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];

        if (end_of_options) {
            result.files.push_back(a);
            continue;
        }

        // '-' alone means stdin/stdout filename
        if (std::strcmp(a, "-") == 0) {
            result.files.push_back("-");
            continue;
        }

        // '--' ends option processing
        if (std::strcmp(a, "--") == 0) {
            end_of_options = true;
            continue;
        }

        // Block-size shorthand: -1 .. -9
        if (a[0] == '-' && a[1] >= '1' && a[1] <= '9' && a[2] == '\0') {
            result.opts.block_level = a[1] - '0';
            continue;
        }

        // Long options
        if (a[0] == '-' && a[1] == '-') {
            if      (std::strcmp(a, "--compress")   == 0) { result.opts.decompress = false; result.mode_explicit = true; }
            else if (std::strcmp(a, "--decompress") == 0) { result.opts.decompress = true;  result.mode_explicit = true; }
            else if (std::strcmp(a, "--test")       == 0) { result.opts.test = true; result.opts.decompress = true; result.mode_explicit = true; }
            else if (std::strcmp(a, "--stdout")     == 0) { result.opts.to_stdout = true; result.opts.keep = true; }
            else if (std::strcmp(a, "--keep")       == 0) result.opts.keep    = true;
            else if (std::strcmp(a, "--force")      == 0) result.opts.force   = true;
            else if (std::strcmp(a, "--verbose")    == 0) result.opts.verbose = true;
            else if (std::strcmp(a, "--quiet")      == 0) result.opts.quiet   = true;
            else if (std::strcmp(a, "--license")    == 0) { print_license(); std::exit(0); }
            else if (std::strcmp(a, "--version")    == 0) {
                std::cout << "bzip2pb " BZIP2PB_VERSION_STRING
                          << " (" BZIP2PB_PLATFORM " " BZIP2PB_ARCH ")\n";
                std::exit(0);
            }
            else if (std::strcmp(a, "--help") == 0) { print_usage(argv[0]); std::exit(0); }
            else if (std::strcmp(a, "--parallel") == 0 && i + 1 < argc)
                result.opts.num_threads = parse_uint_arg(argv[++i], "--parallel");
            else if (std::strcmp(a, "--block") == 0 && i + 1 < argc)
                result.opts.block_level = parse_block_arg(argv[++i]);
            else {
                std::cerr << "bzip2pb: unknown option: " << a << "\n";
                std::exit(2);
            }
            continue;
        }

        // Short options (may be clustered: -kv, but -n4 or -n 4)
        if (a[0] == '-' && a[1] != '\0') {
            for (int j = 1; a[j]; ++j) {
                switch (a[j]) {
                case 'z': result.opts.decompress = false; result.mode_explicit = true; break;
                case 'd': result.opts.decompress = true;  result.mode_explicit = true; break;
                case 't': result.opts.test = true; result.opts.decompress = true; result.mode_explicit = true; break;
                case 'c': result.opts.to_stdout = true; result.opts.keep = true; break;
                case 'k': result.opts.keep    = true; break;
                case 'f': result.opts.force   = true; break;
                case 'v': result.opts.verbose = true; break;
                case 'q': result.opts.quiet   = true; break;
                case 'L': print_license(); std::exit(0);
                case 'V':
                    std::cout << "bzip2pb " BZIP2PB_VERSION_STRING
                              << " (" BZIP2PB_PLATFORM " " BZIP2PB_ARCH ")\n";
                    std::exit(0);
                case 'h': print_usage(argv[0]); std::exit(0);
                case 'n': // lbzip2-style thread count
                case 'p': // hidden backwards-compat alias for -n
                {
                    const char flag[3] = { '-', a[j], '\0' };
                    if (a[j + 1]) {
                        result.opts.num_threads = parse_uint_arg(a + j + 1, flag);
                        j += static_cast<int>(std::strlen(a + j + 1));
                    } else if (i + 1 < argc) {
                        result.opts.num_threads = parse_uint_arg(argv[++i], flag);
                    } else {
                        std::cerr << "bzip2pb: option " << flag << " requires an argument\n";
                        std::exit(2);
                    }
                    break;
                }
                case 'b':
                    if (a[j + 1]) {
                        result.opts.block_level = parse_block_arg(a + j + 1);
                        j += static_cast<int>(std::strlen(a + j + 1));
                    } else if (i + 1 < argc) {
                        result.opts.block_level = parse_block_arg(argv[++i]);
                    } else {
                        std::cerr << "bzip2pb: option -b requires an argument\n";
                        std::exit(2);
                    }
                    break;
                default:
                    std::cerr << "bzip2pb: unknown option: -" << a[j] << "\n";
                    std::exit(2);
                }
            }
            continue;
        }

        result.files.push_back(a);
    }

    if (result.opts.block_level < 1 || result.opts.block_level > 9) {
        std::cerr << "bzip2pb: block size must be 1-9\n";
        std::exit(2);
    }

    return result;
}
