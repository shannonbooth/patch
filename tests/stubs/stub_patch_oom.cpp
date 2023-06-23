// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <new>
#include <patch/cmdline.h>
#include <patch/patch.h>

namespace Patch {

CmdLine::CmdLine(int, const char* const*)
{
}

int process_patch(const Options&)
{
    throw std::bad_alloc();
}

void CmdLineParser::parse(CmdLineParser::Handler&)
{
}

} // namespace Patch
