#pragma once
#include "common.h"
#include <string>

void decompress_file(const std::string& input_path,
                     const std::string& output_path,
                     const Options& opts);

void decompress_stdin(const Options& opts);
