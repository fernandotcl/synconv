/*
 * This file is part of synconv.
 *
 * © 2013 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#import "SCVFlacPlugin.h"

@implementation SCVFlacPlugin

- (NSString *)pluginName
{
    return @"flac";
}

- (void)checkExecutables
{
    [self findPathForDecoderAndEncoderExecutable:@[@"flac"]];
}

- (NSArray *)decoderExtensions
{
    return @[@"flac"];
}

- (NSTask *)decoderTask
{
    return [self decoderTaskWithPrefixArguments:@[@"--decode", @"--stdout", @"--silent"]
                                suffixArguments:@[@"-"]];
}

- (NSString *)preferredEncoderExtension
{
    return @"flac";
}

- (NSTask *)encoderTask
{
    return [self encoderTaskWithPrefixArguments:@[@"--stdout", @"--no-seektable", @"--silent"]
                                suffixArguments:@[@"-"]];
}

@end
