/*
 * This file is part of synconv.
 *
 * © 2013 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#import <Foundation/Foundation.h>

#import "SCVPlugin.h"

@interface SCVPluginManager : NSObject

+ (instancetype)sharedInstance;

- (SCVPlugin <SCVPlugin> *)pluginForEncodingWithName:(NSString *)name;

- (SCVPlugin <SCVPlugin> *)pluginForDecodingFileWithExtension:(NSString *)extension;

@end
