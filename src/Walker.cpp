/*
 * This file is part of synconv.
 *
 * © 2011-2013 Fernando Tarlá Cardoso Lemos
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
#include <sys/wait.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <algorithm>
#include <cassert>
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

#include "AlacEncoder.h"
#include "config.h"
#include "ConservativeRenamingFilter.h"
#include "DummyCodec.h"
#include "FlacCodec.h"
#include "LameCodec.h"
#include "Walker.h"
#include "VorbisCodec.h"

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
    : m_overwrite(OverwriteAuto), m_recursive(true),
      m_copy_other(true), m_reencode(false),
      m_verbose(false), m_quiet(false), m_dry_run(false),
      m_encoder(NULL), m_delete(false),
      m_num_workers(1), m_workers_should_quit(false)
{
    // Initialize the decoders and encoders
    m_encoders["alac"] = boost::shared_ptr<RootObject>(new AlacEncoder());
    m_decoders["dummy"] = m_encoders["dummy"] = boost::shared_ptr<RootObject>(new DummyCodec());
    m_decoders["flac"] = m_encoders["flac"] = boost::shared_ptr<RootObject>(new FlacCodec());
    m_decoders["lame"] = m_encoders["lame"] = boost::shared_ptr<RootObject>(new LameCodec());
    m_decoders["vorbis"] = m_encoders["vorbis"] = boost::shared_ptr<RootObject>(new VorbisCodec());
}

bool Walker::set_encoder(const std::string &name)
{
    if (name == "alac") {
        m_encoder = dynamic_cast<Encoder *>(m_encoders["alac"].get());
        m_encoder_ext = L".m4a";
    }
    else if (name == "dummy" || name == "wave" || name == "wav") {
        m_encoder = dynamic_cast<Encoder *>(m_encoders["dummy"].get());
        m_encoder_ext = L".wav";
    }
    else if (name == "flac") {
        m_encoder = dynamic_cast<Encoder *>(m_encoders["flac"].get());
        m_encoder_ext = L".flac";
    }
    else if (name == "lame" || name == "mp3") {
        m_encoder = dynamic_cast<Encoder *>(m_encoders["lame"].get());
        m_encoder_ext = L".mp3";
    }
    else if (name == "vorbis") {
        m_encoder = dynamic_cast<Encoder *>(m_encoders["vorbis"].get());
        m_encoder_ext = L".ogg";
    }
    else {
        std::cerr << PROGRAM_NAME ": unrecognized encoder name" << std::endl;
        return false;
    }
    assert(m_encoder);
    return true;
}

void Walker::set_encoder_options(const std::list<std::string> &options)
{
    assert(m_encoder);
    BOOST_FOREACH(const std::string &option, options)
        m_encoder->add_extra_option(option);
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

    // We now need to define what the next output dir should be. To do
    // that, we take a relative path from the base dir, and append that
    // path to the base output dir. Note that the base dir (base_dir) is
    // a prefix of the current dir (p). So, in order to get a relative
    // path, we can just remove the common prefix from the current dir.

    // Calculate the last position in common between the current dir and
    // the base dir, taking into account that the base dir may have a dir
    // separator appended to it
    const std::wstring &base_dir = m_base_dir.string<std::wstring>();
    std::wstring::size_type pos = base_dir.size();
    if (boost::ends_with(base_dir, L"/"))
        --pos;

    // Use that to calculate suffix, then apply the renaming filter
    std::wstring suffix(p.string<std::wstring>().substr(pos));
    apply_renaming_filter(suffix);

    // Now we can calculate the output dir
    m_output_dir = m_base_output_dir / suffix;
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
            decoder = dynamic_cast<Decoder *>(m_decoders["flac"].get());
        else if (ext == ".mp3")
            decoder = dynamic_cast<Decoder *>(m_decoders["lame"].get());
        else if (ext == ".ogg" || ext == ".oga")
            decoder = dynamic_cast<Decoder *>(m_decoders["vorbis"].get());
        else if (ext == ".wav")
            decoder = dynamic_cast<Decoder *>(m_decoders["dummy"].get());
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

    // If the encoder can't read from stdin, we have
    // to create to output to a temporary file first
    fs::path tmp_file;
    if (!m_encoder->encodes_from_stdin()) {
        // Open the temporary file
        tmp_file = output_file.string() + ".wav";
        int tmpfd = try_open_output_file(tmp_file, in_st.st_mode, errors_file);
        if (tmpfd == -1)
            exit(EXIT_FAILURE);

        // Create a pipeline
        pipeline *pl = pipeline_new();

        // Add the decoder
        pipeline_want_infile(pl, p.string().c_str());
        decoder->enter_decoder_pipeline(pl);

        // Write directly to the temporary file
        pipeline_want_out(pl, tmpfd);

        // Run the pipeline and get rid of
        // the temporary file if that fails
        if (!run_and_free_pipeline(p, pl, errors_file)) {
            close(tmpfd);
            unlink(tmp_file.c_str());
            exit(EXIT_FAILURE);
        }

        // Done with tmpfd
        close(tmpfd);
    }

    // Open the encoder output file
    int outfd;
    if (m_encoder->encodes_to_stdout()) {
        outfd = try_open_output_file(output_file, in_st.st_mode, errors_file);
        if (outfd == -1) {
            // Unlink the temporary file
            if (!m_encoder->encodes_from_stdin())
                unlink(tmp_file.c_str());

            // Exit with error
            exit(EXIT_FAILURE);
        }
    }

    // Create the pipeline
    pipeline *pl = pipeline_new();

    if (m_encoder->encodes_from_stdin()) {
        // If the encoder accepts input from stdin,
        // we can simply plug in the decoder
        pipeline_want_infile(pl, p.string().c_str());
        decoder->enter_decoder_pipeline(pl);
    }
    else {
        // Otherwise we'll let the encoder open
        // the temporary file instead
        m_encoder->set_encoder_input_file(tmp_file.c_str());
    }

    if (m_encoder->encodes_to_stdout()) {
        // Add the encoder (order matters)
        m_encoder->enter_encoder_pipeline(pl);

        // If the encoder accepts output to stdout,
        // we can simply pipe it to the output fd
        pipeline_want_out(pl, outfd);
    }
    else {
        // Otherwise we'll let the encoder open
        // the output file instead
        m_encoder->set_encoder_output_file(output_file.c_str());

        // Add the encoder (order matters)
        m_encoder->enter_encoder_pipeline(pl);
    }

    // Run the pipeline
    bool pipeline_success = run_and_free_pipeline(p, pl, errors_file);

    // Close the open descriptor
    if (m_encoder->encodes_to_stdout())
        close(outfd);

    // Get rid of the temporary file
    if (!m_encoder->encodes_to_stdout())
        unlink(tmp_file.c_str());

    // Get rid of the output file and exit with error
    // if the pipeline failed
    if (!pipeline_success) {
        unlink(output_file.c_str());
        exit(EXIT_FAILURE);
    }

    // If we didn't create the file ourselves, we
    // have to fix the permissions
    if (!m_encoder->encodes_to_stdout())
        chmod(output_file.c_str(), in_st.st_mode);

    // Return successfully
    exit(EXIT_SUCCESS);
}

int Walker::try_open_output_file(const fs::path &p, mode_t mode, const char *errors_file) {
    int fd = open(p.c_str(), O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (fd == -1) {
        std::ofstream err_out(errors_file);
        err_out << PROGRAM_NAME ": unable to open `" << p.string() << "' for writing" << std::endl;
    }
    return fd;
}

bool Walker::run_and_free_pipeline(const fs::path &p, pipeline *pl, const char *errors_file) {
    // Get things started
    pipeline_start(pl);
    int status = pipeline_wait(pl);

    // Handle errors
    if (status != 0) {
        char *pipeline_str = pipeline_tostring(pl);
        {
            std::ofstream err_out(errors_file);
            err_out << PROGRAM_NAME ": failed to transcode `" << p.string() << "'" << std::endl;
            err_out << "The transcoding pipeline was `" << pipeline_str << "'" << std::endl;
        }
        free(pipeline_str);
        pipeline_free(pl);
        return false;
    }

    // Free the pipeline and return success
    pipeline_free(pl);
    return true;
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
