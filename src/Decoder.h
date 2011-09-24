/*
 * This file is part of synconv.
 *
 * © 2011 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#ifndef DECODER_H
#define DECODER_H

extern "C" {
#include <pipeline.h>
}

class Decoder
{
    public:
        Decoder() {};
        virtual ~Decoder() {};

        virtual void enter_decoder_pipeline(pipeline *p) = 0;
};

#endif
