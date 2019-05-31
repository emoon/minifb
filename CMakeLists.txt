cmake_minimum_required(VERSION 2.8.11)
project (noise)

include_directories(include src)

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
    set (CMAKE_C_FLAGS "-g -Wall -Wextra -pedantic -Wno-switch -Wno-unused-function")
    set (CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++11")
    set (CMAKE_OBJC_FLAGS "${CMAKE_C_FLAGS}")
    set (CMAKE_OBJCXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

if (APPLE) 
    OPTION(USE_METAL_API "Build the project using metal API code" ON)
elseif (UNIX)
    OPTION(USE_WAYLAND_API "Build the project using wayland API code" OFF)
endif()

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
    list(APPEND SrcLib ${SrcMacOSX})
elseif (UNIX)
    if(USE_WAYLAND_API)
        list(APPEND SrcLib ${SrcWayland})
    else()
        list(APPEND SrcLib ${SrcX11})
    endif()
endif()

add_library(minifb STATIC 
    ${SrcLib}
)

add_executable(noise 
    tests/noise2.cpp
)

target_link_libraries(noise minifb)

if (MSVC)
elseif (MINGW)
elseif (APPLE)
    target_link_libraries(noise "-framework Cocoa")
    target_link_libraries(noise "-framework QuartzCore")
    target_link_libraries(noise "-framework Metal")
    target_link_libraries(noise "-framework MetalKit")
elseif (UNIX)
    if(USE_WAYLAND_API)
        target_link_libraries(noise -lwayland-client -lwayland-cursor)
    else()
        target_link_libraries(noise -lX11)
    endif()
endif()
