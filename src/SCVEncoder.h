/*
 * This file is part of synconv.
 *
 * © 2013 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#import <Foundation/Foundation.h>

@protocol SCVEncoder <NSObject>

- (NSString *)preferredEncoderExtension;

- (NSTask *)encoderTask;

@end
