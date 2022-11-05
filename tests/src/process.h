// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#pragma once

#include <string>
#include <vector>

class Process {
public:
    Process(const char* cmd, const std::vector<const char*>& args, const std::string& stdin_data = {});

    const std::string& stdout_data() const { return m_stdout_data; }

    const std::string& stderr_data() const { return m_stderr_data; }

    int return_code() const { return m_return_code; }

private:
    std::string m_stdout_data;
    std::string m_stderr_data;
    int m_return_code;
};
