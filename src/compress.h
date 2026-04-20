#pragma once
#include "common.h"
#include <iosfwd>
#include <string>

// Core stream API — used by tests and by the file/stdin wrappers below.
void compress_stream(std::istream& in, std::ostream& out, const Options& opts);

void compress_file(const std::string& input_path,
                   const std::string& output_path,
                   const Options& opts);

void compress_file_stdout(const std::string& input_path, const Options& opts);

void compress_stdin(const Options& opts);
