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
#include <MiniFB_internal.h>

//-------------------------------------
@interface iOSViewController ()
- (void) setupRendererIfPossible;
- (void) _onAppDidBecomeActive:(NSNotification *) note;
- (void) _onAppWillResignActive:(NSNotification *) note;
- (void) _onAppWillTerminate:(NSNotification *) note;
@end

//-------------------------------------
@implementation iOSViewController
{
    iOSView         *metal_view;
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
- (void) attachWindowData:(SWindowData *) windowData {
    window_data = windowData;

    if ([self isViewLoaded] && [self.view isKindOfClass:[iOSView class]]) {
        ((iOSView *) self.view)->window_data = window_data;
    }

    [self setupRendererIfPossible];
}

//-------------------------------------
- (void) setupRendererIfPossible {
    if (![self isViewLoaded] || ![self.view isKindOfClass:[iOSView class]]) {
        return;
    }

    metal_view = (iOSView *) self.view;
    metal_view->window_data = window_data;
    metal_view.backgroundColor = UIColor.blackColor;

    if (metal_view.device == nil) {
        id<MTLDevice> default_device = MTLCreateSystemDefaultDevice();
        metal_view.device = default_device;
#if !__has_feature(objc_arc)
        [default_device release];
#endif
    }

    if(!metal_view.device) {
        MFB_LOG(MFB_LOG_ERROR, "iOSViewController: Metal is not supported on this device.");
        UIView *fallback = [[UIView alloc] initWithFrame:self.view.frame];
        self.view = fallback;
#if !__has_feature(objc_arc)
        [fallback release];
#endif
        return;
    }

    if (window_data == NULL || window_data->specific == NULL) {
        MFB_LOG(MFB_LOG_ERROR, "iOSViewController: window_data is null in setupRendererIfPossible.");
        return;
    }

    SWindowData_IOS *window_data_specific = (SWindowData_IOS *) window_data->specific;
    if (window_data_specific->view_delegate == nil) {
        window_data_specific->view_delegate = [[iOSViewDelegate alloc] initWithMetalKitView:metal_view windowData:window_data];
    }
    if (window_data_specific->view_delegate == nil) {
        MFB_LOG(MFB_LOG_ERROR, "iOSViewController: failed to initialize iOSViewDelegate.");
        return;
    }

    // Use MTKView's effective drawable size in pixels.
    [metal_view layoutIfNeeded];
    CGSize pixelSize = metal_view.drawableSize;
    if (pixelSize.width <= 0.0 || pixelSize.height <= 0.0) {
        CGFloat scale = UIScreen.mainScreen.scale;
        pixelSize = CGSizeMake(metal_view.bounds.size.width  * scale,
                               metal_view.bounds.size.height * scale);
    }
    [window_data_specific->view_delegate mtkView:metal_view drawableSizeWillChange:pixelSize];

    if (metal_view.delegate != window_data_specific->view_delegate) {
        metal_view.delegate = window_data_specific->view_delegate;
    }
}

//-------------------------------------
- (void) loadView {
    iOSView *view = [[iOSView alloc] initWithFrame:[UIScreen mainScreen].bounds];
    // Probably the window was created automatically by an storyboard or similar
    if(window_data == 0x0) {
        MFB_LOG(MFB_LOG_ERROR, "iOSViewController: window_data is null in loadView!");
        [self setView:view];
#if !__has_feature(objc_arc)
        [view release];
#endif
        return;
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

    NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
    [nc addObserver:self selector:@selector(_onAppDidBecomeActive:)  name:UIApplicationDidBecomeActiveNotification    object:nil];
    [nc addObserver:self selector:@selector(_onAppWillResignActive:) name:UIApplicationWillResignActiveNotification  object:nil];
    [nc addObserver:self selector:@selector(_onAppWillTerminate:)    name:UIApplicationWillTerminateNotification     object:nil];

    [self setupRendererIfPossible];
}

//-------------------------------------
- (void) _onAppDidBecomeActive:(NSNotification *) note {
    (void) note;
    if (window_data == NULL) return;
    window_data->is_active = true;
    kCall(active_func, true);
}

//-------------------------------------
- (void) _onAppWillResignActive:(NSNotification *) note {
    (void) note;
    if (window_data == NULL) return;
    window_data->is_active = false;
    kCall(active_func, false);
}

//-------------------------------------
- (void) _onAppWillTerminate:(NSNotification *) note {
    (void) note;
    if (window_data == NULL) return;
    // iOS cannot cancel termination; fire the callback as a notification only.
    if (window_data->close_func != NULL) {
        window_data->close_func((struct mfb_window *) window_data);
    }
    window_data->close = true;
}

//-------------------------------------
- (void) dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
#if !__has_feature(objc_arc)
    [super dealloc];
#endif
}

@end
