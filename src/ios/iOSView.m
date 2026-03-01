#include "iOSView.h"
#include <MiniFB_internal.h>

//-------------------------------------
@implementation iOSView

//-------------------------------------
- (BOOL) canBecomeFirstResponder {
    return YES;
}

//-------------------------------------
- (void) touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    kUnused(event);

    if(window_data != 0x0) {
        CGPoint point;
        int     button_number = MOUSE_BTN_0;
        for(UITouch *touch in touches) {
            point = [touch locationInView:self];
            window_data->mouse_pos_x = point.x;
            window_data->mouse_pos_y = point.y;
            window_data->mouse_button_status[button_number & MFB_MAX_MOUSE_BUTTONS_MASK] = true;
            kCall(mouse_btn_func, button_number, 0, true);
            ++button_number;
        }
    }
}

//-------------------------------------
- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    kUnused(event);

    if(window_data != 0x0) {
        CGPoint point;
        int     button_number = MOUSE_BTN_0;
        for(UITouch *touch in touches) {
            point = [touch locationInView:self];
            window_data->mouse_pos_x = point.x;
            window_data->mouse_pos_y = point.y;
            window_data->mouse_button_status[button_number & MFB_MAX_MOUSE_BUTTONS_MASK] = true;
            kCall(mouse_move_func, point.x, point.y);
            ++button_number;
        }
    }
}

//-------------------------------------
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    kUnused(event);

    if(window_data != 0x0) {
        CGPoint point;
        int     button_number = MOUSE_BTN_0;
        for(UITouch *touch in touches) {
            point = [touch locationInView:self];
            window_data->mouse_pos_x = point.x;
            window_data->mouse_pos_y = point.y;
            window_data->mouse_button_status[button_number & MFB_MAX_MOUSE_BUTTONS_MASK] = false;
            kCall(mouse_btn_func, button_number, 0, false);
            ++button_number;
        }
    }
}

//-------------------------------------
- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    kUnused(event);

    if(window_data != 0x0) {
        CGPoint point;
        int     button_number = MOUSE_BTN_0;
        for(UITouch *touch in touches) {
            point = [touch locationInView:self];
            window_data->mouse_pos_x = point.x;
            window_data->mouse_pos_y = point.y;
            window_data->mouse_button_status[button_number & MFB_MAX_MOUSE_BUTTONS_MASK] = false;
            kCall(mouse_btn_func, button_number, 0, false);
            ++button_number;
        }
    }
}

@end
