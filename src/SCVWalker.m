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
    NSMutableSet *_pathsToKeep;

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
        _pathsToKeep = [NSMutableSet setWithCapacity:100];
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
        self.numThreads = [NSProcessInfo processInfo].processorCount;
    }
    _transcoding_semaphore = dispatch_semaphore_create(self.numThreads);

    if (self.verbose) {
        SCVConsoleLog(@"using up to %d threads for transcoding", self.numThreads);
    }

    _baseOutputDir = [self absolutePathWithPath:outputDir];

    NSFileManager *fm = [NSFileManager defaultManager];
    for (__strong NSString *inputPath in inputPaths) {
        // Make sure the stack is clear
        [_stack removeAllObjects];

        inputPath = [self absolutePathWithPath:inputPath];

        BOOL isDirectory;
        [fm fileExistsAtPath:inputPath isDirectory:&isDirectory];
        if (isDirectory) {
            // If the last character of the input directory is the path separator
            // or if the output directory doesn't exist, copy its contents instead
            // (like the Unix cp command does)
            if (![inputPath hasSuffix:@"/"]) {
                [_stack addObject:inputPath.lastPathComponent];
                NSString *keep = [_baseOutputDir stringByAppendingPathComponent:inputPath.lastPathComponent];
                [_pathsToKeep addObject:keep];
            }

            // Walk the hierarchy
            [self walk:inputPath fileVisitor:^(NSString *path) {
                [self visitFile:path];
            } dirVisitor:^BOOL(NSString *path) {
                return [self visitDirectory:path];
            } dirVisitorAfter:^(NSString *path) {
                [self visitDirectoryAfter:path];
            }];
        }
        else {
            NSDictionary *attrs = [fm attributesOfItemAtPath:inputPath error:NULL];
            BOOL isRegularFile = [attrs[NSFileType] isEqualToString:NSFileTypeRegular];
            if (isRegularFile) {
                _currentOutputDir = _baseOutputDir;
                [self visitFile:inputPath];
            } else {
                SCVConsoleLogError(@"skipping `%@' (not a regular file or directory)",
                                   inputPath.lastPathComponent);
            }
        }
    }

    // Wait for the transcoding threads
    dispatch_group_wait(_transcoding_group, DISPATCH_TIME_FOREVER);

    // Delete extraneous files and directories
    if (self.deleteExtraneous) {
        NSMutableArray *dirsToDelete = [NSMutableArray array];

        [self walk:outputDir fileVisitor:^(NSString *path) {
            if ([_pathsToKeep containsObject:path]) {
                return;
            }

            if (!self.dryRun) {
                NSError *error;
                BOOL success = [[NSFileManager defaultManager] removeItemAtPath:path error:&error];
                if (!success) {
                    SCVConsoleLogError(@"failed to delete `%@': %@", path.lastPathComponent,
                                       error.localizedDescription);
                    return;
                }
            }

            if (!self.quiet) {
                SCVConsolePrint(@"Deleted `%@'", path.lastPathComponent);
            }
        } dirVisitor:^BOOL(NSString *path) {
            // We can't delete this directory yet because the
            // file visitor hasn't reached it yet
            if (![_pathsToKeep containsObject:path]) {
                [dirsToDelete addObject:path];
            }

            return YES;
        } dirVisitorAfter:nil];

        // Sort the paths by depth so deletion works (we must delete
        // the deepest paths before we can delete their parents)
        [dirsToDelete sortUsingComparator:^NSComparisonResult(id obj1, id obj2) {
            int num1 = ((NSString *)obj1).pathComponents.count;
            int num2 = ((NSString *)obj2).pathComponents.count;
            if (num1 < num2) {
                return NSOrderedDescending;
            } else if (num2 < num1) {
                return NSOrderedAscending;
            } else {
                return NSOrderedSame;
            }
        }];

        // Now we can delete those directores if they're not empty
        for (NSString *path in dirsToDelete) {
            NSError *error;
            if (!self.dryRun) {
                NSFileManager *fm = [NSFileManager defaultManager];
                NSArray *contents = [fm contentsOfDirectoryAtPath:path error:NULL];
                if (!contents.count) {
                    BOOL success = [[NSFileManager defaultManager] removeItemAtPath:path error:&error];
                    if (!success) {
                        SCVConsoleLogError(@"failed to delete `%@': %@", path.lastPathComponent,
                                           error.localizedDescription);
                        continue;
                    }
                }
            }

            if (!self.quiet) {
                SCVConsolePrint(@"Deleted `%@'", path.lastPathComponent);
            }
        }
    }

    if (self.dryRun) {
        SCVConsoleLog(@"finished running in dry-run mode, no actual changes made");
    }
}

