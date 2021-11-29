#import <Cocoa/Cocoa.h>
#include "macos_extras.h"

int isDark(void) {
    if (@available(macOS 10.14, *)) {
        id appObjects[] = { NSAppearanceNameAqua, NSAppearanceNameDarkAqua };
        NSArray* appearances = [NSArray arrayWithObjects: appObjects count:2];
        return [[[NSAppearance currentAppearance] bestMatchFromAppearancesWithNames: appearances] isEqualToString:NSAppearanceNameDarkAqua];
    } else {
        return false;
    }
}

int hasDarkMode(void) {
    if (@available(macOS 10.14, *)) {
        return true;
    } else {
        return false;
    }
}
