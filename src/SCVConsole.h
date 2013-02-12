/*
 * This file is part of synconv.
 *
 * © 2013 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#import <Foundation/Foundation.h>

@interface SCVConsole : NSObject

+ (instancetype)sharedInstance;

@end

void SCVConsolePrint(NSString *format, ...) __attribute__((format(__NSString__, 1, 2)));
void SCVConsolePrintError(NSString *format, ...) __attribute__((format(__NSString__, 1, 2)));

void SCVConsoleLog(NSString *format, ...) __attribute__((format(__NSString__, 1, 2)));
void SCVConsoleLogError(NSString *format, ...) __attribute__((format(__NSString__, 1, 2)));

void SCVConsoleFloat(NSString *format, ...) __attribute__((format(__NSString__, 1, 2)));
void SCVConsoleUnfloat(NSString *format, ...) __attribute__((format(__NSString__, 1, 2)));
