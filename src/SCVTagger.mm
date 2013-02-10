/*
 * This file is part of synconv.
 *
 * © 2013 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#import <taglib/fileref.h>
#import <taglib/tag.h>

#import "SCVTagger.h"

@implementation SCVTagger

+ (instancetype)sharedInstance
{
    static SCVTagger *instance;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        instance = [SCVTagger new];
    });
    return instance;
}

- (void)copyTagsFromInputPath:(NSString *)inputPath toOutputPath:(NSString *)outputPath
{
    TagLib::FileRef inputTags(inputPath.UTF8String);
    TagLib::FileRef outputTags(outputPath.UTF8String);
    TagLib::Tag::duplicate(inputTags.tag(), outputTags.tag());
    outputTags.save();
}

@end
