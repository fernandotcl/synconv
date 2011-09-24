/*
 * This file is part of synconv.
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
#include <sys/types.h>
#include <algorithm>
#include <exception>
#include <fcntl.h>
#include <iostream>

extern "C" {
#include <pipeline.h>
}

#include "config.h"
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

bool Walker::check_output_dir(const fs::path &output_dir)
{
    m_output_dir_created = false;
    m_output_dir_error = false;
    if (fs::exists(m_output_dir)) {
        if (!fs::is_directory(m_output_dir)) {
            std::cerr << PROGRAM_NAME ": cannot overwrite non-directory `" << m_output_dir
                << "' with directory `" << output_dir << "'" << std::endl;
            return false;
        }
        m_output_dir_created = true;
    }
    return true;
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
            if (!check_output_dir(output_dir))
                continue;

            // Walk the hierarchy
            m_base_output_dir = m_output_dir;
            m_base_dir = fs::absolute(p);
            try {
                ::walk(p, boost::bind(&Walker::visit_file, this, _1), boost::bind(&Walker::visit_directory, this, _1));
            }
            catch (std::exception &e) {
                std::cerr << PROGRAM_NAME ": " << e.what() << std::endl;
            }
        }
        else if (fs::is_regular_file(p)) {
            // Just convert the single file
            m_output_dir = output_dir;
            if (check_output_dir(output_dir))
                visit_file(fs::absolute(p));
        }
        else {
            std::cerr << PROGRAM_NAME ": skipping `" << p << "' (not a regular file or directory)" << std::endl;
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

bool Walker::create_output_dir()
{
    // All good, we already have an output dir
    if (m_output_dir_created)
        return true;

    // Try to create it
    boost::system::error_code ec;
    if (fs::create_directory(m_output_dir, ec)) {
        std::cout << "XXX CREATED " << m_output_dir << std::endl;
        m_output_dir_created = true;
        return true;
    }
    else {
        std::cerr << "Unable to create directory `" << m_output_dir << "': " << ec.message() << std::endl;
        m_output_dir_error = true;
        return false;
    }
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
                std::cout << PROGRAM_NAME ": skipping `" << p << "' (not overwriting)" << std::endl;
                return;
            }
            // m_overwrite == OverwriteAuto, check timestamps
            struct stat in_st;
            if (stat(p.string().c_str(), &in_st) == -1) {
                std::cerr << PROGRAM_NAME ": stat(2) failed for `" << p << "`" << std::endl;
                return;
            }
            if (in_st.st_mtime >= out_st.st_mtime) {
                std::cout << PROGRAM_NAME ": skipping `" << p << "' (not overwriting)" << std::endl;
                return;
            }
        }
    }

    // Select the codec based on the extension
    Decoder *decoder;
    if (ext == ".flac") {
        decoder = &m_flac_codec;
    }

    // Try to copy the file
    else if (m_copy_other) {
        if (!create_output_dir())
            return;
        boost::system::error_code ec;
        std::cout << "`" << p << "' -> `" << output_file << "'" << std::endl;
        fs::copy_file(p, output_file, ec);
        if (ec)
            std::cerr << PROGRAM_NAME ": failed to copy `" << p << "': " << ec.message() << std::endl;
        return;
    }

    // Skipping this file
    else {
        std::cout << PROGRAM_NAME ": skipping `" << p << "'" << std::endl;
        return;
    }

    // Create the output directory
    if (!create_output_dir())
        return;

    // Open the output file
    int outfd = open(output_file.string().c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (outfd == -1) {
        std::cerr << PROGRAM_NAME ": unable to open `" << output_file << "' for writing" << std::endl;
        return;
    }

    // Create the pipeline
    std::cout << "`" << p << "' -> `" << output_file << "'" << std::endl;
    pipeline *pl = pipeline_new();
    pipeline_want_infile(pl, p.string().c_str());
    decoder->enter_decoder_pipeline(pl);
    m_lame_codec.enter_encoder_pipeline(pl);
    pipeline_want_out(pl, outfd);

    // Get things started
    pipeline_start(pl);
    int status = pipeline_wait(pl);
    pipeline_free(pl);
    close(outfd);

    // Make sure the commands returned EXIT_SUCCESS
    if (status != 0) {
        std::cerr << PROGRAM_NAME ": failed to transcode `" << p << "'" << std::endl;
        return;
    }

    // TODO: Save the permissions and timestamps
    // TODO: Save the tags
}
