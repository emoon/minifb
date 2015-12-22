require "tundra.syntax.glob"
local native = require('tundra.native')

local win32 = {
	Env = {
 		GENERATE_PDB = "1",
		CCOPTS = {
			"/FS",
			"/W4", 
			"/WX", "/I.", "/D_CRT_SECURE_NO_WARNINGS",
			{ "/Od"; Config = "*-*-debug" },
			{ "/O2"; Config = "*-*-release" },
		},

		PROGOPTS = {
			"/INCREMENTAL:NO"-- Disable incremental linking. It doesn't work properly in our use case (nearly all code in libs) and causes log spam.
		},

	},
}

local macosx = {
	Env = {
		CCOPTS = {
			"-Wpedantic", "-Werror", "-Wall",
			{ "-O0", "-g"; Config = "*-*-debug" },
			{ "-O3"; Config = "*-*-release" },
		},
	},

	Frameworks = { "Cocoa" },
}

local x11 = {
	Env = {
           CPPPATH = { "/usr/include", },
           CCOPTS = {
			"-Wpedantic", "-Werror", "-Wall",
			{ "-O0", "-g"; Config = "*-*-debug" },
			{ "-O3"; Config = "*-*-release" },
           },
        },
}


Build {
	IdeGenerationHints = {
		Msvc = {
		PlatformMappings = {
			['win32-msvc'] = 'Win32',
			['win32-msvc'] = 'Win64',
		},
		FullMappings = {
			['win32-msvc-debug-default']         = { Config='Debug',              Platform='Win32' },
			['win32-msvc-production-default']    = { Config='Production',         Platform='Win32' },
			['win32-msvc-release-default']       = { Config='Release',            Platform='Win32' },
			['win64-msvc-debug-default']         = { Config='Debug',              Platform='Win64' },
			['win64-msvc-production-default']    = { Config='Production',         Platform='Win64' },
			['win64-msvc-release-default']       = { Config='Release',            Platform='Win64' },
			},
		},
		MsvcSolutions = { ['minfb.sln'] = { } },
	},

	Configs = {
		Config { Name = "win32-msvc", Inherit = win32, Tools = { "msvc" }, SupportedHosts = { "windows" }, },
		Config { Name = "win64-msvc", Inherit = win32, Tools = { "msvc" }, SupportedHosts = { "windows" }, },
		Config { Name = "macosx-clang", Inherit = macosx, Tools = { "clang-osx" }, SupportedHosts = { "macosx" },},
		Config { Name = "x11-gcc", Inherit = x11, Tools = { "gcc" }, SupportedHosts = { "linux", "freebsd" },},
		Config { Name = "wayland-gcc", Inherit = x11, Tools = { "gcc" }, SupportedHosts = { "linux" },},
		-- Config { Name = "x11-clang", Inherit = x11, Tools = { "clang" }, SupportedHosts = { "linux", "freebsd" },},
	},

	Units = {
		"units.lua",
	},
}
