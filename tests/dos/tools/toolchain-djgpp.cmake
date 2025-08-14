#
# CMake toolchain file for DJGPP. Usage:
#
# 1. Download and extract DGJPP
# 2. Place this file into the root folder of DJGPP
# 3. When configuring your CMake project, specify the toolchain file like this:
#
#   cmake -DCMAKE_TOOLCHAIN_FILE=/path/to/djgpp/toolchain-djgpp.cmake -B build
#
set (CMAKE_SYSTEM_NAME linux-djgpp)

set (DJGPP TRUE)

# specify the cross compiler
if(WIN32)
	set (CMAKE_C_COMPILER ${CMAKE_CURRENT_LIST_DIR}/djgpp/bin/i586-pc-msdosdjgpp-gcc.exe)
	set (CMAKE_CXX_COMPILER ${CMAKE_CURRENT_LIST_DIR}/djgpp/bin/i586-pc-msdosdjgpp-g++.exe)
else()
	set (CMAKE_C_COMPILER ${CMAKE_CURRENT_LIST_DIR}/djgpp/bin/i586-pc-msdosdjgpp-gcc)
	set (CMAKE_CXX_COMPILER ${CMAKE_CURRENT_LIST_DIR}/djgpp/bin/i586-pc-msdosdjgpp-g++)
endif()

# where is the target environment
set (CMAKE_FIND_ROOT_PATH ${CMAKE_CURRENT_LIST_DIR})

# search for programs in the build host directories
set (CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
set (CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set (CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set (CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# add sys-include dir to c/c++ flags by default. ides using
# compile-commands.json can't otherwise find includes.
set (CMAKE_C_FLAGS -I${CMAKE_CURRENT_LIST_DIR}/djgpp/i586-pc-msdosdjgpp/sys-include/)
set (CMAKE_CXX_FLAGS -I${CMAKE_CURRENT_LIST_DIR}/djgpp/i586-pc-msdosdjgpp/sys-include/)