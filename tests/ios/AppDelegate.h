//
//  AppDelegate.h
//  MiniFB
//
//  Created by Carlos Aragones on 22/04/2020.
//  Copyright © 2020 Carlos Aragones. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface AppDelegate : UIResponder <UIApplicationDelegate>
{
    CADisplayLink   *mDisplayLink;
}

@property (strong, nonatomic) UIWindow *window;

- (void) OnUpdateFrame;

@end

