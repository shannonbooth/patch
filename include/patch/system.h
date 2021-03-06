// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#pragma once

#include <fstream>
#include <istream>
#include <string>
#include <vector>

namespace Patch {

enum class NewLine {
    LF,
    CRLF,
    None,
};

bool get_line(std::istream& is, std::string& line, NewLine* newline = nullptr);

std::string read_tty_until_enter();

void chdir(const std::string& path);

bool remove_empty_directory(const std::string& path);

} // namespace Patch
