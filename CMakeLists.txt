cmake_minimum_required(VERSION 3.5)

set(PROJECT_NAME MiniFB)
project(${PROJECT_NAME})

message("Processing " ${PROJECT_NAME})

set(SrcLib
    include/MiniFB.h
    include/MiniFB_cpp.h
    include/MiniFB_enums.h

    src/MiniFB_common.c
    src/MiniFB_cpp.cpp
    src/MiniFB_internal.c
    src/MiniFB_internal.h
    src/WindowData.h
)

set(SrcWindows
    src/windows/WinMiniFB.c
    src/windows/WindowData_Win.h
)

set(SrcMacOSX
    src/macosx/MacMiniFB.m
    src/macosx/OSXWindow.h
    src/macosx/OSXWindow.m
    src/macosx/OSXWindowFrameView.h
    src/macosx/OSXWindowFrameView.m
    src/macosx/WindowData_OSX.h
)

set(SrcWayland
    src/wayland/WaylandMiniFB.c
    src/wayland/WindowData_Way.h
)

set(SrcX11
    src/x11/X11MiniFB.c
    src/x11/WindowData_X11.h
)

# Avoid RelWithDebInfo and MinSizeRel
#--------------------------------------
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

# Define Release by default
#--------------------------------------
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
    message(STATUS "Build type not specified: Use Release by default")
endif(NOT CMAKE_BUILD_TYPE)

# Set GCC/Clang flags
#--------------------------------------
if (NOT MSVC)
    # Avoid default flag values
    #--------------------------------------
    set(CMAKE_C_FLAGS "" CACHE STRING "" FORCE)
    set(CMAKE_C_FLAGS_DEBUG "" CACHE STRING "" FORCE)
    set(CMAKE_C_FLAGS_RELEASE "" CACHE STRING "" FORCE)

    set(CMAKE_CXX_FLAGS "" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS_DEBUG "" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS_RELEASE "" CACHE STRING "" FORCE)

    set(CMAKE_OBJC_FLAGS "" CACHE STRING "" FORCE)
    set(CMAKE_OBJC_FLAGS_DEBUG "" CACHE STRING "" FORCE)
    set(CMAKE_OBJC_FLAGS_RELEASE "" CACHE STRING "" FORCE)

    set(CMAKE_OBJCXX_FLAGS "" CACHE STRING "" FORCE)
    set(CMAKE_OBJCXX_FLAGS_DEBUG "" CACHE STRING "" FORCE)
    set(CMAKE_OBJCXX_FLAGS_RELEASE "" CACHE STRING "" FORCE)

    # Set our flags
    #--------------------------------------
    add_compile_options("$<$<CONFIG:Debug>:-g>")
    add_compile_options("$<IF:$<CONFIG:Debug>,-O0,-O2>")
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic -Wno-switch -Wno-unused-function -Wno-implicit-fallthrough")
    set (CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++11")
    set (CMAKE_OBJC_FLAGS "${CMAKE_C_FLAGS}")
    set (CMAKE_OBJCXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

# Set default cmake flags
#--------------------------------------
if (APPLE) 
    OPTION(USE_METAL_API "Build the project using metal API code" ON)
elseif (UNIX)
    OPTION(USE_WAYLAND_API "Build the project using wayland API code" OFF)
endif()

# Set compiler/platform specific flags and dependencies
#--------------------------------------
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
#--------------------------------------
add_library(minifb STATIC 
    ${SrcLib}
)

# For all projects
#--------------------------------------
target_include_directories(minifb PUBLIC  include)
target_include_directories(minifb PRIVATE src)
link_libraries(minifb)

# Examples
#--------------------------------------
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

message("Done " ${PROJECT_NAME})
