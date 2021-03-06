// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <new>
#include <patch/cmdline.h>
#include <patch/patch.h>

namespace Patch {

int process_patch(const Options&)
{
    throw std::bad_alloc();
}

const Options& CmdLineParser::parse()
{
    static Options options;
    return options;
}

} // namespace Patch
