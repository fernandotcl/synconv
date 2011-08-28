/*
 * This file is part of mp3sync.
 *
 * © 2011 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <sys/stat.h>
#include <algorithm>
#include <exception>
#include <iostream>

#include "Walker.h"

namespace fs = boost::filesystem;

template <typename FileVisitor, typename DirectoryVisitor>
static inline void walk(const fs::path &p, FileVisitor f, DirectoryVisitor d)
{
    // Get a sorted list of children paths
    std::vector<fs::path> children;
    std::copy(fs::directory_iterator(p), fs::directory_iterator(), std::back_inserter(children));
    std::sort(children.begin(), children.end());

    // Iterate through them
    BOOST_FOREACH(const fs::path &p, children) {
        if (fs::is_directory(p) && d(p))
            walk(p, f, d);
        else
            f(p);
    }
}

Walker::Walker()
    : m_overwrite(OverwriteAuto), m_recursive(true), m_copy_other(true)
{
}

void Walker::walk(const std::vector<fs::path> &input_paths, fs::path &output_dir)
{
    output_dir = fs::absolute(output_dir);
    BOOST_FOREACH(const fs::path &p, input_paths) {
        if (fs::is_directory(p)) {
			// If the last character of the input directory is the path separator
			// or if the output directory doesn't exist, copy its contents instead
            // (like the Unix cp command does)
            m_output_dir = output_dir;
            if (!boost::ends_with(p.string(), "/") && fs::exists(output_dir))
                m_output_dir /= p.filename();

            // Walk the hierarchy
            m_base_output_dir = m_output_dir;
            m_base_dir = fs::absolute(p);
            try {
                ::walk(p, boost::bind(&Walker::visit_file, this, _1), boost::bind(&Walker::visit_directory, this, _1));
            }
            catch (std::exception &e) {
                std::cerr << "mp3sync: " << e.what() << std::endl;
            }
        }
        else if (fs::is_regular_file(p)) {
            // Just convert the single file
            m_output_dir = output_dir;
            m_output_dir_created = false;
            m_output_dir_error = false;
            visit_file(fs::absolute(p));
        }
        else {
            std::cerr << "mp3sync: skipping `" << p << "' (not a regular file or directory)" << std::endl;
        }
    }
}

bool Walker::visit_directory(const fs::path &p)
{
    // Nothing to do if we aren't in recursive mode
    if (!m_recursive)
        return false;

    // Calculate the next output dir
    std::string diff(p.string().substr(m_base_dir.string().size()));
    m_output_dir = m_base_output_dir / diff;
    m_output_dir = fs::absolute(m_output_dir);
    m_output_dir_created = false;
    m_output_dir_error = false;
    return true;
}

void Walker::visit_file(const fs::path &p)
{
	// Don't do anything if the output directory is in error
    if (m_output_dir_error)
        return;

	// Get and normalize the extension and calculate the new filename if converted
    std::string ext = boost::to_lower_copy(p.extension().string());
    fs::path output_file = m_output_dir / (p.stem().string() + ".mp3");

    // Stat the output file to check if we should proceed
    if (m_overwrite != OverwriteAlways) {
        struct stat out_st;
        if (!stat(output_file.c_str(), &out_st)) {
            // If we're never overwriting it, nothing else to do
            if (m_overwrite == OverwriteNever) {
                std::cout << "mp3sync: skipping `" << p << "' (not overwriting)" << std::endl;
                return;
            }
            // m_overwrite == OverwriteAuto, check timestamps
            struct stat in_st;
            if (stat(p.string().c_str(), &in_st) == -1) {
                std::cerr << "mp3sync: stat(2) failed for `" << p << '`' << std::endl;
                return;
            }
            if (in_st.st_mtime >= out_st.st_mtime) {
                std::cout << "mp3sync: skipping `" << p << "' (not overwriting)" << std::endl;
                return;
            }
        }
    }

    std::cout << "XXX " << p << " (" << ext << ") -> " << output_file << std::endl;
}
