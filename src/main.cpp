/*
 * This file is part of synconv.
 *
 * © 2011 Fernando Tarlá Cardoso Lemos
 *
 * synconv is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * synconv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with synconv.  If not, see <http://www.gnu.org/licenses/>.
 * Refer to the LICENSE file for licensing information.
 *
 */

#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include <iostream>

#include "config.h"
#include "Walker.h"

namespace fs = boost::filesystem;

static void print_usage(std::ostream &out)
{
    out << "\
Usage: \n\
    synconv [options] <file or dir> [<file or dir>] [...] <output dir>\n\
    synconv --help\
" << std::endl;
}

int main(int argc, char **argv)
{
    struct option long_options[] = {
        {"dont-copy-others", no_argument, NULL, 'C'},
        {"dont-recurse", no_argument, NULL, 'R'},
        {"help", no_argument, NULL, 'h'},
        {"overwrite-mode", required_argument, NULL, 'o'},
        {"threads", required_argument, NULL, 't'},
        {NULL, 0, NULL, 0}
    };

    // Create the walker object
    Walker walker;

    // Parse the command line options
    int opt;
    while ((opt = getopt_long(argc, argv, "CRho:t:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'C':
                walker.set_copy_other(false);
                break;
            case 'R':
                walker.set_recursive(false);
                break;
            case 'h':
                print_usage(std::cout);
                return EXIT_SUCCESS;
            case 'o':
                if (!strcpy(optarg, "auto")) {
                    walker.set_overwrite_mode(Walker::OverwriteAuto);
                }
                else if (!strcpy(optarg, "always")) {
                    walker.set_overwrite_mode(Walker::OverwriteAlways);
                }
                else if (!strcpy(optarg, "never")) {
                    walker.set_overwrite_mode(Walker::OverwriteNever);
                }
                else {
                    print_usage(std::cerr);
                    return EXIT_SUCCESS;
                }
                break;
            case 't':
                break;
            default:
                print_usage(std::cerr);
                return EXIT_FAILURE;
        }
    }

    // We need at least an input and an output
    int num_args = argc - optind;
    if (num_args < 2) {
        print_usage(std::cerr);
        return EXIT_FAILURE;
    }

	// Make sure the last argument is a directory if it exists
    fs::path output_dir(argv[argc - 1]);
    bool output_dir_exists = fs::exists(output_dir);
    if (output_dir_exists && !fs::is_directory(output_dir)) {
        std::cerr << PROGRAM_NAME ": target `" << output_dir << " is not a directory" << std::endl;
        return EXIT_FAILURE;
    }

	// If multiple input paths are specified, the destination directory must exist
	if (num_args > 2 && !fs::exists(output_dir)) {
        std::cerr << PROGRAM_NAME ": target `" << output_dir << "' is not a directory" << std::endl;
        return EXIT_FAILURE;
	}

    // Get the path objects
    std::vector<fs::path> input_paths(num_args - 1);
    for (int i = 0; i < num_args - 1; ++i)
        input_paths[i] = argv[optind + i];

	// Make sure the other arguments are existing files or directories
    BOOST_FOREACH(const fs::path &p, input_paths) {
        if (!fs::exists(p)) {
            std::cerr << PROGRAM_NAME ": cannot stat `" << p << "': No such file or directory" << std::endl;
            return EXIT_FAILURE;
        }
    }

    // Run the walker
    walker.walk(input_paths, output_dir);

    return EXIT_SUCCESS;
}
