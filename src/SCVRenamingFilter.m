/*
 * This file is part of synconv.
 *
 * © 2013 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#import "SCVRenamingFilter.h"

@implementation SCVRenamingFilter

- (NSString *)renamedPathComponent:(NSString *)pathComponent
{
    NSCharacterSet *forbidden = [self allowedCharacterSet].invertedSet;
    NSArray *components = [pathComponent componentsSeparatedByCharactersInSet:forbidden];
    return [components componentsJoinedByString:[self replacementString]];
}

- (NSCharacterSet *)allowedCharacterSet
{
    [self doesNotRecognizeSelector:_cmd];
    return nil;
}

- (NSString *)replacementString
{
    [self doesNotRecognizeSelector:_cmd];
    return nil;
}

@end
