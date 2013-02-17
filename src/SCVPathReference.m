/*
 * This file is part of synconv.
 *
 * © 2013 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#import "SCVPathReference.h"

@interface SCVPathReferenceCache : NSObject

+ (instancetype)sharedInstance;

- (BOOL)isPathCaseSensitive:(NSString *)path;

@end

@implementation SCVPathReferenceCache {
    NSMutableDictionary *_cache;
}

+ (instancetype)sharedInstance
{
    static SCVPathReferenceCache *instance;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        instance = [SCVPathReferenceCache new];
    });
    return instance;
}

- (id)init
{
    self = [super init];
    if (self) {
        _cache = [NSMutableDictionary dictionaryWithCapacity:50];
    }
    return self;
}

- (BOOL)isPathCaseSensitive:(NSString *)path
{
    NSNumber *cacheHit = _cache[path];
    if (cacheHit) {
        return cacheHit.boolValue;
    }

    NSNumber *caseSensitive;
    NSURL *url = [NSURL fileURLWithPath:path];
    [url getResourceValue:&caseSensitive
                   forKey:NSURLVolumeSupportsCaseSensitiveNamesKey
                    error:NULL];

    if (caseSensitive) {
        _cache[path] = caseSensitive;
    }

    return caseSensitive.boolValue;
}

@end

#pragma mark -

@interface SCVPathReference ()

@property (nonatomic, strong) NSString *path;

@end

@implementation SCVPathReference

+ (instancetype)pathReferenceWithPath:(NSString *)path
{
    return [[SCVPathReference alloc] initWithPath:path];
}

- (id)initWithPath:(NSString *)path
{
    self = [super init];
    if (self) {
        self.path = [self pathWithCaseInsensitiveComponentsLowercased:path];
    }
    return self;
}

- (NSString *)pathWithCaseInsensitiveComponentsLowercased:(NSString *)path
{
    NSString *result = @"";
    NSString *accumulatedPath = @"";
    SCVPathReferenceCache *cache = [SCVPathReferenceCache sharedInstance];

    for (NSString *pathComponent in path.pathComponents) {
        @autoreleasepool {
            accumulatedPath = [accumulatedPath stringByAppendingPathComponent:pathComponent];
            if ([cache isPathCaseSensitive:accumulatedPath]) {
                result = [result stringByAppendingPathComponent:pathComponent];
            } else {
                result = [result stringByAppendingPathComponent:pathComponent.lowercaseString];
            }
        }
    }

    return result;
}

- (BOOL)isEqual:(id)anObject
{
    if ([anObject isKindOfClass:[SCVPathReference class]]) {
        return [self isEqualToPathReference:anObject];
    } else {
        return [super isEqual:anObject];
    }
}

- (BOOL)isEqualToPathReference:(SCVPathReference *)pathReference
{
    return [self.path isEqualToString:pathReference.path];
}

- (NSUInteger)hash
{
    return self.path.hash;
}

@end
