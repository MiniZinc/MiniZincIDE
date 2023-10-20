#import <Cocoa/Cocoa.h>
 
#include "darkmodenotifier.h"

#include <functional>

@interface DarkModeObserver : NSObject
@property (readonly, nonatomic) std::function<void()> handler;
- (instancetype)initWithHandler:(std::function<void()>)handler;
@end
 
@implementation DarkModeObserver
- (instancetype)initWithHandler:(std::function<void()>)handler
{
    if ((self = [super init])) {
        _handler = handler;
        if (@available(macOS 10.14, *)) {
            [NSApp addObserver:self
                    forKeyPath:@"effectiveAppearance"
                       options:0
                       context:nullptr
            ];
        }
    }
    return self;
}
 
- (void)dealloc
{
    if (@available(macOS 10.14, *)) {
        [NSApp removeObserver:self
               forKeyPath:@"effectiveAppearance"
        ];
    }
    [super dealloc];
}
 
- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary *)change
                       context:(void *)context
{
    self.handler();
}
@end

class DarkModeNotifier::Internal {
public:
    DarkModeObserver* darkModeObserver = nullptr;
};

void DarkModeNotifier::init()
{
    auto getDarkMode = [=] () {
        if (@available(macOS 10.14, *)) {
            id appObjects[] = { NSAppearanceNameAqua, NSAppearanceNameDarkAqua };
            NSArray* appearances = [NSArray arrayWithObjects: appObjects count:2];
            NSAppearanceName appearance = [[NSApp effectiveAppearance] bestMatchFromAppearancesWithNames: appearances];
            if ([appearance isEqualToString:NSAppearanceNameDarkAqua]) {
                return true;
            }
        }
        return false;
    };

    _internal = new Internal;

    _darkMode = getDarkMode();

    auto handler = [=] () {
        bool newDarkMode = getDarkMode();
        if (_darkMode != newDarkMode) {
            _darkMode = newDarkMode;
            emit darkModeChanged(newDarkMode);
        }
    };

    _internal->darkModeObserver = [[DarkModeObserver alloc] initWithHandler:handler];
}

DarkModeNotifier::~DarkModeNotifier()
{
    [_internal->darkModeObserver release];
    delete _internal;
}

bool DarkModeNotifier::hasSystemSetting() const
{
    if (@available(macOS 10.14, *)) {
        return true;
    }
    return false;
}

bool DarkModeNotifier::hasNativeDarkMode() const
{
    return hasSystemSetting();
}
