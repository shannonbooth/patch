// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#pragma once

#include <iostream>
#include <istream>
#include <patch/hunk.h>
#include <patch/options.h>
#include <vector>

namespace Patch {

class File;

class RejectWriter {
public:
    explicit RejectWriter(const Patch& patch, File& reject_file, Options::RejectFormat reject_format = Options::RejectFormat::Default)
        : m_patch(patch)
        , m_reject_file(reject_file)
        , m_reject_format(reject_format)
    {
    }

    void write_reject_file(const Hunk& hunk);

    int rejected_hunks() const { return m_rejected_hunks; }

private:
    bool should_write_as_unified() const;

    const Patch& m_patch;
    int m_rejected_hunks { 0 };
    File& m_reject_file;
    Options::RejectFormat m_reject_format { Options::RejectFormat::Default };
};

struct Result {
    int failed_hunks;
    bool was_skipped;
    bool all_hunks_applied_perfectly;
};

Result apply_patch(File& out_file, RejectWriter& reject_writer, const std::vector<Line>& input_lines, Patch& patch, const Options& options = {}, std::ostream& out = std::cout);

void reverse(Patch& patch);

void reverse(Hunk& hunk);

} // namespace Patch
