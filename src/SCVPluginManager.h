/*
 * This file is part of synconv.
 *
 * © 2013 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#import <Foundation/Foundation.h>

#import "SCVDecoder.h"
#import "SCVEncoder.h"
#import "SCVPlugin.h"

@interface SCVPluginManager : NSObject

+ (instancetype)sharedInstance;

- (SCVPlugin <SCVEncoder> *)pluginForEncodingWithName:(NSString *)name;

- (SCVPlugin <SCVDecoder> *)pluginForDecodingFileWithExtension:(NSString *)extension;

@end
