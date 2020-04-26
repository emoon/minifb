//
//  iOSViewController.m
//  MiniFB
//
//  Created by Carlos Aragones on 22/04/2020.
//  Copyright © 2020 Carlos Aragones. All rights reserved.
//

#import "iOSViewController.h"
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import "iOSViewDelegate.h"

//-------------------------------------
@implementation iOSViewController
{
    MTKView         *metal_view;
    iOSViewDelegate *view_delegate;
    SWindowData     *window_data;
}

//-------------------------------------
- (id) initWithWindowData:(SWindowData *) windowData {
    self = [super init];
    if (self) {
        window_data = windowData;
    }
    return self;
}

//-------------------------------------
- (void) loadView {
    UIView *view = [[MTKView alloc] initWithFrame:[UIScreen mainScreen].bounds];
    [self setView:view];
    [view release];
}

//-------------------------------------
- (void) viewDidLoad
{
    [super viewDidLoad];

    metal_view = (MTKView *)self.view;
    metal_view.device = MTLCreateSystemDefaultDevice();
    metal_view.backgroundColor = UIColor.blackColor;

    if(!metal_view.device) {
        NSLog(@"Metal is not supported on this device");
        self.view = [[UIView alloc] initWithFrame:self.view.frame];
        return;
    }

    view_delegate = [[iOSViewDelegate alloc] initWithMetalKitView:metal_view windowData:window_data];
    [view_delegate mtkView:metal_view drawableSizeWillChange:metal_view.bounds.size];

    metal_view.delegate = view_delegate;
}

@end
