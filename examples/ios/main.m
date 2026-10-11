//
//  main.m
//  MiniFB
//
//  Created by Carlos Aragones on 22/04/2020.
//  Copyright © 2020 Carlos Aragones. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "AppDelegate.h"

#if defined(MINIFB_USE_MMGR)
    #include "mmgr.h"
#endif

int main(int argc, char * argv[]) {
    @autoreleasepool {
#if defined(MINIFB_USE_MMGR)
        // mmgr replaces the global operator new, so system libraries also allocate through it from their own threads
        m_enableMultithread();
        // The working directory of an iOS app is "/", which is read only
        NSString *tmp = NSTemporaryDirectory();
        m_setLogFiles([tmp stringByAppendingPathComponent:@MINIFB_MMGR_MEMORY_LOG].fileSystemRepresentation,
                      [tmp stringByAppendingPathComponent:@MINIFB_MMGR_LEAKS_LOG].fileSystemRepresentation);
#endif

        return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
    }
}
