/*
 * This file is part of synconv.
 *
 * © 2013 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#import "SCVConservativeRenamingFilter.h"

@implementation SCVConservativeRenamingFilter

- (NSString *)renamedPathComponent:(NSString *)pathComponent
{
    NSData *data = [pathComponent dataUsingEncoding:NSASCIIStringEncoding allowLossyConversion:YES];
    NSString *converted = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    return [super renamedPathComponent:converted];
}

- (NSCharacterSet *)allowedCharacterSet
{
    NSMutableCharacterSet *allowed = [NSMutableCharacterSet alphanumericCharacterSet];
    [allowed addCharactersInString:@" %-_@~`!(){}^#&."];
    return allowed;
}

- (NSString *)replacementString
{
    return @"_";
}

@end
