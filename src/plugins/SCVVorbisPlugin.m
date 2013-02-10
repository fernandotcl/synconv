/*
 * This file is part of synconv.
 *
 * © 2013 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#import "SCVVorbisPlugin.h"

@implementation SCVVorbisPlugin

- (NSString *)pluginName
{
    return @"vorbis";
}

- (void)checkExecutables
{
    [self findPathForDecoderExecutable:@[@"oggdec"]];
    [self findPathForEncoderExecutable:@[@"oggenc"]];
}

- (NSArray *)decoderExtensions
{
    return @[@"ogg", @"oga"];
}

- (NSTask *)decoderTask
{
    return [self decoderTaskWithPrefixArguments:@[@"--output=-", @"--quiet"]
                                suffixArguments:@[@"-"]];
}

- (NSString *)preferredEncoderExtension
{
    return @"ogg";
}

- (NSTask *)encoderTask
{
    return [self encoderTaskWithPrefixArguments:@[@"--raw", @"--output=-", @"--quiet"]
                                suffixArguments:@[@"-"]];
}

@end
