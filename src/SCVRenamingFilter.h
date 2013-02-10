/*
 * This file is part of synconv.
 *
 * © 2013 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#import <Foundation/Foundation.h>

@interface SCVRenamingFilter : NSObject

- (NSString *)renamedPathComponent:(NSString *)pathComponent;

- (NSCharacterSet *)allowedCharacterSet;

- (NSString *)replacementString;

@end
