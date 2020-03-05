MiniFB
======

MiniFB (Mini FrameBuffer) is a small cross platform library that makes it easy to render (32-bit) pixels in a window. An example is the best way to show how it works:

	struct mfb_window *window = mfb_open_ex("my display", 800, 600, WF_RESIZABLE);
	if (!window)
		return 0;

	for (;;)
	{
		int state;

		// TODO: add some fancy rendering to the buffer of size 800 * 600

		state = mfb_update(buffer);

		if (state < 0)
			break;
	}


Furthermore, you can add callbacks to the windows:

	void active(struct mfb_window *window, bool isActive) {
		...
	}

	void resize(struct mfb_window *window, int width, int height) {
		...
		// Optionally you can also change the viewport size
		mfb_set_viewport(window, x, y, width, height);
	}

	void keyboard(struct mfb_window *window, mfb_key key, mfb_key_mod mod, bool isPressed) {
		...
		// Remember to close the window in some way
		if(key == KB_KEY_ESCAPE) {
			mfb_close(window);
		}

		fprintf(stdout, "%s > keyboard: key: %s (pressed: %d) [key_mod: %x]\n", window_title, mfb_get_key_name(key), isPressed, mod);

	}

	void char_input(struct mfb_window *window, unsigned int charCode) {
		...
	}

	void mouse_btn(struct mfb_window *window, mfb_mouse_button button, mfb_key_mod mod, bool isPressed) {
		...
	}

	// Use wisely this event. It can be sent too often
	void mouse_move(struct mfb_window *window, int x, int y) {
		...
	}

	// Mouse wheel
	void mouse_scroll(struct mfb_window *window, mfb_key_mod mod, float deltaX, float deltaY) {
		...
	}

	struct mfb_window *window = mfb_open_ex("my display", 800, 600, WF_RESIZABLE);
	if (!window)
		return 0;

	mfb_set_active_callback(window, active);
	mfb_set_resize_callback(window, resize);
	mfb_set_keyboard_callback(window, keyboard);
	mfb_set_char_input_callback(window, char_input);
	mfb_set_mouse_button_callback(window, mouse_btn);
	mfb_set_mouse_move_callback(window, mouse_move);
	mfb_set_mouse_scroll_callback(window, mouse_scroll);


Additionally you can set data per window and recover it

	mfb_set_user_data(window, (void *) myData);
	...
	myData = (someCast *) mfb_get_user_data(window);


Finally, you can get information about the events in the window directly:

	bool                mfb_is_window_active(struct mfb_window *window);
	
	unsigned            mfb_get_window_width(struct mfb_window *window);
	unsigned            mfb_get_window_height(struct mfb_window *window);

	int                 mfb_get_mouse_x(struct mfb_window *window);             // Last mouse pos X
	int                 mfb_get_mouse_y(struct mfb_window *window);             // Last mouse pos Y

	float               mfb_get_mouse_scroll_x(struct mfb_window *window);      // Mouse wheel X as a sum. When you call this function it resets.
	float               mfb_get_mouse_scroll_y(struct mfb_window *window);      // Mouse wheel Y as a sum. When you call this function it resets.

	const uint8_t *     mfb_get_mouse_button_buffer(struct mfb_window *window); // One byte for every button. Press (1), Release 0. (up to 8 buttons)

	const uint8_t *     mfb_get_key_buffer(struct mfb_window *window);          // One byte for every key. Press (1), Release 0.


First the code creates window with the mfb_open call that is used to display the data, next it's the applications responsibility to allocate a buffer (which has to be at least the size of the window and in 32-bit) Next when calling mfb_update function the buffer will be copied over to the window and displayed. Currently the mfb_update will return -1 if ESC key is pressed but later on it will support to return a key code for a pressed button. See https://github.com/emoon/minifb/blob/master/tests/noise.c for a complete example

MiniFB has been tested on Windows, Mac OS X and Linux but may of course have trouble depending on your setup. Currently the code will not do any converting of data if not a proper 32-bit display can be created.

Build instructions
------------------

MiniFB uses tundra https://github.com/deplinenoise/tundra as build system and is required to build the code as is but not many changes should be needed if you want to use it directly in your own code.

You can also use CMake as build system.

Mac
---

Cocoa and clang is assumed to be installed on the system (downloading latest XCode + installing the command line tools should do the trick) then to build run: tundra2 macosx-clang-debug and you should be able to run the noise example (t2-output/macosx-clang-debug-default/noise)

MacOS X Mojave does not support Cocoa framework as expected. For that reason now you can switch to Metal API.
To enable it just compile defining the preprocessor macro USE_METAL_API.

If you use CMake just enable the flag:

	mkdir build
	cd build
	cmake .. -DUSE_METAL_API=ON

or if you don't want to use Metal API:

	mkdir build
	cd build
	cmake .. -DUSE_METAL_API=OFF


Windows
-------

Visual Studio (ver 2012 express has been tested) tools needed (using the vcvars32.bat (for 32-bit) will set up the enviroment) to build run: tundra2 win32-msvc-debug and you should be able to run noise in t2-output/win32-msvc-debug-default/noise.exe

If you use CMake the Visual Studio project will be generated (2015, 2017 and 2019 have been tested).


x11 (FreeBSD, Linux, *nix)
--------------------------

gcc and x11-dev libs needs to be installed. To build the code run tundra2 x11-gcc-debug and you should be able to run t2-output/x11-gcc-debug-default/noise

If you use CMake just disable the flag:

	mkdir build
	cd build
	cmake .. -DUSE_WAYLAND_API=OFF


wayland (Linux)
--------------------------

Depends on gcc and wayland-client and wayland-cursor. Built using the wayland-gcc variants.

If you use CMake just enable the flag:

	mkdir build
	cd build
	cmake .. -DUSE_WAYLAND_API=ON
