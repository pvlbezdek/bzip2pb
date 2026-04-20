#pragma once
#include "common.h"
#include <string>

void compress_file(const std::string& input_path,
                   const std::string& output_path,
                   const Options& opts);

// Compress input_path and write result to stdout.
void compress_file_stdout(const std::string& input_path, const Options& opts);

void compress_stdin(const Options& opts);
