/*
 * This file is part of synconv.
 *
 * © 2013 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#import "SCVLamePlugin.h"

@implementation SCVLamePlugin

- (NSString *)pluginName
{
    return @"lame";
}

- (NSArray *)pluginAliases
{
    return @[@"lame", @"mp3"];
}

- (void)checkExecutables
{
    [self findPathForDecoderAndEncoderExecutable:@[@"lame"]];
}

- (NSArray *)decoderExtensions
{
    return @[@"mp3"];
}

- (NSTask *)decoderTask
{
    return [self decoderTaskWithPrefixArguments:@[@"--decode", @"--mp3input", @"--quiet"]
                                suffixArguments:@[@"-", @"-"]];
}

- (NSString *)preferredEncoderExtension
{
    return @"mp3";
}

- (NSTask *)encoderTask
{
    return [self encoderTaskWithPrefixArguments:@[@"--quiet"]
                                suffixArguments:@[@"-", @"-"]];
}

@end
