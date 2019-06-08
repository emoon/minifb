#include "MiniFB.h"
#include "WindowData.h"
#include <MiniFB_internal.h>

//-------------------------------------
void mfb_active_callback(struct Window *window, mfb_active_func callback) {
    if(window != 0x0) {
        SWindowData *window_data = (SWindowData *) window;
        window_data->active_func = callback;
    }
}

//-------------------------------------
void mfb_resize_callback(struct Window *window, mfb_resize_func callback) {
    if(window != 0x0) {
        SWindowData *window_data = (SWindowData *) window;
        window_data->resize_func = callback;
    }
}

//-------------------------------------
void mfb_keyboard_callback(struct Window *window, mfb_keyboard_func callback) {
    if(window != 0x0) {
        SWindowData *window_data = (SWindowData *) window;
        window_data->keyboard_func = callback;
    }
}

//-------------------------------------
void mfb_char_input_callback(struct Window *window, mfb_char_input_func callback) {
    if(window != 0x0) {
        SWindowData *window_data = (SWindowData *) window;
        window_data->char_input_func = callback;
    }
}

//-------------------------------------
void mfb_mouse_button_callback(struct Window *window, mfb_mouse_btn_func callback) {
    if(window != 0x0) {
        SWindowData *window_data = (SWindowData *) window;
        window_data->mouse_btn_func = callback;
    }
}

//-------------------------------------
void mfb_mouse_move_callback(struct Window *window, mfb_mouse_move_func callback) {
    if(window != 0x0) {
        SWindowData *window_data = (SWindowData *) window;
        window_data->mouse_move_func = callback;
    }
}

//-------------------------------------
void mfb_mouse_scroll_callback(struct Window *window, mfb_mouse_scroll_func callback) {
    if(window != 0x0) {
        SWindowData *window_data = (SWindowData *) window;
        window_data->mouse_wheel_func = callback;
    }
}

//-------------------------------------
void mfb_set_user_data(struct Window *window, void *user_data) {
    if(window != 0x0) {
        SWindowData *window_data = (SWindowData *) window;
        window_data->user_data = user_data;
    }
}

//-------------------------------------
void *mfb_get_user_data(struct Window *window) {
    if(window != 0x0) {
        SWindowData *window_data = (SWindowData *) window;
        return window_data->user_data;
    }

    return 0x0;
}

//-------------------------------------
void mfb_close(struct Window *window)
{
    if(window != 0x0) {
        SWindowData *window_data = (SWindowData *) window;
        window_data->close = true;
    }
}

//-------------------------------------
void keyboard_default(struct Window *window, Key key, KeyMod mod, bool isPressed) {
    kUnused(mod);
    kUnused(isPressed);
    if (key == KB_KEY_ESCAPE) {
        SWindowData *window_data = (SWindowData *) window;
        window_data->close = true;
    }
}


