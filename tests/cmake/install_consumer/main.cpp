// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2026 Shannon Booth <shannon.ml.booth@gmail.com>

#include <patch/options.h>

#include <sstream>

int main()
{
    std::ostringstream output;
    Patch::show_version(output);
    return output.str().empty();
}
