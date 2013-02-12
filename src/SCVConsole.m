/*
 * This file is part of synconv.
 *
 * © 2013 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#import <stdio.h>
#import <unistd.h>

#import "SCVConsole.h"

@interface SCVFloatingLine : NSObject

@property (nonatomic, strong) NSString *content;
@property (nonatomic, strong) NSThread *thread;

@end

@implementation SCVFloatingLine

@end

@implementation SCVConsole {
    dispatch_queue_t _queue;
    NSMutableArray *_floatingLines;
    BOOL _printFloating;
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
        _floatingLines = [NSMutableArray new];
        _printFloating = isatty(fileno(stdout));
    }
    return self;
}

- (void)printString:(NSString *)str toFilePointer:(FILE *)fp unfloat:(BOOL)unfloat
{
    NSThread *thread = [NSThread currentThread];
    dispatch_sync(_queue, ^{
        [self clearFloatingLines];
        fprintf(fp, "%s\n", str.UTF8String);
        if (unfloat) {
            SCVFloatingLine *floatingLine = [self floatingLineForThread:thread];
            if (floatingLine) {
                [_floatingLines removeObject:floatingLine];
            }
        }
        [self drawFloatingLines];
        fflush(fp);
    });
}

- (void)printString:(NSString *)str toFilePointer:(FILE *)fp
{
    [self printString:str toFilePointer:fp unfloat:NO];
}

- (void)logString:(NSString *)str toFilePointer:(FILE *)fp
{
    dispatch_sync(_queue, ^{
        [self clearFloatingLines];
        fprintf(fp, PROGRAM_NAME ": %s\n", str.UTF8String);
        [self drawFloatingLines];
        fflush(fp);
    });
}

- (void)printFloatingLine:(NSString *)str
{
    NSThread *thread = [NSThread currentThread];
    dispatch_sync(_queue, ^{
        [self clearFloatingLines];
        SCVFloatingLine *floatingLine = [self floatingLineForThread:thread];
        if (!floatingLine) {
            floatingLine = [SCVFloatingLine new];
            floatingLine.thread = thread;
            [_floatingLines addObject:floatingLine];
        }
        floatingLine.content = str;
        [self drawFloatingLines];
        fflush(stdout);
    });
}

- (SCVFloatingLine *)floatingLineForThread:(NSThread *)thread
{
    for (SCVFloatingLine *line in _floatingLines) {
        if ([line.thread isEqualTo:thread]) {
            return line;
        }
    }
    return nil;
}

- (void)clearFloatingLines
{
    if (_printFloating) {
        for (SCVFloatingLine *line in _floatingLines) {
            printf("\033[F\033[J");
        }
    }
}

- (void)drawFloatingLines
{
    if (_printFloating) {
        for (SCVFloatingLine *line in _floatingLines) {
            printf("%s\n", line.content.UTF8String);
        }
    }
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

void SCVConsoleFloat(NSString *format, ...)
{
    EXTRACT_STR;
    [[SCVConsole sharedInstance] printFloatingLine:str];
}

void SCVConsoleUnfloat(NSString *format, ...)
{
    EXTRACT_STR;
    [[SCVConsole sharedInstance] printString:str toFilePointer:stdout unfloat:YES];
}

#undef EXTRACT_STR
