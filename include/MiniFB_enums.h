#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "MiniFB_keylist.h"
#include "MiniFB_macros.h"

// Enums
//----------------------------
typedef enum {
    MFB_STATE_OK             =  0,
    MFB_STATE_EXIT           = -1,
    MFB_STATE_INVALID_WINDOW = -2,
    MFB_STATE_INVALID_BUFFER = -3,
    MFB_STATE_INTERNAL_ERROR = -4,

    STATE_OK             __MFB_ENUM_DEPRECATED("STATE_OK is deprecated, use MFB_STATE_OK")                         = MFB_STATE_OK,
    STATE_EXIT           __MFB_ENUM_DEPRECATED("STATE_EXIT is deprecated, use MFB_STATE_EXIT")                     = MFB_STATE_EXIT,
    STATE_INVALID_WINDOW __MFB_ENUM_DEPRECATED("STATE_INVALID_WINDOW is deprecated, use MFB_STATE_INVALID_WINDOW") = MFB_STATE_INVALID_WINDOW,
    STATE_INVALID_BUFFER __MFB_ENUM_DEPRECATED("STATE_INVALID_BUFFER is deprecated, use MFB_STATE_INVALID_BUFFER") = MFB_STATE_INVALID_BUFFER,
    STATE_INTERNAL_ERROR __MFB_ENUM_DEPRECATED("STATE_INTERNAL_ERROR is deprecated, use MFB_STATE_INTERNAL_ERROR") = MFB_STATE_INTERNAL_ERROR,
} mfb_update_state;

//-------------------------------------
typedef enum {
    MFB_MOUSE_BTN_0, // No mouse button
    MFB_MOUSE_BTN_1,
    MFB_MOUSE_BTN_2,
    MFB_MOUSE_BTN_3,
    MFB_MOUSE_BTN_4,
    MFB_MOUSE_BTN_5,
    MFB_MOUSE_BTN_6,
    MFB_MOUSE_BTN_7,

    MOUSE_BTN_0 __MFB_ENUM_DEPRECATED("MOUSE_BTN_0 is deprecated, use MFB_MOUSE_BTN_0") = MFB_MOUSE_BTN_0,
    MOUSE_BTN_1 __MFB_ENUM_DEPRECATED("MOUSE_BTN_1 is deprecated, use MFB_MOUSE_BTN_1") = MFB_MOUSE_BTN_1,
    MOUSE_BTN_2 __MFB_ENUM_DEPRECATED("MOUSE_BTN_2 is deprecated, use MFB_MOUSE_BTN_2") = MFB_MOUSE_BTN_2,
    MOUSE_BTN_3 __MFB_ENUM_DEPRECATED("MOUSE_BTN_3 is deprecated, use MFB_MOUSE_BTN_3") = MFB_MOUSE_BTN_3,
    MOUSE_BTN_4 __MFB_ENUM_DEPRECATED("MOUSE_BTN_4 is deprecated, use MFB_MOUSE_BTN_4") = MFB_MOUSE_BTN_4,
    MOUSE_BTN_5 __MFB_ENUM_DEPRECATED("MOUSE_BTN_5 is deprecated, use MFB_MOUSE_BTN_5") = MFB_MOUSE_BTN_5,
    MOUSE_BTN_6 __MFB_ENUM_DEPRECATED("MOUSE_BTN_6 is deprecated, use MFB_MOUSE_BTN_6") = MFB_MOUSE_BTN_6,
    MOUSE_BTN_7 __MFB_ENUM_DEPRECATED("MOUSE_BTN_7 is deprecated, use MFB_MOUSE_BTN_7") = MFB_MOUSE_BTN_7,

    __mfb_mouse_left_deprecated   __MFB_ENUM_DEPRECATED("MOUSE_LEFT is deprecated, use MFB_MOUSE_LEFT")     = MFB_MOUSE_BTN_1,
    __mfb_mouse_right_deprecated  __MFB_ENUM_DEPRECATED("MOUSE_RIGHT is deprecated, use MFB_MOUSE_RIGHT")   = MFB_MOUSE_BTN_2,
    __mfb_mouse_middle_deprecated __MFB_ENUM_DEPRECATED("MOUSE_MIDDLE is deprecated, use MFB_MOUSE_MIDDLE") = MFB_MOUSE_BTN_3,
} mfb_mouse_button;

//-----------------
#define MFB_MOUSE_LEFT   MFB_MOUSE_BTN_1
#define MFB_MOUSE_RIGHT  MFB_MOUSE_BTN_2
#define MFB_MOUSE_MIDDLE MFB_MOUSE_BTN_3

#define MOUSE_LEFT       __mfb_mouse_left_deprecated
#define MOUSE_RIGHT      __mfb_mouse_right_deprecated
#define MOUSE_MIDDLE     __mfb_mouse_middle_deprecated

