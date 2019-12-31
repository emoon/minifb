cmake_minimum_required(VERSION 2.8.11)
project (minifb)

file(GLOB SrcLib       "src/*.c" 
                       "src/*.cpp" 
                       "src/*.h"
                       "include/*.h")

file(GLOB SrcWindows   "src/windows/*.c" 
                       "src/windows/*.h")

file(GLOB SrcMacOSX    "src/macosx/*.c" 
                       "src/macosx/*.cpp" 
                       "src/macosx/*.m" 
                       "src/macosx/*.mm" 
                       "src/macosx/*.h")

file(GLOB SrcWayland   "src/wayland/*.c")

file(GLOB SrcX11       "src/x11/*.c")

if (NOT MSVC)
    set (CMAKE_C_FLAGS "-g -Wall -Wextra -pedantic -Wno-switch -Wno-unused-function -Wno-implicit-fallthrough")
    set (CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++11")
    set (CMAKE_OBJC_FLAGS "${CMAKE_C_FLAGS}")
    set (CMAKE_OBJCXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

# Set default cmake flags
if (APPLE) 
    OPTION(USE_METAL_API "Build the project using metal API code" ON)
elseif (UNIX)
    OPTION(USE_WAYLAND_API "Build the project using wayland API code" OFF)
endif()

# Set compiler/platform specific flags and dependencies
if (MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-D_WIN32_WINNT=0x0600)

    list (APPEND SrcLib ${SrcWindows})
elseif (MINGW)
    add_definitions(-D_WIN32_WINNT=0x0600)

    list(APPEND SrcLib ${SrcWindows})
elseif (APPLE)
    if(USE_METAL_API)
        add_definitions(-DUSE_METAL_API)
    endif()
    
    link_libraries("-framework Cocoa")
    link_libraries("-framework QuartzCore")
    link_libraries("-framework Metal")
    link_libraries("-framework MetalKit")
    
    list(APPEND SrcLib ${SrcMacOSX})
elseif (UNIX)
    if(USE_WAYLAND_API)
        link_libraries("-lwayland-client")
        link_libraries("-lwayland-cursor")

        list(APPEND SrcLib ${SrcWayland})
    else()
        link_libraries("-lX11")

        list(APPEND SrcLib ${SrcX11})
    endif()
endif()

# Library
add_library(minifb STATIC 
    ${SrcLib}
)

# For all projects
include_directories(include src)
link_libraries(minifb)

# Examples
add_executable(noise 
    tests/noise.c
)

add_executable(input_events 
    tests/input_events.c
)

add_executable(input_events_cpp
    tests/input_events_cpp.cpp
)

add_executable(multiple_windows
    tests/multiple_windows.c
)
