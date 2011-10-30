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
#include <boost/thread/locks.hpp>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <algorithm>
#include <cstdlib>
#include <exception>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <unistd.h>
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

static bool compare_paths_for_deletion(const fs::path &lhs, const fs::path &rhs)
{
    std::string lhs_s(lhs.string());
    std::string rhs_s(rhs.string());
    if (lhs_s.size() > rhs_s.size()) {
        if (lhs_s.substr(0, rhs_s.size()) == rhs_s)
            return true;
        else
            return lhs < rhs;
    }
    else if (rhs_s.size() > lhs_s.size()) {
        if (rhs_s.substr(0, lhs_s.size()) == lhs_s)
            return false;
        else
            return lhs < rhs;
    }
    else {
        return lhs < rhs;
    }
}

Walker::Walker()
    : m_overwrite(OverwriteAuto), m_recursive(true), m_copy_other(true),
      m_verbose(false), m_quiet(false), m_dry_run(false),
      m_encoder(NULL), m_delete(false),
      m_num_workers(1), m_workers_should_quit(false)
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

void Walker::add_dont_transcode(const std::string &ext)
{
    if (ext.size() > 0) {
        std::string extension = boost::to_lower_copy(ext);
        if (extension[0] != '.')
            extension = '.' + extension;
        m_dont_transcode_exts.insert(extension);
    }
}

void Walker::set_output_extension(const std::string &ext)
{
    m_forced_ext.assign(ext.size(), L' ');
    std::copy(ext.begin(), ext.end(), m_forced_ext.begin());
    if (m_forced_ext[0] != L'.')
        m_forced_ext = L'.' + m_forced_ext;
}

bool Walker::check_output_dir(const fs::path &output_dir)
{
    m_output_dir_created = false;
    m_output_dir_error = false;
    if (fs::exists(m_output_dir)) {
        if (!fs::is_directory(m_output_dir)) {
            boost::unique_lock<boost::mutex> lock(m_mutex);
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
    // Create the worker threads
    m_workers.reserve(m_num_workers);
    for (unsigned int i = 0; i < m_num_workers; ++i)
        m_workers.push_back(boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&Walker::worker_thread, this))));

    output_dir = fs::absolute(output_dir);
    BOOST_FOREACH(const fs::path &rel_p, input_paths) {
        fs::path p(fs::absolute(rel_p));
        if (fs::is_directory(p)) {
            // If the last character of the input directory is the path separator
            // or if the output directory doesn't exist, copy its contents instead
            // (like the Unix cp command does)
            m_output_dir = output_dir;
            if (!boost::ends_with(p.string(), "/") && fs::exists(output_dir)) {
                std::wstring newdir(p.filename().string<std::wstring>());
                apply_renaming_filter(newdir);
                m_output_dir /= newdir;
            }
            if (!check_output_dir(output_dir))
                continue;

            // Walk the hierarchy
            m_base_output_dir = m_output_dir;
            m_base_dir = p;
            try {
                ::walk(p, boost::bind(&Walker::visit_file, this, _1), boost::bind(&Walker::visit_directory, this, _1));
            }
            catch (std::exception &e) {
                boost::unique_lock<boost::mutex> lock(m_mutex);
                std::cerr << PROGRAM_NAME ": " << e.what() << std::endl;
            }

            if (m_delete) {
                // Walk the output hierarchy, accumulate the paths we didn't touch
                if (!m_dry_run || (fs::exists(m_base_output_dir) && fs::is_directory(m_base_output_dir))) {
                    try {
                        ::walk(m_base_output_dir, boost::bind(&Walker::visit_file_deletion, this, _1),
                                boost::bind(&Walker::visit_directory_deletion, this, _1));
                    }
                    catch (std::exception &e) {
                        boost::unique_lock<boost::mutex> lock(m_mutex);
                        std::cerr << PROGRAM_NAME ": " << e.what() << std::endl;
                    }
                }
                m_dont_delete_paths.clear();

                // Sort the paths so we can delete them, then delete them
                m_paths_to_delete.sort(compare_paths_for_deletion);
                BOOST_FOREACH(const fs::path &p, m_paths_to_delete) {
                    bool delete_error = false;
                    if (!m_dry_run) {
                        boost::system::error_code ec;
                        fs::remove(p, ec);
                        if (ec) {
                            boost::unique_lock<boost::mutex> lock(m_mutex);
                            std::cerr << PROGRAM_NAME ": failed to delete `" << p.string() << "': " << ec.message() << std::endl;
                            delete_error = true;
                        }
                    }
                    if (!delete_error) {
                        boost::unique_lock<boost::mutex> lock(m_mutex);
                        if (m_verbose)
                            std::cout << PROGRAM_NAME ": deleted `" << p.string() << "'" << std::endl;
                        else
                            std::cout << "Deleted `" << p.filename().string() << "'" << std::endl;
                    }
                }
            }
        }
        else if (fs::is_regular_file(p)) {
            // Just convert the single file
            m_output_dir = output_dir;
            if (check_output_dir(output_dir))
                visit_file(p);
        }
        else {
            boost::unique_lock<boost::mutex> lock(m_mutex);
            std::cerr << PROGRAM_NAME ": skipping `" << p.string() << "' (not a regular file or directory)" << std::endl;
        }
    }

    {
        // Wait until all work is done and signal that to the workers
        boost::unique_lock<boost::mutex> workers_lock(m_workers_mutex);
        while (m_work_unit.get())
            m_workers_cond.wait(workers_lock);
        m_workers_should_quit = true;
    }

    // Let all workers know and join the threads
    m_workers_cond.notify_all();
    BOOST_FOREACH(boost::shared_ptr<boost::thread> worker, m_workers)
        worker->join();

    // Inform the user if we ran in dry run mode
    if (m_dry_run)
        std::cout << PROGRAM_NAME ": finished running in dry-run mode, no actual changes made" << std::endl;
}

