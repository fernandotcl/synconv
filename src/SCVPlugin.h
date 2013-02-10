/*
 * This file is part of synconv.
 *
 * © 2013 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#import <Foundation/Foundation.h>

@interface SCVPlugin : NSObject

@property (nonatomic, strong) NSArray *additionalEncoderFlags;

@property (nonatomic, assign) BOOL enabledForDecoding;

@property (nonatomic, assign) BOOL enabledForEncoding;

- (NSString *)pluginName;

- (NSArray *)pluginAliases;

- (void)checkExecutables;

- (void)findPathForDecoderExecutable:(NSArray *)executableNames;

- (void)findPathForEncoderExecutable:(NSArray *)executableNames;

- (void)findPathForDecoderAndEncoderExecutable:(NSArray *)executableNames;

- (NSTask *)decoderTaskWithPrefixArguments:(NSArray *)prefixArguments
                           suffixArguments:(NSArray *)suffixArguments;

- (NSTask *)encoderTaskWithPrefixArguments:(NSArray *)prefixArguments
                           suffixArguments:(NSArray *)suffixArguments;

@end
