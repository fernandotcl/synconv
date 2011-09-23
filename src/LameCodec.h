/*
 * This file is part of mp3sync.
 *
 * © 2011 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#ifndef LAME_CODEC_H
#define LAME_CODEC_H

#include "Codec.h"

class LameCodec : public Codec
{
    public:
        void enter_decoder_pipeline(pipeline *p)
        {
            pipeline_command_args(p, "lame", "-S", "--decode", "-", "-", NULL);
        }

        void enter_encoder_pipeline(pipeline *p)
        {
            pipeline_command_args(p, "lame", "-S", "-V2", "-", "-", NULL);
        }
};

#endif
