#include "iOSView.h"
#include <MiniFB_internal.h>
#include <math.h>

//-------------------------------------
static inline CGFloat
input_scale_for_view(iOSView *view) {
    CGFloat scale = 0.0;

    if (view != nil) {
        scale = view.contentScaleFactor;
    }

    if (scale <= 0.0 && view.window != nil && view.window.screen != nil) {
        scale = view.window.screen.scale;
    }

    if (scale <= 0.0) {
        scale = UIScreen.mainScreen.scale;
    }

    if (scale <= 0.0) {
        scale = 1.0;
    }

    return scale;
}

//-------------------------------------
@implementation iOSView

//-------------------------------------
- (instancetype) initWithFrame:(CGRect)frame {
    return [self initWithFrame:frame device:nil];
}

- (instancetype) initWithFrame:(CGRect)frame device:(id<MTLDevice>)device {
    self = [super initWithFrame:frame device:device];
    if (self) {
        touch_to_button = [[NSMutableDictionary alloc] init];
    }

    return self;
}

//-------------------------------------
- (void) dealloc {
#if !__has_feature(objc_arc)
    [touch_to_button release];
    touch_to_button = nil;
    [super dealloc];
#endif
}

//-------------------------------------
- (BOOL) canBecomeFirstResponder {
    return YES;
}

//-------------------------------------
// Returns the next free button index not currently in use
- (int) _allocButtonForTouch:(UITouch *)touch {
    // Find the lowest index not already taken
    bool used[MFB_MAX_MOUSE_BUTTONS_MASK + 1] = {false};
    for (NSValue *key in touch_to_button) {
        int idx = [[touch_to_button objectForKey:key] intValue];
        if (idx >= 0 && idx <= MFB_MAX_MOUSE_BUTTONS_MASK) {
            used[idx] = true;
        }
    }

    int btn = MFB_MOUSE_BTN_0;
    while (btn <= MFB_MAX_MOUSE_BUTTONS_MASK && used[btn]) {
        ++btn;
    }

    if (btn > MFB_MAX_MOUSE_BUTTONS_MASK) {
        return -1;
    }

    NSValue *key = [NSValue valueWithPointer:(__bridge const void *)touch];
    [touch_to_button setObject:@(btn) forKey:key];
    return btn;
}

//-------------------------------------
- (int) _buttonForTouch:(UITouch *)touch {
    NSValue *key = [NSValue valueWithPointer:(__bridge const void *)touch];
    NSNumber *num = [touch_to_button objectForKey:key];
    return num ? [num intValue] : -1;
}

//-------------------------------------
- (void) _freeButtonForTouch:(UITouch *)touch {
    NSValue *key = [NSValue valueWithPointer:(__bridge const void *)touch];
    [touch_to_button removeObjectForKey:key];
}

//-------------------------------------
- (void) touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    kUnused(event);

    if(window_data != 0x0) {
        CGFloat scale = input_scale_for_view(self);
        CGPoint point;
        for(UITouch *touch in touches) {
            int button_number = [self _allocButtonForTouch:touch];
            if (button_number < 0) {
                continue;
            }
            point = [touch locationInView:self];
            int32_t x = (int32_t) lround(point.x * scale);
            int32_t y = (int32_t) lround(point.y * scale);
            window_data->mouse_pos_x = mfb_pack_pos_id(x, (uint32_t) button_number);
            window_data->mouse_pos_y = mfb_pack_pos_id(y, (uint32_t) button_number);
            window_data->mouse_button_status[button_number & MFB_MAX_MOUSE_BUTTONS_MASK] = true;
            kCall(mouse_btn_func, button_number, 0, true);
        }
    }
}

//-------------------------------------
- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    kUnused(event);

    if(window_data != 0x0) {
        CGFloat scale = input_scale_for_view(self);
        CGPoint point;
        for(UITouch *touch in touches) {
            int button_number = [self _buttonForTouch:touch];
            if (button_number < 0) {
                continue;
            }
            point = [touch locationInView:self];
            int32_t x = (int32_t) lround(point.x * scale);
            int32_t y = (int32_t) lround(point.y * scale);
            window_data->mouse_pos_x = mfb_pack_pos_id(x, (uint32_t) button_number);
            window_data->mouse_pos_y = mfb_pack_pos_id(y, (uint32_t) button_number);
            // button_status does not change on move - touch is still pressed
            kCall(mouse_move_func, window_data->mouse_pos_x, window_data->mouse_pos_y);
        }
    }
}

//-------------------------------------
- (void) _endTouches:(NSSet<UITouch *> *)touches {
    if(window_data != 0x0) {
        CGFloat scale = input_scale_for_view(self);
        CGPoint point;
        for(UITouch *touch in touches) {
            int button_number = [self _buttonForTouch:touch];
            if (button_number < 0) {
                continue;
            }
            point = [touch locationInView:self];
            int32_t x = (int32_t) lround(point.x * scale);
            int32_t y = (int32_t) lround(point.y * scale);
            window_data->mouse_pos_x = mfb_pack_pos_id(x, (uint32_t) button_number);
            window_data->mouse_pos_y = mfb_pack_pos_id(y, (uint32_t) button_number);
            window_data->mouse_button_status[button_number & MFB_MAX_MOUSE_BUTTONS_MASK] = false;
            kCall(mouse_btn_func, button_number, 0, false);
            [self _freeButtonForTouch:touch];
        }
    }
}

//-------------------------------------
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    kUnused(event);
    [self _endTouches:touches];
}

//-------------------------------------
- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    kUnused(event);
    [self _endTouches:touches];
}

@end
