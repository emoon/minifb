#pragma once

/*
epic preprocessor tomfoolery

pass another macro name INTO this macro to get the correct data in the way you need it

e.g to link every value to its string:
	const char *key_strings = {
		#define DEFINE_KEY(name, value, str) [name] = str,
	KEY_LIST(DEFINE_KEY)
		#undef DEFINE_KEY
	}
*/

#define KEY_LIST(_) \
    _(KB_KEY_UNKNOWN,       -1,  "Unknown") \
    _(KB_KEY_SPACE,         32,  "Space") \
    _(KB_KEY_APOSTROPHE,    39,  "Apostrophe") \
    _(KB_KEY_COMMA,         44,  "Comma") \
    _(KB_KEY_MINUS,         45,  "Minus") \
    _(KB_KEY_PERIOD,        46,  "Period") \
    _(KB_KEY_SLASH,         47,  "Slash") \
    _(KB_KEY_0,             48,  "0") \
    _(KB_KEY_1,             49,  "1") \
    _(KB_KEY_2,             50,  "2") \
    _(KB_KEY_3,             51,  "3") \
    _(KB_KEY_4,             52,  "4") \
    _(KB_KEY_5,             53,  "5") \
    _(KB_KEY_6,             54,  "6") \
    _(KB_KEY_7,             55,  "7") \
    _(KB_KEY_8,             56,  "8") \
    _(KB_KEY_9,             57,  "9") \
    _(KB_KEY_SEMICOLON,     59,  "Semicolon") \
    _(KB_KEY_EQUAL,         61,  "Equal") \
    _(KB_KEY_A,             65,  "A") \
    _(KB_KEY_B,             66,  "B") \
    _(KB_KEY_C,             67,  "C") \
    _(KB_KEY_D,             68,  "D") \
    _(KB_KEY_E,             69,  "E") \
    _(KB_KEY_F,             70,  "F") \
    _(KB_KEY_G,             71,  "G") \
    _(KB_KEY_H,             72,  "H") \
    _(KB_KEY_I,             73,  "I") \
    _(KB_KEY_J,             74,  "J") \
    _(KB_KEY_K,             75,  "K") \
    _(KB_KEY_L,             76,  "L") \
    _(KB_KEY_M,             77,  "M") \
    _(KB_KEY_N,             78,  "N") \
    _(KB_KEY_O,             79,  "O") \
    _(KB_KEY_P,             80,  "P") \
    _(KB_KEY_Q,             81,  "Q") \
    _(KB_KEY_R,             82,  "R") \
    _(KB_KEY_S,             83,  "S") \
    _(KB_KEY_T,             84,  "T") \
    _(KB_KEY_U,             85,  "U") \
    _(KB_KEY_V,             86,  "V") \
    _(KB_KEY_W,             87,  "W") \
    _(KB_KEY_X,             88,  "X") \
    _(KB_KEY_Y,             89,  "Y") \
    _(KB_KEY_Z,             90,  "Z") \
    _(KB_KEY_LEFT_BRACKET,  91,  "Left_Bracket") \
    _(KB_KEY_BACKSLASH,     92,  "Backslash") \
    _(KB_KEY_RIGHT_BRACKET, 93,  "Right_Bracket") \
    _(KB_KEY_GRAVE_ACCENT,  96,  "Grave_Accent") \
    _(KB_KEY_WORLD_1,       161, "World_1") \
    _(KB_KEY_WORLD_2,       162, "World_2") \
    _(KB_KEY_ESCAPE,        256, "Escape") \
    _(KB_KEY_ENTER,         257, "Enter") \
    _(KB_KEY_TAB,           258, "Tab") \
    _(KB_KEY_BACKSPACE,     259, "Backspace") \
    _(KB_KEY_INSERT,        260, "Insert") \
    _(KB_KEY_DELETE,        261, "Delete") \
    _(KB_KEY_RIGHT,         262, "Right") \
    _(KB_KEY_LEFT,          263, "Left") \
    _(KB_KEY_DOWN,          264, "Down") \
    _(KB_KEY_UP,            265, "Up") \
    _(KB_KEY_PAGE_UP,       266, "Page_Up") \
    _(KB_KEY_PAGE_DOWN,     267, "Page_Down") \
    _(KB_KEY_HOME,          268, "Home") \
    _(KB_KEY_END,           269, "End") \
    _(KB_KEY_CAPS_LOCK,     280, "Caps_Lock") \
    _(KB_KEY_SCROLL_LOCK,   281, "Scroll_Lock") \
    _(KB_KEY_NUM_LOCK,      282, "Num_Lock") \
    _(KB_KEY_PRINT_SCREEN,  283, "Print_Screen") \
    _(KB_KEY_PAUSE,         284, "Pause") \
    _(KB_KEY_F1,            290, "F1") \
    _(KB_KEY_F2,            291, "F2") \
    _(KB_KEY_F3,            292, "F3") \
    _(KB_KEY_F4,            293, "F4") \
    _(KB_KEY_F5,            294, "F5") \
    _(KB_KEY_F6,            295, "F6") \
    _(KB_KEY_F7,            296, "F7") \
    _(KB_KEY_F8,            297, "F8") \
    _(KB_KEY_F9,            298, "F9") \
    _(KB_KEY_F10,           299, "F10") \
    _(KB_KEY_F11,           300, "F11") \
    _(KB_KEY_F12,           301, "F12") \
    _(KB_KEY_F13,           302, "F13") \
    _(KB_KEY_F14,           303, "F14") \
    _(KB_KEY_F15,           304, "F15") \
    _(KB_KEY_F16,           305, "F16") \
    _(KB_KEY_F17,           306, "F17") \
    _(KB_KEY_F18,           307, "F18") \
    _(KB_KEY_F19,           308, "F19") \
    _(KB_KEY_F20,           309, "F20") \
    _(KB_KEY_F21,           310, "F21") \
    _(KB_KEY_F22,           311, "F22") \
    _(KB_KEY_F23,           312, "F23") \
    _(KB_KEY_F24,           313, "F24") \
    _(KB_KEY_F25,           314, "F25") \
    _(KB_KEY_KP_0,          320, "KP_0") \
    _(KB_KEY_KP_1,          321, "KP_1") \
    _(KB_KEY_KP_2,          322, "KP_2") \
    _(KB_KEY_KP_3,          323, "KP_3") \
    _(KB_KEY_KP_4,          324, "KP_4") \
    _(KB_KEY_KP_5,          325, "KP_5") \
    _(KB_KEY_KP_6,          326, "KP_6") \
    _(KB_KEY_KP_7,          327, "KP_7") \
    _(KB_KEY_KP_8,          328, "KP_8") \
    _(KB_KEY_KP_9,          329, "KP_9") \
    _(KB_KEY_KP_DECIMAL,    330, "KP_Decimal") \
    _(KB_KEY_KP_DIVIDE,     331, "KP_Divide") \
    _(KB_KEY_KP_MULTIPLY,   332, "KP_Multiply") \
    _(KB_KEY_KP_SUBTRACT,   333, "KP_Subtract") \
    _(KB_KEY_KP_ADD,        334, "KP_Add") \
    _(KB_KEY_KP_ENTER,      335, "KP_Enter") \
    _(KB_KEY_KP_EQUAL,      336, "KP_Equal") \
    _(KB_KEY_LEFT_SHIFT,    340, "Left_Shift") \
    _(KB_KEY_LEFT_CONTROL,  341, "Left_Control") \
    _(KB_KEY_LEFT_ALT,      342, "Left_Alt") \
    _(KB_KEY_LEFT_SUPER,    343, "Left_Super") \
    _(KB_KEY_RIGHT_SHIFT,   344, "Right_Shift") \
    _(KB_KEY_RIGHT_CONTROL, 345, "Right_Control") \
    _(KB_KEY_RIGHT_ALT,     346, "Right_Alt") \
    _(KB_KEY_RIGHT_SUPER,   347, "Right_Super") \
    _(KB_KEY_MENU,          348, "Menu")
