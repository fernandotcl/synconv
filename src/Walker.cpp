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

#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <sys/stat.h>
#include <sys/types.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <algorithm>
#include <exception>
#include <fcntl.h>
#include <iostream>
#include <utime.h>

extern "C" {
#include <pipeline.h>
}

#include "config.h"
#include "ConservativeRenamingFilter.h"
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
    : m_overwrite(OverwriteAuto), m_recursive(true), m_copy_other(true),
      m_verbose(false), m_quiet(false), m_encoder(NULL)
{
}

bool Walker::set_encoder(const std::string &name)
{
    if (name == "flac") {
        m_encoder = &m_flac_codec;
        m_encoder_ext = L".flac";
    }
    else if (name == "lame") {
        m_encoder = &m_lame_codec;
        m_encoder_ext = L".mp3";
    }
    else if (name == "vorbis") {
        m_encoder = &m_vorbis_codec;
        m_encoder_ext = L".ogg";
    }
    else {
        std::cerr << PROGRAM_NAME ": unrecognized encoder name" << std::endl;
        return false;
    }
    return true;
}

bool Walker::set_renaming_filter(const std::string &filter)
{
    if (filter == "conservative") {
        m_renaming_filter.reset(new ConservativeRenamingFilter);
    }
    else if (filter == "none") {
        m_renaming_filter.reset();
    }
    else {
        std::cerr << PROGRAM_NAME ": unrecognized renaming filter" << std::endl;
        return false;
    }
    return true;
}

void Walker::apply_renaming_filter(std::wstring &path_component)
{
    if (m_renaming_filter.get())
        path_component = m_renaming_filter->filter(path_component);
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
    BOOST_FOREACH(const fs::path &rel_p, input_paths) {
        fs::path p(fs::absolute(rel_p));
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
            m_base_dir = p;
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
                visit_file(p);
        }
        else {
            std::cerr << PROGRAM_NAME ": skipping `" << p.string() << "' (not a regular file or directory)" << std::endl;
        }
    }
}

bool Walker::visit_directory(const fs::path &p)
{
    // Notify the user
    if (!m_verbose && !m_quiet)
        std::cout << "Entering `" << p.filename().string() << "'" << std::endl;

    // Nothing to do if we aren't in recursive mode
    if (!m_recursive)
        return false;

    // Calculate the next output dir
    std::wstring diff(p.string<std::wstring>().substr(m_base_dir.string<std::wstring>().size() + 1));
    apply_renaming_filter(diff);
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
    if (fs::create_directories(m_output_dir, ec)) {
        m_output_dir_created = true;
        return true;
    }
    else {
        std::cerr << "Unable to create directory `" << m_output_dir << "': " << ec.message() << std::endl;
        m_output_dir_error = true;
        return false;
    }
}

bool Walker::restore_timestamps(const boost::filesystem::path &p, const struct stat &st)
{
    struct utimbuf ut;
    ut.actime = st.st_atime;
    ut.modtime = st.st_mtime;
    if (utime(p.c_str(), &ut) == -1) {
        std::cerr << "Unable to change the timestamp metadata for `" << p.string() << "'" << std::endl;
        return false;
    }
    return true;
}

