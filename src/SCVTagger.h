/*
 * This file is part of synconv.
 *
 * © 2013 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#import <Foundation/Foundation.h>

@interface SCVTagger : NSObject

+ (instancetype)sharedInstance;

- (void)copyTagsFromInputPath:(NSString *)inputPath toOutputPath:(NSString *)outputPath;

@end
