// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 Shannon Booth <shannon.ml.booth@gmail.com>

#include <exception>
#include <iostream>
#include <patch/cmdline.h>
#include <patch/options.h>
#include <patch/patch.h>

int main(int argc, const char* const* argv)
{
    try {
        Patch::OptionHandler handler;
        Patch::CmdLine cmdline(argc, argv);
        Patch::CmdLineParser cmdline_parser(cmdline);

        cmdline_parser.parse(handler);
        handler.apply_defaults();

        return Patch::process_patch(handler.options());
    } catch (const std::bad_alloc&) {
        std::cerr << argv[0] << ": **** out of memory\n";
        return 2;
    } catch (const Patch::cmdline_parse_error& e) {
        std::cerr << argv[0] << ": " << e.what() << '\n'
                  << argv[0] << ": Try '" << argv[0] << " --help' for more information.\n";
        return 2;
    } catch (const std::exception& e) {
        std::cerr << argv[0] << ": **** " << e.what() << '\n';
        return 2;
    }
}
