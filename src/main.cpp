#include "cli.h"
#include "compress.h"
#include "decompress.h"
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static bool str_ends_with(const std::string& s, const char* suffix) {
    const size_t slen = std::strlen(suffix);
    return s.size() >= slen && s.compare(s.size() - slen, slen, suffix) == 0;
}

// Determine output filename for decompress (lbzip2 convention).
static std::string decompress_output_name(const std::string& input) {
    if (str_ends_with(input, ".bz2"))
        return input.substr(0, input.size() - 4);
    if (str_ends_with(input, ".tbz2"))
        return input.substr(0, input.size() - 5) + ".tar";
    if (str_ends_with(input, ".tbz"))
        return input.substr(0, input.size() - 4) + ".tar";
    return input + ".out";
}

// Print lbzip2-style verbose stats to stderr.
// in_bytes/out_bytes are always from the tool's perspective (in = input to tool).
// For compression: in=uncompressed, out=compressed.
// For decompression: in=compressed, out=uncompressed.
static void print_stats(const std::string& name,
                        uintmax_t in_bytes, uintmax_t out_bytes,
                        bool compressing) {
    // Express ratio as uncompressed:compressed regardless of direction.
    const uintmax_t raw = compressing ? in_bytes  : out_bytes;
    const uintmax_t cmp = compressing ? out_bytes : in_bytes;

    if (raw == 0) {
        std::cerr << "  " << name << ":  0 in, 0 out.\n";
        return;
    }
    const double dcmp  = cmp > 0 ? static_cast<double>(cmp) : 1.0;
    const double draw  = static_cast<double>(raw);
    const double ratio = draw / dcmp;
    const double bpb   = 8.0 * dcmp / draw;
    const double saved = 100.0 * (1.0 - dcmp / draw);

    std::cerr << std::fixed
              << "  " << name << ":  "
              << std::setprecision(3) << ratio << ":1,  "
              << std::setprecision(3) << bpb   << " bits/byte, "
              << std::setprecision(2) << saved << "% saved, "
              << in_bytes << " in, " << out_bytes << " out.\n";
}

int main(int argc, char* argv[]) {
    ParsedArgs parsed = parse_args(argc, argv);
    Options& opts     = parsed.opts;

    // Infer mode from extensions when not explicitly set.
    if (!parsed.mode_explicit && !parsed.files.empty()) {
        bool all_compressed = true;
        for (const auto& f : parsed.files) {
            if (f == "-") continue;
            if (!str_ends_with(f, ".bz2") &&
                !str_ends_with(f, ".tbz2") &&
                !str_ends_with(f, ".tbz"))
            { all_compressed = false; break; }
        }
        if (all_compressed) opts.decompress = true;
    }

    // stdin/stdout mode: no files given, or only '-' was given.
    if (parsed.files.empty() ||
        (parsed.files.size() == 1 && parsed.files[0] == "-"))
    {
        try {
            if (opts.test)       decompress_stdin_test(opts);
            else if (opts.decompress) decompress_stdin(opts);
            else                      compress_stdin(opts);
        } catch (const std::exception& e) {
            if (!opts.quiet) std::cerr << "bzip2pb: " << e.what() << "\n";
            return 2;
        }
        return 0;
    }

    int ret = 0;

    for (const auto& input : parsed.files) {
        // '-' inside a file list: treat as stdin/stdout.
        if (input == "-") {
            try {
                if (opts.test)            decompress_stdin_test(opts);
                else if (opts.decompress) decompress_stdin(opts);
                else                      compress_stdin(opts);
            } catch (const std::exception& e) {
                if (!opts.quiet)
                    std::cerr << "bzip2pb: (stdin): " << e.what() << "\n";
                ret = 2;
            }
            continue;
        }

        try {
            // -t test mode: decompress to null sink, no output file.
            if (opts.test) {
                if (opts.verbose && !opts.quiet)
                    std::cerr << "  " << input << ": testing...\n";
                decompress_file_test(input, opts);
                if (opts.verbose && !opts.quiet)
                    std::cerr << "  " << input << ": OK\n";
                continue;
            }

            // -c stdout mode: write to stdout, do not delete input.
            if (opts.to_stdout) {
                if (opts.decompress) decompress_file_stdout(input, opts);
                else                 compress_file_stdout(input, opts);
                continue;
            }

            // Normal file-to-file mode.
            const std::string output = opts.decompress
                ? decompress_output_name(input)
                : input + ".bz2";

            if (!opts.force && fs::exists(output)) {
                if (!opts.quiet)
                    std::cerr << "bzip2pb: " << output
                              << " already exists; use -f to overwrite.\n";
                ret = 1;
                continue;
            }

            const uintmax_t in_size =
                fs::exists(input) ? fs::file_size(input) : 0;

            if (opts.decompress) decompress_file(input, output, opts);
            else                 compress_file(input, output, opts);

            const uintmax_t out_size =
                fs::exists(output) ? fs::file_size(output) : 0;

            if (opts.verbose && !opts.quiet)
                print_stats(input, in_size, out_size, !opts.decompress);

            if (!opts.keep)
                fs::remove(input);

        } catch (const std::exception& e) {
            if (!opts.quiet)
                std::cerr << "bzip2pb: " << input << ": " << e.what() << "\n";
            ret = 2;
        }
    }

    return ret;
}
