cmake_minimum_required(VERSION 3.5)

set(PROJECT_NAME MiniFB)
project(${PROJECT_NAME})

message("Processing " ${PROJECT_NAME})

if(NOT DEFINED IOS)
    if(DEFINED CMAKE_SYSTEM_NAME)
        string(TOLOWER CMAKE_SYSTEM_NAME CMAKE_SYSTEM_NAME_LOWER)
        if(CMAKE_SYSTEM_NAME_LOWER STREQUAL "ios")
            set(IOS true)
        endif()
    endif()
endif()

set(SrcLib
    include/MiniFB.h
    include/MiniFB_cpp.h
    include/MiniFB_enums.h

    src/MiniFB_common.c
    src/MiniFB_cpp.cpp
    src/MiniFB_internal.c
    src/MiniFB_internal.h
    src/MiniFB_timer.c
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
    src/macosx/OSXView.h
    src/macosx/OSXView.m
    src/macosx/OSXViewDelegate.h
    src/macosx/OSXViewDelegate.m
    src/macosx/WindowData_OSX.h
)

set(SrcIOS
    src/ios/WindowData_IOS.h
    src/ios/iOSMiniFB.m
    src/ios/iOSView.h
    src/ios/iOSView.m
    src/ios/iOSViewController.h
    src/ios/iOSViewController.m
    src/ios/iOSViewDelegate.h
    src/ios/iOSViewDelegate.m
    include/MiniFB_ios.h
)

set(SrcWayland
    src/wayland/WaylandMiniFB.c
    src/wayland/WindowData_Way.h
    src/MiniFB_linux.c
)

set(SrcX11
    src/x11/X11MiniFB.c
    src/x11/WindowData_X11.h
    src/MiniFB_linux.c
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

# Set features
#--------------------------------------
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
    set (CMAKE_OBJC_FLAGS "${CMAKE_C_FLAGS}")
    set (CMAKE_OBJCXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

# Set default cmake flags
#--------------------------------------
if (APPLE AND NOT IOS) 
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
elseif (IOS)
    list(APPEND SrcLib ${SrcIOS})
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

# Library
#--------------------------------------
add_library(minifb STATIC 
    ${SrcLib}
)

#--------------------------------------
if (APPLE)
    if(IOS)
        target_link_libraries(minifb 
            "-framework UIKit"
            "-framework QuartzCore"
            "-framework Metal"
            "-framework MetalKit"
        )
    else()
        target_link_libraries(minifb
            "-framework Cocoa"
            "-framework QuartzCore"
            "-framework Metal"
            "-framework MetalKit"
        )
    endif()
elseif (UNIX)
    if(USE_WAYLAND_API)
        target_link_libraries(minifb 
            "-lwayland-client"
            "-lwayland-cursor"
        )
    else()
        target_link_libraries(minifb 
            "-lX11"
        )
    endif()
endif()

# For all projects
#--------------------------------------
target_include_directories(minifb PUBLIC  ${CMAKE_CURRENT_LIST_DIR}/include)
target_include_directories(minifb PRIVATE src)

link_libraries(minifb)

# Examples
#--------------------------------------
if(NOT IOS)

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
    
else()

    add_executable(noise 
        tests/ios/main.m
        tests/ios/AppDelegate.h
        tests/ios/AppDelegate.m
    )

    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Set CMake deployment target" ${FORCE_CACHE})

    target_include_directories(noise PRIVATE src)
    target_include_directories(noise PRIVATE src/ios)

    add_definitions(-DTVOS_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET})

endif()

message("Done " ${PROJECT_NAME})