bool Walker::visit_directory(const fs::path &p)
{
    // Notify the user
    if (!m_verbose && !m_quiet) {
        boost::unique_lock<boost::mutex> lock(m_mutex);
        std::cout << "Entering `" << p.filename().string() << "'" << std::endl;
    }

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
    // All good, we already have an output dir or if we're in dry run mode
    if (m_output_dir_created || m_dry_run)
        return true;

    // Check if we need to create it
    if (!check_output_dir(m_output_dir))
        return false;

    // We need to check again if we have the output dir, as
    // check_output_dir will look in the filesystem
    if (m_output_dir_created)
        return true;

    // Try to create it
    boost::system::error_code ec;
    if (fs::create_directories(m_output_dir, ec)) {
        m_output_dir_created = true;
        return true;
    }
    else {
        boost::unique_lock<boost::mutex> lock(m_mutex);
        std::cerr << "Unable to create directory `" << m_output_dir << "': " << ec.message() << std::endl;
        m_output_dir_error = true;
        return false;
    }
}

bool Walker::restore_timestamps(const fs::path &p, const struct stat &st)
{
    struct utimbuf ut;
    ut.actime = st.st_atime;
    ut.modtime = st.st_mtime;
    if (utime(p.c_str(), &ut) == -1) {
        boost::unique_lock<boost::mutex> lock(m_mutex);
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
    if (m_dont_transcode_exts.find(ext) == m_dont_transcode_exts.end()) {
        if (ext == ".flac")
            decoder = &m_flac_codec;
        else if (ext == ".mp3")
            decoder = &m_lame_codec;
        else if (ext == ".ogg" || ext == ".oga")
            decoder = &m_vorbis_codec;
    }

    // Check if we're transcoding or not
    bool transcoding = decoder && (static_cast<Codec *>(decoder) != static_cast<Codec *>(m_encoder) || m_reencode);

    // Create the output filename
    std::wstring suffix;
    if (transcoding) {
        suffix = p.stem().string<std::wstring>();
        if (m_forced_ext.empty())
            suffix += m_encoder_ext;
        else
            suffix += m_forced_ext;
    }
    else {
        suffix = p.filename().string<std::wstring>();
    }
    apply_renaming_filter(suffix);
    fs::path output_file = m_output_dir / suffix;

    // Stat the input file to get mode and creation time
    struct stat in_st;
    if (stat(p.c_str(), &in_st) == -1) {
        boost::unique_lock<boost::mutex> lock(m_mutex);
        std::cerr << PROGRAM_NAME ": failed to stat `" << p.string() << "'" << std::endl;
        return;
    }

    // Stat the output file to check if we should proceed
    if (m_overwrite != OverwriteAlways) {
        struct stat out_st;
        if (!stat(output_file.c_str(), &out_st)) {
            // If we're never overwriting it, nothing else to do
            if (m_overwrite == OverwriteNever) {
                add_dont_delete_file(output_file);
                if (m_verbose) {
                    boost::unique_lock<boost::mutex> lock(m_mutex);
                    std::cout << PROGRAM_NAME ": skipping `" << p.string() << "' (not overwriting)" << std::endl;
                }
                return;
            }
            // m_overwrite == OverwriteAuto, check timestamps
            if (in_st.st_mtime <= out_st.st_mtime) {
                add_dont_delete_file(output_file);
                if (m_verbose) {
                    boost::unique_lock<boost::mutex> lock(m_mutex);
                    std::cout << PROGRAM_NAME ": skipping `" << p.string() << "' (up-to-date)" << std::endl;
                }
                return;
            }
        }
    }

    if (!transcoding) {
        if (m_copy_other) {
            // We're copying, create the output directory
            if (!create_output_dir())
                return;

            if (!m_dry_run) {
                // Copy the file
                boost::system::error_code ec;
                fs::copy_file(p, output_file, fs::copy_option::overwrite_if_exists, ec);
                if (ec) {
                    boost::unique_lock<boost::mutex> lock(m_mutex);
                    std::cerr << PROGRAM_NAME ": failed to copy `" << p.string() << "': " << ec.message() << std::endl;
                }

                // Restore the timestamps
                restore_timestamps(output_file, in_st);
            }

            // Notify the user
            if (!m_quiet) {
                boost::unique_lock<boost::mutex> lock(m_mutex);
                if (m_verbose)
                    std::cout << "`" << p << "' -> `" << output_file.string() << "'" << std::endl;
                else
                    std::cout << "Copied `" << output_file.filename().string() << "'" << std::endl;
            }

            // Don't delete this path afterwards
            add_dont_delete_file(output_file);
            return;
        }
        else {
            // Just skip this file, notify the user
            if (m_verbose) {
                boost::unique_lock<boost::mutex> lock(m_mutex);
                std::cout << PROGRAM_NAME ": skipping `" << p.string() << "'" << std::endl;
            }
            return;
        }
    }

    // Create the output directory
    if (!create_output_dir())
        return;

    // Don't delete this path afterwards
    add_dont_delete_file(output_file);

    {
        // Wait until we can post this work unit
        boost::unique_lock<boost::mutex> workers_lock(m_workers_mutex);
        while (m_work_unit.get())
            m_workers_cond.wait(workers_lock);

        // Post the work unit
        m_work_unit.reset(new work_unit_t);
        m_work_unit->decoder = decoder;
        m_work_unit->input = p;
        m_work_unit->output = output_file;
        m_work_unit->input_st = in_st;
    }

    // Let the workers know
    m_workers_cond.notify_one();
}

void Walker::parallel_transcode(boost::shared_ptr<work_unit_t> work)
{
    // Get easy references
    Decoder *decoder = work->decoder;
    fs::path &p(work->input);
    fs::path &output_file(work->output);
    struct stat &in_st(work->input_st);

    // Create an almost unique temporary filename for errors
    char tmp_template[] = "/tmp/" PROGRAM_NAME ".XXXXXX";
    const char *errors_file = mktemp(tmp_template);

    // Fork into a new process because libpipeline isn't thread safe
    pid_t pid = fork();
    if (pid == -1) {
        boost::unique_lock<boost::mutex> lock(m_mutex);
        std::cerr << PROGRAM_NAME ": unable to fork into another process" << std::endl;
        return;
    }

    // If we're the parent, just wait for the child and handle its return code
    else if (pid != 0) {
        int status;
        waitpid(pid, &status, 0);
        if (!WIFEXITED(status)) {
            boost::unique_lock<boost::mutex> lock(m_mutex);
            std::cerr << PROGRAM_NAME ": the forked process crashed (errors file at `" <<  errors_file << "')" << std::endl;
        }
        else {
            int rc = WEXITSTATUS(status);
            switch (rc) {
                case EXIT_SUCCESS: {
                    if (!m_dry_run) {
                        // Transfer the tags
                        TagLib::FileRef in_tags(p.c_str());
                        TagLib::FileRef out_tags(output_file.c_str());
                        TagLib::Tag::duplicate(in_tags.tag(), out_tags.tag());
                        out_tags.save();

                        // Restore the timestamps
                        restore_timestamps(output_file, in_st);
                    }

                    // Notify the user
                    if (!m_quiet) {
                        if (m_verbose) {
                            boost::unique_lock<boost::mutex> lock(m_mutex);
                            std::cout << "`" << p.string() << "' -> `" << output_file.string() << "'" << std::endl;
                        }
                        else {
                            boost::unique_lock<boost::mutex> lock(m_mutex);
                            if (static_cast<Codec *>(decoder) == static_cast<Codec *>(m_encoder))
                                std::cout << "Re-encoded `";
                            else
                                std::cout << "Transcoded `";
                            std::cout << output_file.filename().string() << "'" << std::endl;
                        }
                    }
                    break;
                }
                case EXIT_FAILURE: {
                    // Output the error message from the file
                    boost::unique_lock<boost::mutex> lock(m_mutex);
                    std::ifstream err_in(errors_file);
                    if (err_in.is_open()) {
                        std::string line;
                        while (getline(err_in, line))
                            std::cerr << line << std::endl;
                        err_in.close();
                        unlink(errors_file);
                    }
                    else {
                        std::cerr << PROGRAM_NAME ": unable to open the errors file" << std::endl;
                    }
                    break;
                }
                default: {
                    boost::unique_lock<boost::mutex> lock(m_mutex);
                    std::cerr << PROGRAM_NAME ": the forked process exited with an unknown return code" << std::endl;
                    break;
                }
            }
        }
        return;
    }

    // Nothing to do in dry run mode
    if (m_dry_run)
        exit(EXIT_SUCCESS);

    // Open the output file
    int outfd = open(output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, in_st.st_mode);
    if (outfd == -1) {
        std::ofstream err_out(errors_file);
        err_out << PROGRAM_NAME ": unable to open `" << output_file.string() << "' for writing" << std::endl;
        exit(EXIT_FAILURE);
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
        char *pipeline_str = pipeline_tostring(pl);
        {
            std::ofstream err_out(errors_file);
            err_out << PROGRAM_NAME ": failed to transcode `" << p.string() << "'" << std::endl;
            err_out << "The transcoding pipeline was `" << pipeline_str << "'" << std::endl;
        }
        free(pipeline_str);
        pipeline_free(pl);
        close(outfd);
        unlink(output_file.c_str());
        exit(EXIT_FAILURE);
    }

    // Free the pipeline
    pipeline_free(pl);
    close(outfd);

    // Return successfully
    exit(EXIT_SUCCESS);
}

void Walker::worker_thread()
{
    while (true) {
        boost::shared_ptr<work_unit_t> work;
        {
            // Wait until we have a work unit to work on
            boost::unique_lock<boost::mutex> workers_lock(m_workers_mutex);
            while (!m_work_unit.get() && !m_workers_should_quit)
                m_workers_cond.wait(workers_lock);

            // Check if we're done
            if (m_workers_should_quit)
                break;

            // Claim the work unit for ourselves
            work = m_work_unit;
            m_work_unit.reset();
        }

        // Notify the producer
        m_workers_cond.notify_all();

        // Work on the work item
        parallel_transcode(work);
    }
}

void Walker::add_dont_delete_file(const fs::path &p)
{
    if (m_delete) {
        m_dont_delete_paths.insert(p);
        m_dont_delete_paths.insert(p.parent_path());
    }
}

void Walker::visit_file_deletion(const fs::path &p)
{
    if (m_dont_delete_paths.find(p) == m_dont_delete_paths.end())
        m_paths_to_delete.push_back(p);
}

bool Walker::visit_directory_deletion(const fs::path &p)
{
    if (m_dont_delete_paths.find(p) == m_dont_delete_paths.end())
        m_paths_to_delete.push_back(p);
    return true;
}
