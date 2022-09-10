// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <exception>
#include <iostream>
#include <patch/cmdline.h>
#include <patch/patch.h>

int main(int argc, const char* const* argv)
{
    try {
        Patch::CmdLine cmdline(argc, argv);
        Patch::CmdLineParser cmdline_parser(cmdline);
        return Patch::process_patch(cmdline_parser.parse());
    } catch (const std::bad_alloc&) {
        std::cerr << "patch: **** out of memory\n";
        return 2;
    } catch (const Patch::cmdline_parse_error& e) {
        std::cerr << "patch: **** " << e.what() << '\n'
                  << "Try '" << argv[0] << " --help' for more information.\n";
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "patch: **** " << e.what() << '\n';
        return 2;
    }
}
