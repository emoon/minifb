require "tundra.syntax.glob"
local native = require('tundra.native')

local win32 = {
	Env = {
 		GENERATE_PDB = "1",
		CCOPTS = {
			"/FS",
			"/W4", 
			"/WX", "/I.", "/D_CRT_SECURE_NO_WARNINGS", "\"/DOBJECT_DIR=$(OBJECTDIR:#)\"",
			{ "/Od"; Config = "*-*-debug" },
			{ "/O2"; Config = "*-*-release" },
		},

		PROGOPTS = {
			"/INCREMENTAL:NO"-- Disable incremental linking. It doesn't work properly in our use case (nearly all code in libs) and causes log spam.
		},

	},

	ReplaceEnv = {
		["OBJCCOM"] = "dummy",	-- no ObjC compiler
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

Build {
	Configs = {
		Config { Name = "win32-msvc", Inherit = win32, Tools = { "msvc" }, SupportedHosts = { "windows" }, },
		Config { Name = "win64-msvc", Inherit = win32, Tools = { "msvc" }, SupportedHosts = { "windows" }, },
		Config { Name = "macosx-clang", Inherit = macosx, Tools = { "clang-osx" }, SupportedHosts = { "macosx" },},
	},

	Units = {
		"units.lua",
	},
}
