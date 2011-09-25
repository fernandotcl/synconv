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

#ifndef WALKER_H
#define WALKER_H

#include <boost/filesystem.hpp>
#include <sys/stat.h>
#include <vector>

#include "FlacCodec.h"
#include "LameCodec.h"
#include "VorbisCodec.h"

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
        void set_reencode(bool reencode) { m_reencode = reencode; }
        void set_verbose(bool verbose) { m_verbose = verbose; }
        void set_quiet(bool quiet) { m_quiet = quiet; }

        bool set_encoder(const std::string &name);
        bool has_encoder() const { return m_encoder != NULL; }

        void walk(const std::vector<boost::filesystem::path> &input_paths,
                boost::filesystem::path &output_dir);

    private:
        void visit_file(const boost::filesystem::path &p);
        bool visit_directory(const boost::filesystem::path &p);

        bool check_output_dir(const boost::filesystem::path &output_dir);
        bool create_output_dir();

        bool restore_timestamps(const boost::filesystem::path &p, const struct stat &st);

        OverwriteMode m_overwrite;
        bool m_recursive, m_copy_other, m_reencode;

        boost::filesystem::path m_output_dir, m_base_output_dir, m_base_dir;
        bool m_output_dir_created, m_output_dir_error;

        bool m_verbose, m_quiet;

        FlacCodec m_flac_codec;
        LameCodec m_lame_codec;
        VorbisCodec m_vorbis_codec;

        Encoder *m_encoder;
        std::string m_encoder_ext;
};

#endif
