// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#pragma once

#include <iostream>
#include <stdexcept>

#define EXPECT_EQ(lhs, rhs)                                        \
    do {                                                           \
        if (lhs != rhs) {                                          \
            std::cerr << "FAIL: " << lhs << " != " << rhs << '\n'; \
            throw std::runtime_error("Test failed");               \
        }                                                          \
    } while (false)
