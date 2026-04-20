#include "common.h"
#include "compress.h"
#include "decompress.h"
#include "version.h"
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static void usage(const char* prog) {
    std::cerr <<
        "Usage: " << prog << " [options] [file...]\n"
        "\n"
        "Options:\n"
        "  -c, --compress      Compress (default)\n"
        "  -d, --decompress    Decompress\n"
        "  -k, --keep          Keep input file(s)\n"
        "  -f, --force         Overwrite existing output\n"
        "  -p N, --threads N   Worker threads (default: all cores)\n"
        "  -b N, --block N     Block size 1-9 × 100 KB (default: 9 = 900 KB)\n"
        "  -1 .. -9            Shorthand for --block N\n"
        "  -v, --verbose       Print progress\n"
        "  -V, --version       Print version and exit\n"
        "  -h, --help          Show this help\n"
        "\n"
        "With no files: reads stdin, writes stdout.\n"
        "Compressed files get a .bz2 extension; decompression removes it.\n";
}

// Returns true if s ends with suffix.
static bool ends_with(const std::string& s, const char* suffix) {
    size_t slen = std::strlen(suffix);
    return s.size() >= slen && s.compare(s.size() - slen, slen, suffix) == 0;
}

int main(int argc, char* argv[]) {
    Options opts;
    std::vector<std::string> files;
    bool mode_explicit = false;

    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];

        // Block-size shorthand: -1 .. -9
        if (a[0] == '-' && a[1] >= '1' && a[1] <= '9' && a[2] == '\0') {
            opts.block_level = a[1] - '0';
            continue;
        }

        // Long options
        if (std::strncmp(a, "--", 2) == 0) {
            if      (std::strcmp(a, "--compress")   == 0) { opts.decompress = false; mode_explicit = true; }
            else if (std::strcmp(a, "--decompress") == 0) { opts.decompress = true;  mode_explicit = true; }
            else if (std::strcmp(a, "--keep")       == 0) opts.keep    = true;
            else if (std::strcmp(a, "--force")      == 0) opts.force   = true;
            else if (std::strcmp(a, "--verbose")    == 0) opts.verbose = true;
            else if (std::strcmp(a, "--version")    == 0) {
                std::cout << "bzip2pb " BZIP2PB_VERSION_STRING
                          << " (" BZIP2PB_PLATFORM " " BZIP2PB_ARCH ")\n";
                return 0;
            }
            else if (std::strcmp(a, "--help")       == 0) { usage(argv[0]); return 0; }
            else if (std::strcmp(a, "--threads") == 0 && i + 1 < argc)
                opts.num_threads = static_cast<unsigned>(std::stoul(argv[++i]));
            else if (std::strcmp(a, "--block") == 0 && i + 1 < argc)
                opts.block_level = std::stoi(argv[++i]);
            else { std::cerr << "Unknown option: " << a << "\n"; return 1; }
            continue;
        }

        // Short options
        if (a[0] == '-' && a[1] != '\0') {
            for (int j = 1; a[j]; ++j) {
                switch (a[j]) {
                case 'c': opts.decompress = false; mode_explicit = true; break;
                case 'd': opts.decompress = true;  mode_explicit = true; break;
                case 'k': opts.keep    = true; break;
                case 'f': opts.force   = true; break;
                case 'v': opts.verbose = true; break;
                case 'V':
                    std::cout << "bzip2pb " BZIP2PB_VERSION_STRING
                              << " (" BZIP2PB_PLATFORM " " BZIP2PB_ARCH ")\n";
                    return 0;
                case 'h': usage(argv[0]); return 0;
                case 'p':
                    if (a[j + 1]) {
                        opts.num_threads = static_cast<unsigned>(std::stoul(a + j + 1));
                        j += static_cast<int>(std::strlen(a + j + 1));
                    } else if (i + 1 < argc) {
                        opts.num_threads = static_cast<unsigned>(std::stoul(argv[++i]));
                    }
                    break;
                case 'b':
                    if (a[j + 1]) {
                        opts.block_level = std::stoi(a + j + 1);
                        j += static_cast<int>(std::strlen(a + j + 1));
                    } else if (i + 1 < argc) {
                        opts.block_level = std::stoi(argv[++i]);
                    }
                    break;
                default:
                    std::cerr << "Unknown option: -" << a[j] << "\n";
                    return 1;
                }
            }
            continue;
        }

        files.push_back(a);
    }

    if (opts.block_level < 1 || opts.block_level > 9) {
        std::cerr << "Block size must be 1-9\n";
        return 1;
    }

    // Infer mode from file extensions when not set explicitly
    if (!mode_explicit && !files.empty()) {
        bool all_bz2 = true;
        for (auto& f : files)
            if (!ends_with(f, ".bz2")) { all_bz2 = false; break; }
        if (all_bz2) opts.decompress = true;
    }

    // stdin/stdout mode
    if (files.empty()) {
        try {
            if (opts.decompress) decompress_stdin(opts);
            else                 compress_stdin(opts);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }
        return 0;
    }

    int ret = 0;
    for (const auto& input : files) {
        try {
            std::string output;
            if (opts.decompress) {
                output = ends_with(input, ".bz2")
                       ? input.substr(0, input.size() - 4)
                       : input + ".out";
            } else {
                output = input + ".bz2";
            }

            if (!opts.force && fs::exists(output)) {
                std::cerr << "Output already exists: " << output
                          << "  (use -f to overwrite)\n";
                ret = 1;
                continue;
            }

            if (opts.verbose)
                std::cerr << (opts.decompress ? "Decompressing " : "Compressing ")
                          << input << " -> " << output << "\n";

            if (opts.decompress)
                decompress_file(input, output, opts);
            else
                compress_file(input, output, opts);

            if (!opts.keep)
                fs::remove(input);

        } catch (const std::exception& e) {
            std::cerr << "Error [" << input << "]: " << e.what() << "\n";
            ret = 1;
        }
    }

    return ret;
}
