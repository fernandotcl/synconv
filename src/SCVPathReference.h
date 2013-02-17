/*
 * This file is part of synconv.
 *
 * © 2013 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#import <Foundation/Foundation.h>

@interface SCVPathReference : NSObject

@property (nonatomic, readonly) NSString *path;

+ (instancetype)pathReferenceWithPath:(NSString *)path;

- (id)initWithPath:(NSString *)path;

@end
