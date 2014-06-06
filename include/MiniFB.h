#ifndef _MINIFB_H_
#define _MINIFB_H_

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum
{
	MFB_KEY_ESC = 0x18,
	MFB_KEY_A   = 0x41,
	MFB_KEY_B   = 0x42,
	MFB_KEY_C   = 0x43,
	MFB_KEY_D   = 0x44,
	MFB_KEY_E   = 0x45,
	MFB_KEY_F   = 0x46,
	MFB_KEY_G   = 0x47,
	MFB_KEY_H   = 0x48,
	MFB_KEY_I   = 0x49,
	MFB_KEY_J   = 0x4A,
	MFB_KEY_K   = 0x4B,
	MFB_KEY_L   = 0x4C,
	MFB_KEY_M   = 0x4D,
	MFB_KEY_N   = 0x4E,
	MFB_KEY_O   = 0x4F,
	MFB_KEY_P   = 0x50,
	MFB_KEY_Q   = 0x51,
	MFB_KEY_R   = 0x52,
	MFB_KEY_S   = 0x53,
	MFB_KEY_T   = 0x54,
	MFB_KEY_U   = 0x55,
	MFB_KEY_V   = 0x56,
	MFB_KEY_W   = 0x57,
	MFB_KEY_X   = 0x58,
	MFB_KEY_Y   = 0x59,
	MFB_KEY_Z   = 0x5A,
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define MFB_RGB(r, g, b) (((unsigned int)r) << 16) | (((unsigned int)g) << 8) | b

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Create a window 
int mfb_open(const char* name, int width, int height);

// Update the display. Input buffer is assumed to be a 32-bit buffer of the size given in the open call
// Will return -1 on error, 0 if no key has been pressed otherwise the key code matching the keycode enum
int mfb_update(void* buffer);

// Close the window
void mfb_close();

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif
