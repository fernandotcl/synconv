/*
 * This file is part of synconv.
 *
 * © 2012-2013 Fernando Tarlá Cardoso Lemos
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

#ifndef ALAC_ENCODER_H
#define ALAC_ENCODER_H

#include "Encoder.h"
#include "RootObject.h"

class AlacEncoder : public Encoder, public RootObject
{
    public:
        bool encodes_from_stdin() { return false; }
        bool encodes_to_stdout() { return false; }

        void enter_encoder_pipeline(pipeline *p)
        {
            pipecmd *cmd = pipecmd_new_args("afconvert", "-d", "alac", NULL);
            add_extra_options(cmd);
            pipecmd_args(cmd, m_enc_input_file, m_enc_output_file, NULL);
            pipeline_command(p, cmd);
        }
};

#endif
