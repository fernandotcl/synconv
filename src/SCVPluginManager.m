/*
 * This file is part of synconv.
 *
 * © 2013 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#import "SCVFlacPlugin.h"
#import "SCVLamePlugin.h"
#import "SCVVorbisPlugin.h"
#import "SCVPluginManager.h"

@implementation SCVPluginManager {
    NSArray *_plugins;
}

+ (instancetype)sharedInstance
{
    static SCVPluginManager *instance;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        instance = [SCVPluginManager new];
    });
    return instance;
}

- (id)init
{
    self = [super init];
    if (self) {
        _plugins = @[
            [SCVFlacPlugin new],
            [SCVLamePlugin new],
            [SCVVorbisPlugin new]
        ];
        for (SCVPlugin *plugin in _plugins) {
            [plugin checkExecutables];
        }
    }
    return self;
}

- (SCVPlugin <SCVEncoder> *)pluginForEncodingWithName:(NSString *)name
{
    name = name.lowercaseString;
    for (SCVPlugin *plugin in _plugins) {
        if (plugin.enabledForEncoding) {
            for (NSString *alias in [plugin pluginAliases]) {
                if ([name isEqualToString:alias]) {
                    return (SCVPlugin <SCVEncoder> *)plugin;
                }
            }
        }
    }
    return nil;
}

- (SCVPlugin <SCVDecoder> *)pluginForDecodingFileWithExtension:(NSString *)extension
{
    for (SCVPlugin *plugin in _plugins) {
        if (plugin.enabledForDecoding) {
            NSArray *extensions = [(SCVPlugin <SCVDecoder> *)plugin decoderExtensions];
            if ([extensions indexOfObject:extension] != NSNotFound) {
                return (SCVPlugin <SCVDecoder> *) plugin;
            }
        }
    }
    return nil;
}

@end
