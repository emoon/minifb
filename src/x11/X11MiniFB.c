#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <stdlib.h>
#include <MiniFB.h>
#include <MiniFB_internal.h>
#include "X11WindowData.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SWindowData g_window_data  = { 0 };

extern void 
stretch_image(uint32_t *srcImage, uint32_t srcX, uint32_t srcY, uint32_t srcWidth, uint32_t srcHeight, uint32_t srcPitch,
              uint32_t *dstImage, uint32_t dstX, uint32_t dstY, uint32_t dstWidth, uint32_t dstHeight, uint32_t dstPitch);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int mfb_open(const char* title, int width, int height)
{
	return mfb_open_ex(title, width, height, 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int translate_key(int scancode);
int translate_mod(int state);
int translate_mod_ex(int key, int state, int is_pressed);

static int processEvents()
{
	XEvent event;

	while ((g_window_data.close == eFalse) && XPending(g_window_data.display)) {
		XNextEvent(g_window_data.display, &event);

		switch (event.type) {
			case KeyPress:
			case KeyRelease: 
			{
				int kb_key     = translate_key(event.xkey.keycode);
				int is_pressed = (event.type == KeyPress);
				g_window_data.mod_keys = translate_mod_ex(kb_key, event.xkey.state, is_pressed);

				kCall(s_keyboard, kb_key, g_window_data.mod_keys, is_pressed);
			}
			break;

			case ButtonPress:
			case ButtonRelease:
			{
				eMouseButton button     = event.xbutton.button;
				int          is_pressed = (event.type == ButtonPress);
				g_window_data.mod_keys = translate_mod(event.xkey.state);
				switch (button) {
					case Button1:
					case Button2:
					case Button3:
						kCall(s_mouse_btn, button, g_window_data.mod_keys, is_pressed);
						break;

					case Button4:
						kCall(s_mouse_wheel, g_window_data.mod_keys, 0.0f, 1.0f);
						break;
					case Button5:
						kCall(s_mouse_wheel, g_window_data.mod_keys, 0.0f, -1.0f);
						break;

					case 6:
						kCall(s_mouse_wheel, g_window_data.mod_keys, 1.0f, 0.0f);
						break;
					case 7:
						kCall(s_mouse_wheel, g_window_data.mod_keys, -1.0f, 0.0f);
						break;

					default:
						kCall(s_mouse_btn, button - 4, g_window_data.mod_keys, is_pressed);
						break;
				}
			}
			break;

			case MotionNotify:
				kCall(s_mouse_move, event.xmotion.x, event.xmotion.y);
				break;

			case ConfigureNotify: 
			{
				g_window_data.window_width  = event.xconfigure.width;
				g_window_data.window_height = event.xconfigure.height;
				g_window_data.dst_offset_x = 0;
				g_window_data.dst_offset_y = 0;
				g_window_data.dst_width    = g_window_data.window_width;
				g_window_data.dst_height   = g_window_data.window_height;

				XClearWindow(g_window_data.display, g_window_data.window);
				kCall(s_resize, g_window_data.window_width, g_window_data.window_height);
			}
			break;

			case EnterNotify:
			case LeaveNotify:
			break;

			case FocusIn:
				kCall(s_active, eTrue);
				break;

			case FocusOut:
				kCall(s_active, eFalse);
				break;

			case DestroyNotify:
				return -1;
				break;
		}
	}

	if(g_window_data.close == eTrue)
		return -1;

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int mfb_update(void* buffer)
{
    if (buffer == 0x0) {
        return -2;
    }

    if (g_window_data.buffer_width != g_window_data.dst_width || g_window_data.buffer_height != g_window_data.dst_height) {
        if(g_window_data.image_scaler_width != g_window_data.dst_width || g_window_data.image_scaler_height != g_window_data.dst_height) {
            if(g_window_data.image_scaler != 0x0) {
                g_window_data.image_scaler->data = 0x0;
                XDestroyImage(g_window_data.image_scaler);
            }
            if(g_window_data.image_buffer != 0x0) {
                free(g_window_data.image_buffer);
                g_window_data.image_buffer = 0x0;
            }
            int depth = DefaultDepth(g_window_data.display, g_window_data.screen);
            g_window_data.image_buffer = malloc(g_window_data.dst_width * g_window_data.dst_height * 4);
            g_window_data.image_scaler_width  = g_window_data.dst_width;
            g_window_data.image_scaler_height = g_window_data.dst_height;
            g_window_data.image_scaler = XCreateImage(g_window_data.display, CopyFromParent, depth, ZPixmap, 0, NULL, g_window_data.image_scaler_width, g_window_data.image_scaler_height, 32, g_window_data.image_scaler_width * 4);
        }
    }

    if(g_window_data.image_scaler != 0x0) {
        stretch_image(buffer, 0, 0, g_window_data.buffer_width, g_window_data.buffer_height, g_window_data.buffer_width, g_window_data.image_buffer, 0, 0, g_window_data.dst_width, g_window_data.dst_height, g_window_data.dst_width);
        g_window_data.image_scaler->data = g_window_data.image_buffer;
	    XPutImage(g_window_data.display, g_window_data.window, g_window_data.gc, g_window_data.image_scaler, 0, 0, g_window_data.dst_offset_x, g_window_data.dst_offset_y, g_window_data.dst_width, g_window_data.dst_height);
    }
    else {
    	g_window_data.image->data = (char *) buffer;
	    XPutImage(g_window_data.display, g_window_data.window, g_window_data.gc, g_window_data.image, 0, 0, g_window_data.dst_offset_x, g_window_data.dst_offset_y, g_window_data.dst_width, g_window_data.dst_height);
    }
	XFlush(g_window_data.display);

	if (processEvents() < 0)
		return -1;

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void mfb_close(void)
{
	if(g_window_data.image != 0x0) {
		g_window_data.image->data = 0x0;
        XDestroyImage(g_window_data.image);
		XDestroyWindow(g_window_data.display, g_window_data.window);
		XCloseDisplay(g_window_data.display);

		g_window_data.image  = 0x0;
		g_window_data.display = 0x0;
		g_window_data.window  = 0;
	}
	g_window_data.close = eTrue;
}
