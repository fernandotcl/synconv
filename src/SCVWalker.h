/*
 * This file is part of synconv.
 *
 * © 2013 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#import <Foundation/Foundation.h>

#import "SCVEncoder.h"
#import "SCVPlugin.h"
#import "SCVRenamingFilter.h"

typedef enum : unsigned char {
    kSCVWalkerOverwriteModeAuto,
    kSCVWalkerOverwriteModeAlways,
    kSCVWalkerOverwriteModeNever
} SCVWalkerOverwriteMode;

@interface SCVWalker : NSObject

- (void)walk:(NSArray *)inputPaths outputDir:(NSString *)outputDir;

@property (nonatomic, strong) SCVPlugin <SCVEncoder, SCVPlugin> *encoder;

@property (nonatomic, strong) NSString *encoderExtension;

@property (nonatomic, strong) SCVRenamingFilter *renamingFilter;

@property (nonatomic, assign) BOOL recursive;

@property (nonatomic, assign) BOOL reencode;

@property (nonatomic, assign) NSArray *noTranscodingExtensions;

@property (nonatomic, assign) SCVWalkerOverwriteMode overwriteMode;

@property (nonatomic, assign) BOOL copyOther;

@property (nonatomic, assign) BOOL deleteExtraneous;

@property (nonatomic, assign) BOOL dryRun;

@property (nonatomic, assign) int numThreads;

@property (nonatomic, assign) BOOL quiet;

@property (nonatomic, assign) BOOL verbose;

@end
