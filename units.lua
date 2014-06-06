
StaticLibrary {
	Name = "minifb",

	Env = { CPPPATH = { "include", }, },

	Sources = FGlob {
		Dir = "src",
		Extensions = { ".cpp", ".c", ".h", ".s", ".m" },
		Filters = {
			{ Pattern = "[/\\]windows[/\\]"; Config = { "win32-*", "win64-*" } },
			{ Pattern = "[/\\]macosx[/\\]"; Config = "mac*-*" },
			{ Pattern = "[/\\]unix[/\\]"; Config = { "freebsd*-*", "linux*-*" } },
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
}

Default "noise"