//-------------------------------------
typedef enum {
    #define KEY_VALUE(NAME, VALUE, _) MFB_##NAME = VALUE,
KEY_LIST(KEY_VALUE)
    #undef KEY_VALUE

    #define KEY_VALUE(NAME, VALUE, _) NAME __MFB_ENUM_DEPRECATED(#NAME " is deprecated, use MFB_" #NAME) = MFB_##NAME,
KEY_LIST(KEY_VALUE)
    #undef KEY_VALUE

    __mfb_kb_key_last_deprecated __MFB_ENUM_DEPRECATED("KB_KEY_LAST is deprecated, use MFB_KB_KEY_LAST") = MFB_KB_KEY_MENU,
} mfb_key;

//-----------------
#define MFB_KB_KEY_LAST  MFB_KB_KEY_MENU
#define KB_KEY_LAST      __mfb_kb_key_last_deprecated

//-------------------------------------
typedef enum {
    MFB_KB_MOD_SHIFT        = 0x0001,
    MFB_KB_MOD_CONTROL      = 0x0002,
    MFB_KB_MOD_ALT          = 0x0004,
    MFB_KB_MOD_SUPER        = 0x0008,
    MFB_KB_MOD_CAPS_LOCK    = 0x0010,
    MFB_KB_MOD_NUM_LOCK     = 0x0020,

    KB_MOD_SHIFT     __MFB_ENUM_DEPRECATED("KB_MOD_SHIFT is deprecated, use MFB_KB_MOD_SHIFT")         = MFB_KB_MOD_SHIFT,
    KB_MOD_CONTROL   __MFB_ENUM_DEPRECATED("KB_MOD_CONTROL is deprecated, use MFB_KB_MOD_CONTROL")     = MFB_KB_MOD_CONTROL,
    KB_MOD_ALT       __MFB_ENUM_DEPRECATED("KB_MOD_ALT is deprecated, use MFB_KB_MOD_ALT")             = MFB_KB_MOD_ALT,
    KB_MOD_SUPER     __MFB_ENUM_DEPRECATED("KB_MOD_SUPER is deprecated, use MFB_KB_MOD_SUPER")         = MFB_KB_MOD_SUPER,
    KB_MOD_CAPS_LOCK __MFB_ENUM_DEPRECATED("KB_MOD_CAPS_LOCK is deprecated, use MFB_KB_MOD_CAPS_LOCK") = MFB_KB_MOD_CAPS_LOCK,
    KB_MOD_NUM_LOCK  __MFB_ENUM_DEPRECATED("KB_MOD_NUM_LOCK is deprecated, use MFB_KB_MOD_NUM_LOCK")   = MFB_KB_MOD_NUM_LOCK
} mfb_key_mod;

//-------------------------------------
typedef enum {
    MFB_WF_RESIZABLE          = 0x01,
    MFB_WF_FULLSCREEN         = 0x02,
    MFB_WF_FULLSCREEN_DESKTOP = 0x04,
    MFB_WF_BORDERLESS         = 0x08,
    MFB_WF_ALWAYS_ON_TOP      = 0x10,

    MFB_WF_SIZE_LOGICAL       = 0x20,  // width/height are OS logical units (points / CSS px)
    MFB_WF_SIZE_PHYSICAL      = 0x40,  // width/height are physical device pixels

    WF_RESIZABLE          __MFB_ENUM_DEPRECATED("WF_RESIZABLE is deprecated, use MFB_WF_RESIZABLE")                   = MFB_WF_RESIZABLE,
    WF_FULLSCREEN         __MFB_ENUM_DEPRECATED("WF_FULLSCREEN is deprecated, use MFB_WF_FULLSCREEN")                 = MFB_WF_FULLSCREEN,
    WF_FULLSCREEN_DESKTOP __MFB_ENUM_DEPRECATED("WF_FULLSCREEN_DESKTOP is deprecated, use MFB_WF_FULLSCREEN_DESKTOP") = MFB_WF_FULLSCREEN_DESKTOP,
    WF_BORDERLESS         __MFB_ENUM_DEPRECATED("WF_BORDERLESS is deprecated, use MFB_WF_BORDERLESS")                 = MFB_WF_BORDERLESS,
    WF_ALWAYS_ON_TOP      __MFB_ENUM_DEPRECATED("WF_ALWAYS_ON_TOP is deprecated, use MFB_WF_ALWAYS_ON_TOP")           = MFB_WF_ALWAYS_ON_TOP,
} mfb_window_flags;

//-------------------------------------
typedef enum {
    MFB_LOG_TRACE = 0,
    MFB_LOG_DEBUG,
    MFB_LOG_INFO,
    MFB_LOG_WARNING,
    MFB_LOG_ERROR,
} mfb_log_level;
