/*
 * This file is part of mp3sync.
 *
 * © 2011 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#ifndef WALKER_H
#define WALKER_H

#include <boost/filesystem.hpp>
#include <vector>

class Walker
{
    public:
        enum OverwriteMode {
            OverwriteAuto,
            OverwriteAlways,
            OverwriteNever
        };

        Walker();

        void set_overwrite_mode(OverwriteMode mode) { m_overwrite = mode; }
        void set_recursive(bool recursive) { m_recursive = recursive; }
        void set_copy_other(bool copy_other) { m_copy_other = copy_other; }

        void walk(const std::vector<boost::filesystem::path> &input_paths,
                boost::filesystem::path &output_dir);

    private:
        void visit_file(const boost::filesystem::path &p);
        bool visit_directory(const boost::filesystem::path &p);

        OverwriteMode m_overwrite;
        bool m_recursive, m_copy_other;

        boost::filesystem::path m_output_dir, m_base_output_dir, m_base_dir;
        bool m_output_dir_created, m_output_dir_error;
};

#endif