void Walker::visit_file(const fs::path &p)
{
    // Don't do anything if the output directory is in error
    if (m_output_dir_error)
        return;

    // Get and normalize the extension
    std::string ext = boost::to_lower_copy(p.extension().string());

    // Select the codec based on the extension
    Decoder *decoder = NULL;
    if (ext == ".flac")
        decoder = &m_flac_codec;
    else if (ext == ".mp3")
        decoder = &m_lame_codec;
    else if (ext == ".ogg" || ext == ".oga")
        decoder = &m_vorbis_codec;

    // Create the output filename
    std::wstring suffix;
    if (decoder)
        suffix = p.stem().string<std::wstring>() + m_encoder_ext;
    else
        suffix = p.filename().string<std::wstring>();
    apply_renaming_filter(suffix);
    fs::path output_file = m_output_dir / suffix;

    // Stat the input file to get mode and creation time
    struct stat in_st;
    if (stat(p.c_str(), &in_st) == -1) {
        std::cerr << PROGRAM_NAME ": failed to stat `" << p.string() << "'" << std::endl;
        return;
    }

    // Stat the output file to check if we should proceed
    if (m_overwrite != OverwriteAlways) {
        struct stat out_st;
        if (!stat(output_file.c_str(), &out_st)) {
            // If we're never overwriting it, nothing else to do
            if (m_overwrite == OverwriteNever) {
                if (m_verbose)
                    std::cout << PROGRAM_NAME ": skipping `" << p.string() << "' (not overwriting)" << std::endl;
                return;
            }
            // m_overwrite == OverwriteAuto, check timestamps
            if (in_st.st_mtime <= out_st.st_mtime) {
                if (m_verbose)
                    std::cout << PROGRAM_NAME ": skipping `" << p.string() << "' (not overwriting)" << std::endl;
                return;
            }
        }
    }

    // Either copy or skip the file if we don't have a decoder for it
    if (!decoder || (static_cast<Codec *>(decoder) == static_cast<Codec *>(m_encoder) && !m_reencode)) {
        if (m_copy_other) {
            if (!create_output_dir())
                return;
            boost::system::error_code ec;
            fs::copy_file(p, output_file, fs::copy_option::overwrite_if_exists, ec);
            if (ec)
                std::cerr << PROGRAM_NAME ": failed to copy `" << p.string() << "': " << ec.message() << std::endl;
            restore_timestamps(output_file, in_st);
            if (!m_quiet) {
                if (m_verbose)
                    std::cout << "`" << p << "' -> `" << output_file.string() << "'" << std::endl;
                else
                    std::cout << "Copied `" << output_file.filename().string() << "'" << std::endl;
            }
            return;
        }
        else {
            if (m_verbose)
                std::cout << PROGRAM_NAME ": skipping `" << p.string() << "'" << std::endl;
            return;
        }
    }

    // Create the output directory
    if (!create_output_dir())
        return;

    // Open the output file
    int outfd = open(output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, in_st.st_mode);
    if (outfd == -1) {
        std::cerr << PROGRAM_NAME ": unable to open `" << output_file.string() << "' for writing" << std::endl;
        return;
    }

    // Create the pipeline
    pipeline *pl = pipeline_new();
    pipeline_want_infile(pl, p.string().c_str());
    decoder->enter_decoder_pipeline(pl);
    m_encoder->enter_encoder_pipeline(pl);
    pipeline_want_out(pl, outfd);

    // Get things started
    pipeline_start(pl);
    int status = pipeline_wait(pl);

    // Make sure the commands returned EXIT_SUCCESS
    if (status != 0) {
        std::cerr << PROGRAM_NAME ": failed to transcode `" << p.string() << "'" << std::endl;
        char *pipeline_str = pipeline_tostring(pl);
        std::cerr << "The transcoding pipeline was `" << pipeline_str << "'" << std::endl;
        free(pipeline_str);
        pipeline_free(pl);
        close(outfd);
        unlink(output_file.c_str());
        return;
    }

    // Free the pipeline
    pipeline_free(pl);
    close(outfd);

    // Transfer the tags
    TagLib::FileRef in_tags(p.c_str());
    TagLib::FileRef out_tags(output_file.c_str());
    TagLib::Tag::duplicate(in_tags.tag(), out_tags.tag());
    out_tags.save();

    // Restore the timestamps
    restore_timestamps(output_file, in_st);

    // Notify the user
    if (!m_quiet) {
        if (m_verbose) {
            std::cout << "`" << p.string() << "' -> `" << output_file.string() << "'" << std::endl;
        }
        else {
            if (static_cast<Codec *>(decoder) == static_cast<Codec *>(m_encoder))
                std::cout << "Re-encoded `";
            else
                std::cout << "Transcoded `";
            std::cout << output_file.filename().string() << "'" << std::endl;
        }
    }
}
