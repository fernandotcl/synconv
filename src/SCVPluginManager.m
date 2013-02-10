/*
 * This file is part of synconv.
 *
 * © 2013 Fernando Tarlá Cardoso Lemos
 *
 * Refer to the LICENSE file for licensing information.
 *
 */

#import "SCVLamePlugin.h"
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
        _plugins = @[[SCVLamePlugin new]];
        for (id <SCVPlugin> plugin in _plugins) {
            [plugin checkExecutables];
        }
    }
    return self;
}

- (SCVPlugin <SCVPlugin> *)pluginForEncodingWithName:(NSString *)name
{
    name = name.lowercaseString;
    for (SCVPlugin <SCVPlugin> *plugin in _plugins) {
        if (plugin.enabledForEncoding) {
            for (NSString *alias in [plugin pluginAliases]) {
                if ([name isEqualToString:alias]) {
                    return plugin;
                }
            }
        }
    }
    return nil;
}

- (SCVPlugin <SCVPlugin> *)pluginForDecodingFileWithExtension:(NSString *)extension
{
    for (SCVPlugin <SCVPlugin> *plugin in _plugins) {
        if (plugin.enabledForDecoding) {
            NSArray *extensions = [(SCVPlugin <SCVDecoder> *)plugin decoderExtensions];
            if ([extensions indexOfObject:extension] != NSNotFound)
                return plugin;
        }
    }
    return nil;
}

@end
