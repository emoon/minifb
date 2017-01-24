
StaticLibrary {
	Name = "minifb",

	Env = { CPPPATH = { "include", }, },

	Sources = FGlob {
		Dir = "src",
		Extensions = { ".cpp", ".c", ".h", ".s", ".m" },
		Filters = {
			{ Pattern = "[/\\]windows[/\\]"; Config = { "win32-*", "win64-*" } },
			{ Pattern = "[/\\]macosx[/\\]"; Config = "mac*-*" },
			{ Pattern = "[/\\]x11[/\\]"; Config = { "x11-*" } },
			{ Pattern = "[/\\]wayland[/\\]"; Config = { "wayland-*" } },
		},

		Recursive = true,
	},

	Propagate = {
		Libs = {
			"user32.lib"; Config = "win32-*",
			"ws2_32.lib"; Config = "win32-*",
			"gdi32.lib"; Config = "win32-*",
		},

		Frameworks = { "Cocoa" },
	},
}

Program {

	Name = "noise",

	Env = { CPPPATH = { "include", }, },

	Depends = { "minifb" },
	Sources = { "tests/noise.c" }, 

	Libs = {
           { "X11"; Config = "x11-*" },
           { "wayland-client", "wayland-cursor"; Config = "wayland-*" },
        },
}

Default "noise"
