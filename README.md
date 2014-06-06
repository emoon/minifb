MinFB
======

MiniFB (Mini FrameBuffer) is a small cross platform library that makes it easy to render (32-bit) pixels in a window. An example is the best way to show how it works:

	if (!mfb_open("my display", 800, 600))
		return 0;

	for (;;)
	{
		int state;

		// TODO: add some fancy rendering to the buffer of size 800 * 600

		state = mfb_update(buffer);

		if (state < 0)
			break;
	}

	mfb_close();


First the code creates window with the mfb_open call that is used to display the data, next it's the applications resposiblity to allocate a buffer (which has to be at least the size of the window and in 32-bit) Next when calling mfb_update function the buffer will be copied over to the window and displayed. Currently the mfb_update will return -1 if ESC key is pressed but later on it will support to return a key code for a pressed button. See https://github.com/emoon/minifb/blob/master/tests/noise.c for a complete example

MiniFB has been tested on Windows, Mac OS X and Linux but may of course have trouble depending on your setup. Currently the code will not do any converting of data if not a proper 32-bit display can be created.

Build instructions
------------------

MiniFB uses tundra https://github.com/deplinenoise/tundra as build system and is required to build the code as is but not many changes should be needed if you want to use it directly in your own code.

Mac
---

Cocoa and clang is assumed to be installed on the system (downloading latest XCode + installing the command line tools should do the trick) then to build run: tundra2 macosx-clang-debug and you should be able to run the noise example (t2-output/macosx-clang-debug-default/noise)

Windows
-------

Visual Studio (ver 2012 express has been tested) tools needed (using the vcvars32.bat (for 32-bit) will set up the enviroment) to build run: tundra2 win32-msvc-debug and you should be able to run noise in t2-output/win32-msvc-debug-default/noise.exe


x11 (FreeBSD, Linux, *nix)
--------------------------

gcc and x11-dev libs needs to be installed. To build the code run tundra2 x11-gcc-debug and you should be able to run t2-output/x11-gcc-debug-default/noise

