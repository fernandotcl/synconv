/*
 * This file is part of synconv.
 *
 * © 2013 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#import "SCVConsole.h"
#import "SCVDecoder.h"
#import "SCVEncoder.h"
#import "SCVPlugin.h"

@implementation SCVPlugin {
    NSString *_decoderPath, *_encoderPath;
}

- (id)init
{
    self = [super init];
    if (self) {
        self.enabledForEncoding = [self conformsToProtocol:@protocol(SCVEncoder)];
        self.enabledForDecoding = [self conformsToProtocol:@protocol(SCVDecoder)];
    }
    return self;
}

- (NSString *)pluginName
{
    [self doesNotRecognizeSelector:_cmd];
    return nil;
}

- (NSArray *)pluginAliases
{
    return @[[self pluginName]];
}

- (void)checkExecutables
{
    [self doesNotRecognizeSelector:_cmd];
}

- (void)findPathForDecoderExecutable:(NSArray *)executableNames
{
    _decoderPath = [self pathForExecutable:executableNames];
    if (!_decoderPath) {
        NSString *name = [self pluginName];
        SCVConsoleLogError(@"plugin `%@' disabled for decoding (executable not found)", name);
        self.enabledForDecoding = NO;
    }
}

- (void)findPathForEncoderExecutable:(NSArray *)executableNames
{
    _encoderPath = [self pathForExecutable:executableNames];
    if (!_encoderPath) {
        NSString *name = [self pluginName];
        SCVConsoleLogError(@"plugin `%@' disabled for encoding (executable not found)", name);
        self.enabledForEncoding = NO;
    }
}

- (void)findPathForDecoderAndEncoderExecutable:(NSArray *)executableNames
{
    _decoderPath = [self pathForExecutable:executableNames];
    _encoderPath = _decoderPath;
    if (!_decoderPath) {
        NSString *name = [self pluginName];
        SCVConsoleLogError(@"plugin `%@' disabled (executable not found)", name);
        self.enabledForDecoding = NO;
        self.enabledForEncoding = NO;
    }
}

- (NSString *)pathForExecutable:(NSArray *)executableNames
{
    static NSArray *envPath;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        envPath = [[NSProcessInfo processInfo].environment[@"PATH"] componentsSeparatedByString:@":"];
    });

    NSFileManager *fm = [NSFileManager defaultManager];
    for (NSString *executable in executableNames) {
        for (NSString *baseDir in envPath) {
            NSString *fullPath = [baseDir stringByAppendingPathComponent:executable];
            if ([fm isExecutableFileAtPath:fullPath]) {
                return fullPath;
            }
        }
    }
    return nil;
}

- (NSTask *)taskWithExecutable:(NSString *)executable
               prefixArguments:(NSArray *)prefixArguments
               suffixArguments:(NSArray *)suffixArguments
{
    NSUInteger totalCount = prefixArguments.count + self.additionalEncoderFlags.count + suffixArguments.count;
    NSMutableArray *args = [NSMutableArray arrayWithCapacity:totalCount];
    if (prefixArguments) {
        [args addObjectsFromArray:prefixArguments];
    }
    if (self.additionalEncoderFlags) {
        [args addObjectsFromArray:self.additionalEncoderFlags];
    }
    if (suffixArguments) {
        [args addObjectsFromArray:suffixArguments];
    }

    NSTask *task = [NSTask new];
    task.launchPath = executable;
    task.arguments = args;
    return task;
}

- (NSTask *)decoderTaskWithPrefixArguments:(NSArray *)prefixArguments
                           suffixArguments:(NSArray *)suffixArguments
{
    return [self taskWithExecutable:_decoderPath
                    prefixArguments:prefixArguments
                    suffixArguments:suffixArguments];
}

- (NSTask *)encoderTaskWithPrefixArguments:(NSArray *)prefixArguments
                           suffixArguments:(NSArray *)suffixArguments
{
    return [self taskWithExecutable:_encoderPath
                    prefixArguments:prefixArguments
                    suffixArguments:suffixArguments];
}

@end
