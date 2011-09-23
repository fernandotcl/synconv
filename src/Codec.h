/*
 * This file is part of mp3sync.
 *
 * © 2011 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#ifndef CODEC_H
#define CODEC_H

#include "Decoder.h"
#include "Encoder.h"

class Codec : public Encoder, public Decoder
{
};

#endif
