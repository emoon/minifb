//
//  iOSViewController.m
//  MiniFB
//
//  Created by Carlos Aragones on 22/04/2020.
//  Copyright © 2020 Carlos Aragones. All rights reserved.
//

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import "iOSViewController.h"
#import "iOSViewDelegate.h"
#import "iOSView.h"
#include "WindowData_IOS.h"

//-------------------------------------
@implementation iOSViewController
{
    iOSView         *metal_view;
    //iOSViewDelegate *view_delegate;
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
    iOSView *view = [[iOSView alloc] initWithFrame:[UIScreen mainScreen].bounds];
    // Probably the window was created automatically by an storyboard or similar
    if(window_data == 0x0) {
        NSLog(@"WindowData is null!");
    }
    view->window_data = window_data;
    view.userInteractionEnabled = true;

    [self setView:view];
#if !__has_feature(objc_arc)
    [view release];
#endif
}

//-------------------------------------
- (void) viewDidLoad
{
    [super viewDidLoad];

    metal_view = (iOSView *) self.view;
    metal_view.device = MTLCreateSystemDefaultDevice();
    metal_view.backgroundColor = UIColor.blackColor;

    if(!metal_view.device) {
        NSLog(@"Metal is not supported on this device");
        self.view = [[UIView alloc] initWithFrame:self.view.frame];
        return;
    }

    SWindowData_IOS *window_data_ios = (SWindowData_IOS *) window_data->specific;
    window_data_ios->view_delegate = [[iOSViewDelegate alloc] initWithMetalKitView:metal_view windowData:window_data];
    [window_data_ios->view_delegate mtkView:metal_view drawableSizeWillChange:metal_view.bounds.size];

    metal_view.delegate = window_data_ios->view_delegate;
}

@end
