#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <MiniFB.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define KEY_FUNCTION 0xFF
#define KEY_ESC 0x1B

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static Display* s_display;
static int s_screen;
static int s_width;
static int s_height;
static Window s_window;
static GC s_gc;
static XImage *s_ximage;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int mfb_open(const char* title, int width, int height)
{
	int depth, i, formatCount, convDepth = -1;
	XPixmapFormatValues* formats;
	XSetWindowAttributes windowAttributes;
	XSizeHints sizeHints;
	Visual* visual;

	s_display = XOpenDisplay(0);

	if (!s_display)
		return -1;
	
	s_screen = DefaultScreen(s_display);
	visual = DefaultVisual(s_display, s_screen);
	formats = XListPixmapFormats(s_display, &formatCount);
	depth = DefaultDepth(s_display, s_screen);
	Window defaultRootWindow = DefaultRootWindow(s_display);

	for (i = 0; i < formatCount; ++i)
	{
		if (depth == formats[i].depth)
		{
			convDepth = formats[i].bits_per_pixel;
			break;
		}
	}
  
	XFree(formats);

	// We only support 32-bit right now
	if (convDepth != 32)
	{
		XCloseDisplay(s_display);
		return -1;
	}

	int screenWidth = DisplayWidth(s_display, s_screen);
	int screenHeight = DisplayHeight(s_display, s_screen);

	windowAttributes.border_pixel = BlackPixel(s_display, s_screen);
	windowAttributes.background_pixel = BlackPixel(s_display, s_screen);
	windowAttributes.backing_store = NotUseful;

	s_window = XCreateWindow(s_display, defaultRootWindow, (screenWidth - width) / 2,
					(screenHeight - height) / 2, width, height, 0, depth, InputOutput,
					visual, CWBackPixel | CWBorderPixel | CWBackingStore,
					&windowAttributes);
	if (!s_window)
		return 0;

	XSelectInput(s_display, s_window, KeyPressMask | KeyReleaseMask);
	XStoreName(s_display, s_window, title);

	sizeHints.flags = PPosition | PMinSize | PMaxSize;
	sizeHints.x = 0;
	sizeHints.y = 0;
	sizeHints.min_width = width;
	sizeHints.max_width = width;
	sizeHints.min_height = height;
	sizeHints.max_height = height;

  	XSetWMNormalHints(s_display, s_window, &sizeHints);
  	XClearWindow(s_display, s_window);
  	XMapRaised(s_display, s_window);
	XFlush(s_display);

	s_gc = DefaultGC(s_display, s_screen);

	s_ximage = XCreateImage(s_display, CopyFromParent, depth, ZPixmap, 0, NULL, width, height, 32, width * 4);

	s_width = width;
	s_height = height;

	return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int processEvents()
{
	XEvent event;
	KeySym sym;

	if (!XPending(s_display))
		return;

	XNextEvent(s_display, &event);

	if (event.type != KeyPress)
		return 0;

	sym = XLookupKeysym(&event.xkey, 0);

	if ((sym >> 8) != KEY_FUNCTION)
		return 0;

	if ((sym & 0xFF) == KEY_ESC)
		return -1;

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int mfb_update(void* buffer)
{
	s_ximage->data = (char*)buffer;

	XPutImage(s_display, s_window, s_gc, s_ximage, 0, 0, 0, 0, s_width, s_height);
	XFlush(s_display);

	if (processEvents() < 0)
		return -1;

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void mfb_close (void)
{
	s_ximage->data = NULL;
	XDestroyImage(s_ximage);
	XDestroyWindow(s_display, s_window);
	XCloseDisplay(s_display);
}
