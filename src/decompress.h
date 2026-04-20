#pragma once
#include "common.h"
#include <iosfwd>
#include <string>

// Core stream API — used by tests and by the file/stdin wrappers below.
void decompress_stream(std::istream& in, std::ostream& out, const Options& opts);

void decompress_file(const std::string& input_path,
                     const std::string& output_path,
                     const Options& opts);

// Decompress input_path and write result to stdout.
void decompress_file_stdout(const std::string& input_path, const Options& opts);

// Decompress input_path to a null sink (integrity test, -t mode).
void decompress_file_test(const std::string& input_path, const Options& opts);

void decompress_stdin(const Options& opts);

// Decompress stdin to a null sink (integrity test, -t mode).
void decompress_stdin_test(const Options& opts);
