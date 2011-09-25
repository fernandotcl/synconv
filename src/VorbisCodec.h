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

#ifndef VORBIS_CODEC_H
#define VORBIS_CODEC_H

#include "Codec.h"

class VorbisCodec : public Codec
{
    public:
        void enter_decoder_pipeline(pipeline *p)
        {
            pipeline_command_args(p, "oggdec", "-Q", "-o", "-", "-", NULL);
        }

        void enter_encoder_pipeline(pipeline *p)
        {
            pipecmd *cmd = pipecmd_new_args("oggenc", "-Q", "-r", NULL);
            add_extra_options(cmd);
            pipecmd_arg(cmd, "-");
            pipeline_command(p, cmd);
        }
};

#endif