- (NSString *)absolutePathWithPath:(NSString *)path
{
    if (![path hasPrefix:@"/"]) {
        NSString *cwd = [NSFileManager defaultManager].currentDirectoryPath;
        path = [cwd stringByAppendingPathComponent:path];
    }
    return [path stringByStandardizingPath];
}

- (void)walk:(NSString *)path
 fileVisitor:(void (^)(NSString *))fileVisitor
  dirVisitor:(BOOL (^)(NSString *))dirVisitor
dirVisitorAfter:(void (^)(NSString *))dirVisitorAfter
{
    NSArray *properties = @[NSURLIsRegularFileKey, NSURLIsDirectoryKey];

    NSFileManager *fm = [NSFileManager defaultManager];
    NSArray *children = [fm contentsOfDirectoryAtURL:[NSURL fileURLWithPath:path]
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
                if (dirVisitor(child.path)) {
                    [self walk:child.path fileVisitor:fileVisitor
                    dirVisitor:dirVisitor
               dirVisitorAfter:dirVisitorAfter];
                    if (dirVisitorAfter) {
                        dirVisitorAfter(child.path);
                    }
                }
            } else if (isRegularFile.boolValue) {
                fileVisitor(child.path);
            } else {
                SCVConsoleLogError(@"skipping `%@' (not a regular file or directory)",
                                   child.lastPathComponent);
            }
        }
    }
}

- (BOOL)visitDirectory:(NSString *)path
{
    if (!self.recursive) {
        SCVConsoleLog(@"skipping `%@' (recursion disabled)", path.lastPathComponent);
        return NO;
    }

    if (!self.quiet) {
        SCVConsolePrint(@"Entering `%@'", path.lastPathComponent);
    }

    // Apply the renaming filter
    NSString *outputDir = path.lastPathComponent;
    if (self.renamingFilter) {
        outputDir = [self.renamingFilter renamedPathComponent:outputDir];
    }

    // Push the directory into the stack
    [_stack addObject:outputDir];

    _currentOutputDir = _baseOutputDir;
    for (NSString *path in _stack) {
        _currentOutputDir = [_currentOutputDir stringByAppendingPathComponent:path];
    }

    _currentOutputDirCreated = self.dryRun;

    return YES;
}

- (void)visitDirectoryAfter:(NSString *)path
{
    if (!self.recursive) {
        // Pop the directory we had pushed
        [_stack removeLastObject];
    }
}

