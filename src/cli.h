#pragma once
#include "common.h"
#include <string>
#include <vector>

struct ParsedArgs {
    Options                  opts;
    std::vector<std::string> files;
    bool                     mode_explicit = false;
};

// Parses argv.  Prints usage/version/license and calls std::exit() where
// appropriate.  Exits with code 2 on unrecognised options or bad values.
ParsedArgs parse_args(int argc, char* argv[]);