//-------------------------------------
const char *mfb_get_key_name(Key key) {

    switch (key)
    {
        case KB_KEY_SPACE:
            return "Space";

        case KB_KEY_APOSTROPHE:
            return "Apostrophe";

        case KB_KEY_COMMA:
            return "Comma";

        case KB_KEY_MINUS:
            return "Minus";

        case KB_KEY_PERIOD:
            return "Period";

        case KB_KEY_SLASH:
            return "Slash";

        case KB_KEY_0:
            return "0";

        case KB_KEY_1:
            return "1";

        case KB_KEY_2:
            return "2";

        case KB_KEY_3:
            return "3";

        case KB_KEY_4:
            return "4";

        case KB_KEY_5:
            return "5";

        case KB_KEY_6:
            return "6";

        case KB_KEY_7:
            return "7";

        case KB_KEY_8:
            return "8";

        case KB_KEY_9:
            return "9";

        case KB_KEY_SEMICOLON:
            return "Semicolon";

        case KB_KEY_EQUAL:
            return "Equal";

        case KB_KEY_A:
            return "A";

        case KB_KEY_B:
            return "B";

        case KB_KEY_C:
            return "C";

        case KB_KEY_D:
            return "D";

        case KB_KEY_E:
            return "E";

        case KB_KEY_F:
            return "F";

        case KB_KEY_G:
            return "G";

        case KB_KEY_H:
            return "H";

        case KB_KEY_I:
            return "I";

        case KB_KEY_J:
            return "J";

        case KB_KEY_K:
            return "K";

        case KB_KEY_L:
            return "L";

        case KB_KEY_M:
            return "M";

        case KB_KEY_N:
            return "N";

        case KB_KEY_O:
            return "O";

        case KB_KEY_P:
            return "P";

        case KB_KEY_Q:
            return "Q";

        case KB_KEY_R:
            return "R";

        case KB_KEY_S:
            return "S";

        case KB_KEY_T:
            return "T";

        case KB_KEY_U:
            return "U";

        case KB_KEY_V:
            return "V";

        case KB_KEY_W:
            return "W";

        case KB_KEY_X:
            return "X";

        case KB_KEY_Y:
            return "Y";

        case KB_KEY_Z:
            return "Z";

        case KB_KEY_LEFT_BRACKET:
            return "Left_Bracket";

        case KB_KEY_BACKSLASH:
            return "Backslash";

        case KB_KEY_RIGHT_BRACKET:
            return "Right_Bracket";

        case KB_KEY_GRAVE_ACCENT:
            return "Grave_Accent";

        case KB_KEY_WORLD_1:
            return "World_1";

        case KB_KEY_WORLD_2:
            return "World_2";

        case KB_KEY_ESCAPE:
            return "Escape";

        case KB_KEY_ENTER:
            return "Enter";

        case KB_KEY_TAB:
            return "Tab";

        case KB_KEY_BACKSPACE:
            return "Backspace";

        case KB_KEY_INSERT:
            return "Insert";

        case KB_KEY_DELETE:
            return "Delete";

        case KB_KEY_RIGHT:
            return "Right";

        case KB_KEY_LEFT:
            return "Left";

        case KB_KEY_DOWN:
            return "Down";

        case KB_KEY_UP:
            return "Up";

        case KB_KEY_PAGE_UP:
            return "Page_Up";

        case KB_KEY_PAGE_DOWN:
            return "Page_Down";

        case KB_KEY_HOME:
            return "Home";

        case KB_KEY_END:
            return "End";

        case KB_KEY_CAPS_LOCK:
            return "Caps_Lock";

        case KB_KEY_SCROLL_LOCK:
            return "Scroll_Lock";

        case KB_KEY_NUM_LOCK:
            return "Num_Lock";

        case KB_KEY_PRINT_SCREEN:
            return "Print_Screen";

        case KB_KEY_PAUSE:
            return "Pause";

        case KB_KEY_F1:
            return "F1";

        case KB_KEY_F2:
            return "F2";

        case KB_KEY_F3:
            return "F3";

        case KB_KEY_F4:
            return "F4";

        case KB_KEY_F5:
            return "F5";

        case KB_KEY_F6:
            return "F6";

        case KB_KEY_F7:
            return "F7";

        case KB_KEY_F8:
            return "F8";

        case KB_KEY_F9:
            return "F9";

        case KB_KEY_F10:
            return "F10";

        case KB_KEY_F11:
            return "F11";

        case KB_KEY_F12:
            return "F12";

        case KB_KEY_F13:
            return "F13";

        case KB_KEY_F14:
            return "F14";

        case KB_KEY_F15:
            return "F15";

        case KB_KEY_F16:
            return "F16";

        case KB_KEY_F17:
            return "F17";

        case KB_KEY_F18:
            return "F18";

        case KB_KEY_F19:
            return "F19";

        case KB_KEY_F20:
            return "F20";

        case KB_KEY_F21:
            return "F21";

        case KB_KEY_F22:
            return "F22";

        case KB_KEY_F23:
            return "F23";

        case KB_KEY_F24:
            return "F24";

        case KB_KEY_F25:
            return "F25";

        case KB_KEY_KP_0:
            return "KP_0";

        case KB_KEY_KP_1:
            return "KP_1";

        case KB_KEY_KP_2:
            return "KP_2";

        case KB_KEY_KP_3:
            return "KP_3";

        case KB_KEY_KP_4:
            return "KP_4";

        case KB_KEY_KP_5:
            return "KP_5";

        case KB_KEY_KP_6:
            return "KP_6";

        case KB_KEY_KP_7:
            return "KP_7";

        case KB_KEY_KP_8:
            return "KP_8";

        case KB_KEY_KP_9:
            return "KP_9";

        case KB_KEY_KP_DECIMAL:
            return "KP_Decimal";

        case KB_KEY_KP_DIVIDE:
            return "KP_Divide";

        case KB_KEY_KP_MULTIPLY:
            return "KP_Multiply";

        case KB_KEY_KP_SUBTRACT:
            return "KP_Subtract";

        case KB_KEY_KP_ADD:
            return "KP_Add";

        case KB_KEY_KP_ENTER:
            return "KP_Enter";

        case KB_KEY_KP_EQUAL:
            return "KP_Equal";

        case KB_KEY_LEFT_SHIFT:
            return "Left_Shift";

        case KB_KEY_LEFT_CONTROL:
            return "Left_Control";

        case KB_KEY_LEFT_ALT:
            return "Left_Alt";

        case KB_KEY_LEFT_SUPER:
            return "Left_Super";

        case KB_KEY_RIGHT_SHIFT:
            return "Right_Shift";

        case KB_KEY_RIGHT_CONTROL:
            return "Right_Control";

        case KB_KEY_RIGHT_ALT:
            return "Right_Alt";

        case KB_KEY_RIGHT_SUPER:
            return "Right_Super";

        case KB_KEY_MENU:
            return "Menu";

    }
    return "Unknown";
}
