/*
 * This file is part of synconv.
 *
 * © 2013 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#import "SCVProgressMonitor.h"

@interface SCVProgressMonitor ()

@property (nonatomic, strong) NSPipe *pipe;

@end

@implementation SCVProgressMonitor {
    NSFileHandle *_fileHandle;

    unsigned long long _fileSize;
    unsigned long long _bytesRead;

    int _progress;
    void (^_progressBlock)(int);
}

+ (instancetype)monitorWithFileHandle:(NSFileHandle *)fileHandle
                             fileSize:(unsigned long long)fileSize
                        progressBlock:(void (^)(int))progressBlock
{
    return [[SCVProgressMonitor alloc] initWithFileHandle:fileHandle
                                                 fileSize:fileSize
                                            progressBlock:progressBlock];
}

- (id)initWithFileHandle:(NSFileHandle *)fileHandle
                fileSize:(unsigned long long)fileSize
           progressBlock:(void (^)(int))progressBlock
{
    self = [super init];
    if (self) {
        _fileHandle = fileHandle;
        _fileSize = fileSize;
        _progressBlock = progressBlock;

        self.pipe = [NSPipe pipe];

        NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
        [nc addObserver:self
               selector:@selector(fileHandleReadCompletion:)
                   name:NSFileHandleReadCompletionNotification
                 object:fileHandle];

        [fileHandle readInBackgroundAndNotify];
    }
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)fileHandleReadCompletion:(NSNotification *)notification
{
    NSData *data = notification.userInfo[NSFileHandleNotificationDataItem];
    if (!data.length) {
        [self.pipe.fileHandleForWriting closeFile];
        return;
    }

    [self.pipe.fileHandleForWriting writeData:data];
    _bytesRead += data.length;

    int newProgress = 100.0f * _bytesRead / _fileSize;
    if (newProgress > 99) {
        newProgress = 99;
    }
    if (newProgress != _progress) {
        _progress = newProgress;
        _progressBlock(newProgress);
    }

    [_fileHandle readInBackgroundAndNotify];
}

@end