- (void)visitFile:(NSString *)inputPath
{
    if (_currentOutputDirCreationFailed) {
        return;
    }

    NSString *extension = inputPath.pathExtension.lowercaseString;
    SCVPlugin <SCVDecoder> *decoder = [[SCVPluginManager sharedInstance]
                                       pluginForDecodingFileWithExtension:extension];
    BOOL transcoding = decoder && (self.reencode || ![decoder isEqualTo:self.encoder]);

    if (transcoding && self.noTranscodingExtensions) {
        if ([self.noTranscodingExtensions indexOfObject:extension] != NSNotFound) {
            transcoding = NO;
        }
    }

    // Save the original file's attributes for later use
    NSError *error;
    NSFileManager *fm = [NSFileManager defaultManager];
    NSDictionary *inputAttrs = [fm attributesOfItemAtPath:inputPath error:&error];
    if (!inputAttrs) {
        SCVConsoleLogError(@"failed to stat `%@': %@", inputPath.lastPathComponent,
                           error.localizedDescription);
    }

    NSString *outputFilename = inputPath.lastPathComponent;
    NSString *outputPath = [_currentOutputDir stringByAppendingPathComponent:outputFilename];
    if ([outputPath isEqualToString:inputPath]) {
        SCVConsoleLogError(@"skipping `%@' (input and output files are the same)",
                           inputPath.lastPathComponent);
        return;
    }

    // Apply the renaming filter
    if (self.renamingFilter) {
        outputFilename = [self.renamingFilter renamedPathComponent:outputFilename];
    }

    if (transcoding) {
        outputFilename = [outputFilename stringByDeletingPathExtension];
        outputFilename = [outputFilename stringByAppendingPathExtension:self.encoderExtension];
    }
    outputPath = [_currentOutputDir stringByAppendingPathComponent:outputFilename];

    [_pathsToKeep addObject:_currentOutputDir];
    [_pathsToKeep addObject:outputPath];

    if (self.overwriteMode != kSCVWalkerOverwriteModeAlways) {
        NSDictionary *outputAttrs = [fm attributesOfItemAtPath:outputPath error:NULL];
        if (outputAttrs) {
            // If we're not overwriting, just skip it
            if (self.overwriteMode == kSCVWalkerOverwriteModeNever) {
                if (self.verbose) {
                    SCVConsoleLog(@"skipping `%@' (not overwriting)", outputFilename);
                }
                return;
            }

            // If overwrite mode is "auto", check timestamps
            NSDate *inputDate = inputAttrs[NSFileModificationDate];
            NSDate *outputDate = inputAttrs[NSFileModificationDate];
            if ([outputDate isGreaterThanOrEqualTo:inputDate]) {
                if (self.verbose) {
                    SCVConsoleLog(@"skipping `%@' (up-to-date)", outputFilename);
                }
                return;
            }
        }
    }

    if (!transcoding) {
        if (!self.copyOther) {
            if (self.verbose) {
                SCVConsoleLog(@"skipping `%@'", outputFilename);
            }
            return;
        }

        if (!_currentOutputDirCreated) {
            [self createOutputDir];
        }

        if (!self.dryRun) {
            [fm removeItemAtPath:outputPath error:NULL];
            if (![fm copyItemAtPath:inputPath toPath:outputPath error:&error]) {
                SCVConsoleLogError(@"failed to copy `%@': %@", outputFilename,
                                   error.localizedDescription);
                return;
            }

            [self restoreModificationDate:inputAttrs[NSFileModificationDate] forPath:outputPath];
        }

        if (!self.quiet) {
            SCVConsolePrint(@"Copied `%@'", outputFilename);
        }

        return;
    }

    if (!_currentOutputDirCreated) {
        [self createOutputDir];
    }

    dispatch_semaphore_wait(_transcoding_semaphore, DISPATCH_TIME_FOREVER);

    dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    dispatch_group_async(_transcoding_group, queue, ^{
        if (!self.dryRun) {
            BOOL success = [self runTranscodingPipelineForInputPath:inputPath
                                                         inputAttrs:inputAttrs
                                                            decoder:decoder
                                                         outputPath:outputPath];

            if (!success) {
                dispatch_semaphore_signal(_transcoding_semaphore);
                return;
            }
        }

        if (!self.quiet) {
            NSString *action = [decoder isEqualTo:self.encoder] ? @"Re-encoded" : @"Transcoded";
            SCVConsolePrint(@"%@ `%@'", action, outputFilename);
        }

        dispatch_semaphore_signal(_transcoding_semaphore);
    });
}

- (BOOL)runTranscodingPipelineForInputPath:(NSString *)inputPath
                                inputAttrs:(NSDictionary *)inputAttrs
                                   decoder:(SCVPlugin <SCVDecoder> *)decoder
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
