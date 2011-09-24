/*
 * This file is part of synconv.
 *
 * © 2011 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#ifndef FLAC_CODEC_H
#define FLAC_CODEC_H

#include "Codec.h"

class FlacCodec : public Codec
{
    public:
        void enter_decoder_pipeline(pipeline *p)
        {
            pipeline_command_args(p, "flac", "-s", "-d", "-c", "-", NULL);
        }

        void enter_encoder_pipeline(pipeline *p)
        {
            pipeline_command_args(p, "flac", "-s", "-c", "-", NULL);
        }
};

#endif
