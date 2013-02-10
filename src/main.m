/*
 * This file is part of synconv.
 *
 * © 2013 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#import <Foundation/Foundation.h>

#import <getopt.h>
#import <stdlib.h>
#import <stdio.h>

#import "SCVConsole.h"
#import "SCVEncoder.h"
#import "SCVPluginManager.h"
#import "SCVWalker.h"

static void print_usage(FILE *fp)
{
    fprintf(fp, "\
Usage: \n\
    " PROGRAM_NAME " [options] <file or dir> [<file or dir>] [...] <output dir>\n\
    " PROGRAM_NAME " --help\n");
}

static int autorelease_main(int argc, char **argv)
{
    struct option long_options[] = {
        {"dont-copy-others", no_argument, NULL, 'C'},
        {"encoder-option", required_argument, NULL, 'E'},
        {"renaming-filter", required_argument, NULL, 'N'},
        {"output-extension", required_argument, NULL, 'O'},
        {"dont-recurse", no_argument, NULL, 'R'},
        {"dont-transcode", required_argument, NULL, 'T'},
        {"delete", no_argument, NULL, 'd' },
        {"encoder", required_argument, NULL, 'e' },
        {"help", no_argument, NULL, 'h'},
        {"dry-run", no_argument, NULL, 'n'},
        {"overwrite-mode", required_argument, NULL, 'o'},
        {"quiet", no_argument, NULL, 'q'},
        {"reencode", no_argument, NULL, 'r'},
        {"threads", required_argument, NULL, 't'},
        {"verbose", no_argument, NULL, 'v'},
        {NULL, 0, NULL, 0}
    };
    const char *options = "CE:N:O:RT:de:hno:qrt:v";

    SCVWalker *walker = [SCVWalker new];
    NSString *encoderName = nil;
    NSMutableArray *additionalEncoderOptions = [NSMutableArray new];
    NSMutableArray *noTranscodingExtensions = [NSMutableArray new];

    int opt;
    while ((opt = getopt_long(argc, argv, options, long_options, NULL)) != -1) {
        switch (opt) {
            case 'C':
                walker.copyOther = NO;
                break;
            case 'E':
                [additionalEncoderOptions addObject:[NSString stringWithCString:optarg encoding:NSUTF8StringEncoding]];
                break;
            case 'N':
                // TODO
                break;
            case 'O':
                walker.encoderExtension = [NSString stringWithCString:optarg encoding:NSUTF8StringEncoding];
                break;
            case 'R':
                walker.recursive = NO;
                break;
            case 'T': {
                NSString *extension = [NSString stringWithCString:optarg encoding:NSUTF8StringEncoding];
                [noTranscodingExtensions addObject:extension.lowercaseString];
                break;
            }
            case 'd':
                walker.deleteExtraneous = YES;
                break;
            case 'e':
                encoderName = [NSString stringWithCString:optarg encoding:NSUTF8StringEncoding];
                break;
            case 'h':
                print_usage(stdout);
                return EXIT_SUCCESS;
            case 'n':
                walker.dryRun = YES;
                break;
            case 'o': {
                NSString *mode = [NSString stringWithCString:optarg encoding:NSUTF8StringEncoding];
                if ([mode isEqualToString:@"auto"]) {
                    walker.overwriteMode = kSCVWalkerOverwriteModeAuto;
                } else if ([mode isEqualToString:@"always"]) {
                    walker.overwriteMode = kSCVWalkerOverwriteModeAlways;
                } else if ([mode isEqualToString:@"never"]) {
                    walker.overwriteMode = kSCVWalkerOverwriteModeNever;
                } else {
                    print_usage(stderr);
                    return EXIT_FAILURE;
                }
                break;
            }
            case 'q':
                walker.quiet = YES;
                break;
            case 'r':
                walker.reencode = YES;
                break;
            case 't': {
                NSString *numThreadsStr = [NSString stringWithCString:optarg encoding:NSUTF8StringEncoding];
                walker.numThreads = [numThreadsStr intValue];
                if (walker.numThreads <= 0) {
                    print_usage(stderr);
                    return EXIT_FAILURE;
                }
                break;
            }
            case 'v':
                walker.verbose = YES;
                break;
            default:
                print_usage(stderr);
                return EXIT_FAILURE;
        }
    }

    // We need at least an input and an output
    int numArgs = argc - optind;
    if (numArgs < 2) {
        print_usage(stderr);
        return EXIT_FAILURE;
    }

    NSString *outputDir = [NSString stringWithCString:argv[argc - 1] encoding:NSUTF8StringEncoding];
    
    NSFileManager *fm = [NSFileManager defaultManager];
    BOOL isDirectory;
    BOOL exists = [fm fileExistsAtPath:outputDir isDirectory:&isDirectory];

	// Make sure the last argument is a directory if it exists
    if (exists && !isDirectory) {
        SCVConsoleLogError(@"target `%@' is not a directory", outputDir);
        return EXIT_FAILURE;
    }

	// If multiple input paths are specified, the destination directory must exist
    if (numArgs > 2 && !exists) {
        SCVConsoleLogError(@"target `%@' is not a directory", outputDir);
        return EXIT_FAILURE;
	}

    NSMutableArray *inputPaths = [NSMutableArray arrayWithCapacity:numArgs - 1];
    for (int i = 0; i < numArgs - 1; ++i) {
        [inputPaths addObject:[NSString stringWithCString:argv[optind + i] encoding:NSUTF8StringEncoding]];
    }

	// Make sure the other arguments are existing files or directories
    for (NSString *path in inputPaths) {
        if (![fm fileExistsAtPath:path]) {
            SCVConsoleLogError(@"cannot stat `%@': No such file or directory", path);
            return EXIT_FAILURE;
        }
    }

    if (!encoderName) {
        encoderName = @"lame";
    }

    walker.encoder = (SCVPlugin <SCVEncoder, SCVPlugin> *)
        [[SCVPluginManager sharedInstance] pluginForEncodingWithName:encoderName];
    if (!walker.encoder) {
        SCVConsoleLogError(@"bad encoder: `%s'", encoderName.UTF8String);
        return EXIT_FAILURE;
    }

    if (!walker.encoderExtension) {
        walker.encoderExtension = [(id <SCVEncoder>)walker.encoder preferredEncoderExtension];
    }

    walker.noTranscodingExtensions = noTranscodingExtensions;

    [walker walk:inputPaths outputDir:outputDir];

    return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
    @autoreleasepool {
        return autorelease_main(argc, argv);
    }
}
