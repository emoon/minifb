//
//  iOSViewController.h
//  MiniFB
//
//  Created by Carlos Aragones on 22/04/2020.
//  Copyright © 2020 Carlos Aragones. All rights reserved.
//

#import <UIKit/UIKit.h>
#include "WindowData.h"

@interface iOSViewController : UIViewController
{
    @public SWindowData *window_data;
}

- (id) initWithWindowData:(SWindowData *) windowData;

@end
