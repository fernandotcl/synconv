/*
 * This file is part of synconv.
 *
 * © 2013 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#import <stdlib.h>

#import "SCVConsole.h"
#import "SCVDecoder.h"
#import "SCVPluginManager.h"
#import "SCVTagger.h"
#import "SCVWalker.h"

@implementation SCVWalker {
    NSMutableArray *_stack;

    NSString *_baseOutputDir;
    NSString *_currentOutputDir;

    BOOL _currentOutputDirCreated;
    BOOL _currentOutputDirCreationFailed;

    dispatch_group_t _transcoding_group;
    dispatch_semaphore_t _transcoding_semaphore;
}

- (id)init {
    self = [super init];
    if (self) {
        _stack = [NSMutableArray arrayWithCapacity:5];
        _transcoding_group = dispatch_group_create();

        self.recursive = YES;
        self.overwriteMode = kSCVWalkerOverwriteModeAuto;
        self.copyOther = YES;
        self.numThreads = 0;
    }
    return self;
}

- (void)walk:(NSArray *)inputPaths outputDir:(NSString *)outputDir
{
    // Find out how many threads we'll use. Transcoding is I/O bound as
    // well as CPU bound, so GCD might launch a lot of threads that will
    // make things even worse if we're blocked for I/O
    if (!self.numThreads) {
        self.numThreads = [NSProcessInfo processInfo].processorCount * 2;
    }
    _transcoding_semaphore = dispatch_semaphore_create(self.numThreads);

    _baseOutputDir = outputDir;

    NSFileManager *fm = [NSFileManager defaultManager];
    for (NSString *inputPath in inputPaths) {
        // Make sure the stack is clear
        [_stack removeAllObjects];

        BOOL isDirectory;
        [fm fileExistsAtPath:inputPath isDirectory:&isDirectory];
        if (isDirectory) {
            // If the last character of the input directory is the path separator
            // or if the output directory doesn't exist, copy its contents instead
            // (like the Unix cp command does)
            if (![inputPath hasSuffix:@"/"])
                [_stack addObject:inputPath.lastPathComponent];

            // Walk the hierarchy
            [self walk:[NSURL fileURLWithPath:inputPath] fileVisitor:^(NSURL *url) {
                [self visitFile:url];
            } dirVisitor:^BOOL(NSURL *url) {
                return [self visitDirectory:url];
            }];
        }
        else {
            NSDictionary *attrs = [fm attributesOfItemAtPath:inputPath error:NULL];
            BOOL isRegularFile = [attrs[NSFileType] isEqualToString:NSFileTypeRegular];
            if (isRegularFile) {
                _currentOutputDir = _baseOutputDir;
                [self visitFile:[NSURL fileURLWithPath:inputPath]];
            } else {
                SCVConsoleLogError(@"skipping `%@' (not a regular file or directory)",
                                   inputPath.lastPathComponent);
            }
        }
    }

    // Wait for the transcoding threads
    dispatch_group_wait(_transcoding_group, DISPATCH_TIME_FOREVER);
}

- (void)walk:(NSURL *)url
 fileVisitor:(void (^)(NSURL *))fileVisitor
  dirVisitor:(BOOL (^)(NSURL *))dirVisitor
{
    NSArray *properties = @[NSURLIsRegularFileKey, NSURLIsDirectoryKey];

    NSFileManager *fm = [NSFileManager defaultManager];
    NSArray *children = [fm contentsOfDirectoryAtURL:url
                          includingPropertiesForKeys:properties
                                             options:0
                                               error:NULL];

    children = [children sortedArrayUsingComparator:^NSComparisonResult(id obj1, id obj2) {
        return [((NSURL *)obj1).path.lastPathComponent compare:((NSURL *)obj2).path.lastPathComponent];
    }];

    for (NSURL *child in children) {
        @autoreleasepool {
            NSNumber *isRegularFile, *isDirectory;
            [child getResourceValue:&isRegularFile forKey:NSURLIsRegularFileKey error:NULL];
            [child getResourceValue:&isDirectory forKey:NSURLIsDirectoryKey error:NULL];
            if (isDirectory.boolValue) {
                // Push the child into the stack, visit, then pop it
                [_stack addObject:child.lastPathComponent];
                if (dirVisitor(child)) {
                    [self walk:child fileVisitor:fileVisitor dirVisitor:dirVisitor];
                }
                [_stack removeLastObject];
            } else if (isRegularFile.boolValue) {
                fileVisitor(child);
            } else {
                SCVConsoleLogError(@"skipping `%@' (not a regular file or directory)",
                                   child.lastPathComponent);
            }
        }
    }
}

- (BOOL)visitDirectory:(NSURL *)url
{
    if (!self.recursive) {
        SCVConsoleLog(@"skipping `%@' (recursion disabled)", url.lastPathComponent);
        return NO;
    }

    if (!self.quiet) {
        SCVConsolePrint(@"Entering `%@'", url.lastPathComponent);
    }

    _currentOutputDir = _baseOutputDir;
    for (NSString *path in _stack)
        _currentOutputDir = [_currentOutputDir stringByAppendingPathComponent:path];

    _currentOutputDirCreated = self.dryRun;

    return YES;
}

- (void)visitFile:(NSURL *)url
{
    if (_currentOutputDirCreationFailed) {
        return;
    }

    NSString *extension = url.pathExtension.lowercaseString;
    SCVPlugin <SCVDecoder, SCVPlugin> *decoder = (SCVPlugin <SCVDecoder, SCVPlugin> *)
        [[SCVPluginManager sharedInstance] pluginForDecodingFileWithExtension:extension];
    BOOL transcoding = decoder && (self.reencode || ![decoder isEqualTo:self.encoder]);

    // Save the original file's attributes for later use
    NSError *error;
    NSFileManager *fm = [NSFileManager defaultManager];
    NSDictionary *inputAttrs = [fm attributesOfItemAtPath:url.path error:&error];
    if (!inputAttrs) {
        SCVConsoleLogError(@"failed to stat `%@': %@", url.lastPathComponent,
                           error.localizedDescription);
    }

    NSString *outputPath = [_currentOutputDir stringByAppendingPathComponent:url.lastPathComponent];
    if (transcoding) {
        outputPath = [outputPath stringByDeletingPathExtension];
        outputPath = [outputPath stringByAppendingPathExtension:self.encoderExtension];
    }

    if (self.overwriteMode != kSCVWalkerOverwriteModeAlways) {
        NSDictionary *outputAttrs = [fm attributesOfItemAtPath:outputPath error:NULL];
        if (outputAttrs) {
            // If we're not overwriting, just skip it
            if (self.overwriteMode == kSCVWalkerOverwriteModeNever) {
                if (self.verbose) {
                    SCVConsoleLog(@"skipping `%@' (not overwriting)", outputPath.lastPathComponent);
                }
                return;
            }

            // If overwrite mode is "auto", check timestamps
            NSDate *inputDate = inputAttrs[NSFileModificationDate];
            NSDate *outputDate = inputAttrs[NSFileModificationDate];
            if ([outputDate isGreaterThanOrEqualTo:inputDate]) {
                if (self.verbose) {
                    SCVConsoleLog(@"skipping `%@' (up-to-date)", outputPath.lastPathComponent);
                }
                return;
            }
        }
    }

    if (!transcoding) {
        if (!self.copyOther) {
            if (self.verbose) {
                SCVConsoleLog(@"skipping `%@'", outputPath.lastPathComponent);
            }
            return;
        }

        if (!_currentOutputDir) {
            [self createOutputDir];
        }

        if (!self.dryRun) {
            [fm removeItemAtPath:outputPath error:NULL];
            if (![fm copyItemAtPath:url.path toPath:outputPath error:&error]) {
                SCVConsoleLogError(@"failed to copy `%@': %@", outputPath.lastPathComponent,
                                   error.localizedDescription);
                return;
            }

            [self restoreModificationDate:inputAttrs[NSFileModificationDate] forPath:outputPath];
        }

        if (!self.quiet) {
            SCVConsolePrint(@"Copied `%@'", outputPath.lastPathComponent);
        }

        return;
    }

    if (!_currentOutputDirCreated) {
        [self createOutputDir];
    }

    dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    dispatch_group_async(_transcoding_group, queue, ^{
        if (!self.dryRun) {
            dispatch_semaphore_wait(_transcoding_semaphore, DISPATCH_TIME_FOREVER);
            BOOL success = [self runTranscodingPipelineForInputPath:url.path
                                                         inputAttrs:inputAttrs
                                                            decoder:decoder
                                                         outputPath:outputPath];
            dispatch_semaphore_signal(_transcoding_semaphore);

            if (!success) {
                return;
            }
        }

        if (!self.quiet) {
            NSString *action = [decoder isEqualTo:self.encoder] ? @"Re-encoded" : @"Transcoded";
            SCVConsolePrint(@"%@ `%@'", action, outputPath.lastPathComponent);
        }
    });
}

- (BOOL)runTranscodingPipelineForInputPath:(NSString *)inputPath
                                inputAttrs:(NSDictionary *)inputAttrs
                                   decoder:(SCVPlugin <SCVPlugin, SCVDecoder> *)decoder
                                outputPath:(NSString *)outputPath
{
    NSFileHandle *inputHandle = [NSFileHandle fileHandleForReadingAtPath:inputPath];
    if (!inputHandle) {
        SCVConsoleLogError(@"failed to open `%@' for reading", inputPath.lastPathComponent);
        return NO;
    }

    NSFileManager *fm = [NSFileManager defaultManager];
    if (![fm createFileAtPath:outputPath contents:nil attributes:nil]) {
        SCVConsoleLogError(@"failed to create `%@'", outputPath.lastPathComponent);
        return NO;
    }

    NSFileHandle *outputHandle = [NSFileHandle fileHandleForWritingAtPath:outputPath];
    if (!outputHandle) {
        SCVConsoleLogError(@"failed to open `%@' for writing", outputPath.lastPathComponent);
        [fm removeItemAtPath:outputPath error:NULL];
        return NO;
    }

    NSTask *decoderTask = [decoder decoderTask];
    decoderTask.standardInput = inputHandle;
    decoderTask.standardOutput = [NSPipe pipe];

    NSTask *encoderTask = [self.encoder encoderTask];
    encoderTask.standardInput = decoderTask.standardOutput;
    encoderTask.standardOutput = outputHandle;

    [decoderTask launch];
    [encoderTask launch];

    [decoderTask waitUntilExit];
    [encoderTask waitUntilExit];

    BOOL error = YES;
    if (decoderTask.terminationReason == NSTaskTerminationReasonUncaughtSignal) {
        SCVConsoleLogError(@"decoder process terminated abnormally");
    } else if (encoderTask.terminationReason == NSTaskTerminationReasonUncaughtSignal) {
        SCVConsoleLogError(@"encoder process terminated abnormally");
    } else if (decoderTask.terminationStatus != EXIT_SUCCESS) {
        SCVConsoleLogError(@"error launching decoder");
    } else if (encoderTask.terminationStatus != EXIT_SUCCESS) {
        SCVConsoleLogError(@"error launching encoder");
    } else {
        error = NO;
    }

    if (error) {
        [fm removeItemAtPath:outputPath error:NULL];
        return NO;
    }

    [[SCVTagger sharedInstance] copyTagsFromInputPath:inputPath toOutputPath:outputPath];

    [self restoreModificationDate:inputAttrs[NSFileModificationDate] forPath:outputPath];

    return YES;
}

- (void)createOutputDir
{
    if (self.dryRun) {
        _currentOutputDirCreated = YES;
        return;
    }

    NSError *error;
    NSFileManager *fm = [NSFileManager defaultManager];
    _currentOutputDirCreationFailed = ![fm createDirectoryAtPath:_currentOutputDir
                                     withIntermediateDirectories:YES
                                                      attributes:nil
                                                           error:&error];
    if (_currentOutputDirCreationFailed) {
        SCVConsoleLogError(@"failed to create `%@': %@", _currentOutputDir,
error.localizedDescription);
        return;
    }
}

- (void)restoreModificationDate:(NSDate *)modificationDate forPath:(NSString *)path
{
    NSError *error;
    NSDictionary *attrs = @{NSFileModificationDate : modificationDate};
    NSFileManager *fm = [NSFileManager defaultManager];
    if (![fm setAttributes:attrs ofItemAtPath:path error:&error]) {
        SCVConsoleLogError(@"failed to set modification date for `%@': %@",
                           _currentOutputDir, error.localizedDescription);
    }
}

@end
