//
//  Renderer.h
//  MiniFB
//
//  Created by Carlos Aragones on 22/04/2020.
//  Copyright © 2020 Carlos Aragones. All rights reserved.
//

#import <MetalKit/MetalKit.h>
#include "WindowData.h"
#include "WindowData_IOS.h"

// Our platform independent renderer class.
// Implements the MTKViewDelegate protocol which allows it to accept per-frame
// update and drawable resize callbacks.
@interface iOSViewDelegate : NSObject <MTKViewDelegate>
{
    @public SWindowData     *window_data;
    @public SWindowData_IOS *window_data_ios;
}

-(nonnull instancetype) initWithMetalKitView:(nonnull MTKView *) view windowData:(nonnull SWindowData *) windowData;

@end

