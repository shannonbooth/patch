// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#pragma once

#include <chrono>
#include <string>
#include <vector>

class PtySpawn {
public:
    PtySpawn(const char* cmd, const std::vector<const char*>& args, const std::string& stdin_data = {});

    const std::string& output() const { return m_output; }
    int return_code() const { return m_return_code; }

private:
    void write(const std::string& data);
    std::string read(std::chrono::milliseconds timeout = std::chrono::seconds(2));
    static void disable_echo(int fd);

    std::string m_output;
    int m_return_code;
    int m_master;
};
