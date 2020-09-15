//
//  Renderer.h
//  MiniFB
//
//  Created by Carlos Aragones on 22/04/2020.
//  Copyright © 2020 Carlos Aragones. All rights reserved.
//

#import <MetalKit/MetalKit.h>
#include "WindowData.h"

// Our platform independent renderer class.
// Implements the MTKViewDelegate protocol which allows it to accept per-frame
// update and drawable resize callbacks.
@interface iOSViewDelegate : NSObject <MTKViewDelegate>
{
}

-(nonnull instancetype) initWithMetalKitView:(nonnull MTKView *) view windowData:(nonnull SWindowData *) windowData;
- (void) resizeTextures;

@end

