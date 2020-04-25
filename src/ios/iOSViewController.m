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
    MTKView         *_view;
    iOSViewDelegate *_renderer;
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

    _view = (MTKView *)self.view;
    _view.device = MTLCreateSystemDefaultDevice();
    _view.backgroundColor = UIColor.blackColor;

    if(!_view.device) {
        NSLog(@"Metal is not supported on this device");
        self.view = [[UIView alloc] initWithFrame:self.view.frame];
        return;
    }

    _renderer = [[iOSViewDelegate alloc] initWithMetalKitView:_view];
    [_renderer mtkView:_view drawableSizeWillChange:_view.bounds.size];

    _view.delegate = _renderer;
}

@end
