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
        int     buttonNumber = MOUSE_BTN_0;
        for(UITouch *touch in touches) {
            point = [touch locationInView:self];
            window_data->mouse_pos_x = point.x;
            window_data->mouse_pos_y = point.y;
            window_data->mouse_button_status[buttonNumber & 0x07] = true;
            kCall(mouse_btn_func, buttonNumber, 0, true);
            ++buttonNumber;
        }
    }
}

//-------------------------------------
- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    kUnused(event);

    if(window_data != 0x0) {
        CGPoint point;
        int     buttonNumber = MOUSE_BTN_0;
        for(UITouch *touch in touches) {
            point = [touch locationInView:self];
            window_data->mouse_pos_x = point.x;
            window_data->mouse_pos_y = point.y;
            window_data->mouse_button_status[buttonNumber & 0x07] = true;
            kCall(mouse_move_func, point.x, point.y);
            ++buttonNumber;
        }
    }
}

//-------------------------------------
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    kUnused(event);

    if(window_data != 0x0) {
        CGPoint point;
        int     buttonNumber = MOUSE_BTN_0;
        for(UITouch *touch in touches) {
            point = [touch locationInView:self];
            window_data->mouse_pos_x = point.x;
            window_data->mouse_pos_y = point.y;
            window_data->mouse_button_status[buttonNumber & 0x07] = false;
            kCall(mouse_btn_func, buttonNumber, 0, false);
            ++buttonNumber;
        }
    }
}

//-------------------------------------
- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    kUnused(event);

    if(window_data != 0x0) {
        CGPoint point;
        int     buttonNumber = MOUSE_BTN_0;
        for(UITouch *touch in touches) {
            point = [touch locationInView:self];
            window_data->mouse_pos_x = point.x;
            window_data->mouse_pos_y = point.y;
            window_data->mouse_button_status[buttonNumber & 0x07] = false;
            kCall(mouse_btn_func, buttonNumber, 0, false);
            ++buttonNumber;
        }
    }
}

@end
