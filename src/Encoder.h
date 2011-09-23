/*
 * This file is part of mp3sync.
 *
 * © 2011 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#ifndef ENCODER_H
#define ENCODER_H

extern "C" {
#include <pipeline.h>
}

class Encoder
{
    public:
        Encoder() {};
        virtual ~Encoder() {};

        virtual void enter_encoder_pipeline(pipeline *p) = 0;
};

#endif
