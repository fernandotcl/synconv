/*
 * This file is part of synconv.
 *
 * © 2013 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#import "SCVDecoder.h"
#import "SCVEncoder.h"
#import "SCVPlugin.h"

@interface SCVVorbisPlugin : SCVPlugin <SCVDecoder, SCVEncoder>

@end
