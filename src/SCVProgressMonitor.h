/*
 * This file is part of synconv.
 *
 * © 2013 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#import <Foundation/Foundation.h>

@interface SCVProgressMonitor : NSObject

@property (nonatomic, readonly) NSPipe *pipe;

+ (instancetype)monitorWithFileHandle:(NSFileHandle *)fileHandle
                             fileSize:(unsigned long long)fileSize
                        progressBlock:(void (^)(int))progressBlock;

- (id)initWithFileHandle:(NSFileHandle *)fileHandle
                fileSize:(unsigned long long)fileSize
           progressBlock:(void (^)(int))progressBlock;

@end
