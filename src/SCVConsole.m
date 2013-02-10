/*
 * This file is part of synconv.
 *
 * © 2013 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#import <stdio.h>

#import "SCVConsole.h"

@implementation SCVConsole {
    dispatch_queue_t _queue;
}

+ (instancetype)sharedInstance
{
    static SCVConsole *instance;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        instance = [SCVConsole new];
    });
    return instance;
}

- (id)init
{
    self = [super init];
    if (self) {
        _queue = dispatch_queue_create("com.fernandotcl.synconv.SCVConsole", NULL);
    }
    return self;
}

- (void)printString:(NSString *)str toFilePointer:(FILE *)fp
{
    dispatch_sync(_queue, ^{
        fprintf(fp, "%s\n", str.UTF8String);
    });
}

- (void)logString:(NSString *)str toFilePointer:(FILE *)fp
{
    dispatch_sync(_queue, ^{
        fprintf(fp, PROGRAM_NAME ": %s\n", str.UTF8String);
    });
}

@end

#define EXTRACT_STR \
    va_list args; \
    va_start(args, format); \
    NSString *str = [[NSString alloc] initWithFormat:format arguments:args]; \
    va_end(args) \

void SCVConsolePrint(NSString *format, ...)
{
    EXTRACT_STR;
    [[SCVConsole sharedInstance] printString:str toFilePointer:stdout];
}

void SCVConsolePrintError(NSString *format, ...)
{
    EXTRACT_STR;
   [[SCVConsole sharedInstance] printString:str toFilePointer:stdout];
}

void SCVConsoleLog(NSString *format, ...)
{
    EXTRACT_STR;
    [[SCVConsole sharedInstance] logString:str toFilePointer:stdout];
}

void SCVConsoleLogError(NSString *format, ...)
{
    EXTRACT_STR;
    [[SCVConsole sharedInstance] logString:str toFilePointer:stdout];
}

#undef EXTRACT_STR
