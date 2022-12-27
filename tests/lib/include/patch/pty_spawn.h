// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#pragma once

#include <chrono>
#include <string>

class PtySpawn {
public:
    PtySpawn(int argc, const char* const* argv)
        : PtySpawn(argc, const_cast<char**>(argv)) // NOLINT(cppcoreguidelines-pro-type-const-cast)
    {
    }

    PtySpawn(int argc, char** argv);

    void write(const std::string& data);

    std::string read(std::chrono::milliseconds timeout = std::chrono::seconds(2));

private:
    static void disable_echo(int fd);

    int m_master;
};
