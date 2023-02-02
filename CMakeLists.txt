cmake_minimum_required(VERSION 3.7)

set(PROJECT_NAME MiniFB)
project(${PROJECT_NAME})

message(STATUS "Processing " ${PROJECT_NAME})

include(GNUInstallDirs)

# Detect iOS
#--------------------------------------
if(NOT DEFINED IOS)
    if(DEFINED CMAKE_SYSTEM_NAME)
        string(TOLOWER CMAKE_SYSTEM_NAME CMAKE_SYSTEM_NAME_LOWER)
        if(CMAKE_SYSTEM_NAME_LOWER STREQUAL "ios")
            set(IOS true)
        endif()
    endif()
endif()

# Sources
#--------------------------------------
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

#--
set(SrcWindows
    src/windows/WinMiniFB.c
    src/windows/WindowData_Win.h
)

#--
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

#--
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

#--
set(SrcWayland
    src/wayland/WaylandMiniFB.c
    src/wayland/WindowData_Way.h
    src/MiniFB_linux.c
)

#--
set(SrcX11
    src/x11/X11MiniFB.c
    src/x11/WindowData_X11.h
    src/MiniFB_linux.c
)

set(SrcGL
    src/gl/MiniFB_GL.h
    src/gl/MiniFB_GL.c
)

#--
set(SrcWeb
    src/web/WebMiniFB.c
)

# Set features
#--------------------------------------
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#--------------------------------------
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Code generation options
#--------------------------------------
option(MINIFB_BUILD_EXAMPLES "Build minifb example programs" TRUE)
option(MINIFB_AVOID_CPP_HEADERS "Avoid including C++ Headers" FALSE)

if(APPLE AND NOT IOS)
    option(USE_METAL_API "Build the project using metal API code" ON)
    option(USE_INVERTED_Y_ON_MACOS "Use default mouse position: (0, 0) at (left, down)" OFF)
elseif(UNIX)
    if (NOT EMSCRIPTEN)
        option(USE_WAYLAND_API "Build the project using wayland API code" OFF)
        if(NOT USE_WAYLAND_API)
            option(USE_OPENGL_API "Build the project using OpenGL API code" ON)
        endif()
    endif()
elseif(WIN32)
    option(USE_OPENGL_API "Build the project using OpenGL API code" ON)
endif()

# Set GCC/Clang flags
#--------------------------------------
if(NOT MSVC)
    # Avoid default flag values
    #--------------------------------------
    set(CMAKE_C_FLAGS "")
    set(CMAKE_C_FLAGS_DEBUG "" )
    set(CMAKE_C_FLAGS_RELEASE "")

    set(CMAKE_CXX_FLAGS "")
    set(CMAKE_CXX_FLAGS_DEBUG "")
    set(CMAKE_CXX_FLAGS_RELEASE "")

    set(CMAKE_OBJC_FLAGS "")
    set(CMAKE_OBJC_FLAGS_DEBUG "")
    set(CMAKE_OBJC_FLAGS_RELEASE "")

    set(CMAKE_OBJCXX_FLAGS "")
    set(CMAKE_OBJCXX_FLAGS_DEBUG "")
    set(CMAKE_OBJCXX_FLAGS_RELEASE "")

    # Set our flags
    #--------------------------------------
    add_compile_options("$<$<CONFIG:Debug>:-g>")
    add_compile_options("$<IF:$<CONFIG:Debug>,-O0,-O2>")
    add_compile_options(-Wall -Wextra)
    add_compile_options(-Wno-switch -Wno-unused-function -Wno-unused-parameter -Wno-implicit-fallthrough)

    if(NOT APPLE)
        add_compile_options(-Wno-cast-function-type)
    endif()
else()
    # Security check
    add_compile_options(/GS)
    # Function level linking
    add_compile_options(/Gy)
    # Exceptions
    add_compile_options(/EHsc)
    if(MSVC_VERSION GREATER_EQUAL 1900)
        # SDL checks 2015+
        add_compile_options(/sdl)
    endif()
    if(MSVC_VERSION LESS_EQUAL 1920)
        # Enable Minimal Rebuild (required for Edit and Continue) (deprecated)
        add_compile_options(/Gm)
    endif()
    add_compile_options(/fp:fast)
    # Program database for edit and continue
    add_compile_options("$<IF:$<CONFIG:Debug>,/ZI,/Zi>")
    # Optimizations
    add_compile_options("$<IF:$<CONFIG:Debug>,/Od,/O2>")
    # Inline function expansion
    add_compile_options("$<IF:$<CONFIG:Debug>,/Ob0,/Ob2>")
    # Basic runtime checks
    add_compile_options("$<$<CONFIG:Debug>:/RTC1>")
    # Force Visual Studio to actualize __cplusplus version macro
    add_compile_options(/Zc:__cplusplus)

endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-D_DEBUG)
    add_definitions(-DDEBUG)
    if(EMSCRIPTEN)
        add_link_options(-g)
    endif()
endif()

# Set compiler/platform specific flags and dependencies
#--------------------------------------
if(WIN32)

    if(MSVC)
        add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    endif()
    add_definitions(-D_WIN32_WINNT=0x0601)  # Windows 7 (we are in 2020)

    if(USE_OPENGL_API)
        list(APPEND SrcLib ${SrcGL})

        add_definitions(-DUSE_OPENGL_API)
    endif()

    list(APPEND SrcLib ${SrcWindows})

elseif(IOS)

    list(APPEND SrcLib ${SrcIOS})

elseif(APPLE)

    if(USE_METAL_API)
        add_definitions(-DUSE_METAL_API)
    endif()

    if(USE_INVERTED_Y_ON_MACOS)
        add_definitions(-DUSE_INVERTED_Y_ON_MACOS)
    endif()

    list(APPEND SrcLib ${SrcMacOSX})

elseif(UNIX)

    if(USE_WAYLAND_API)
        list(APPEND SrcLib ${SrcWayland})
    elseif(EMSCRIPTEN)
        list(APPEND SrcLib ${SrcWeb})
    else()
        if(USE_OPENGL_API)
            list(APPEND SrcLib ${SrcGL})

            add_definitions(-DUSE_OPENGL_API)
        endif()
        list(APPEND SrcLib ${SrcX11})
    endif()

endif()

# Library
#--------------------------------------
add_library(minifb STATIC
    ${SrcLib}
)
add_library(minifb::minifb ALIAS minifb)

# Link
#--------------------------------------
if(APPLE)

    if(IOS)
        target_link_libraries(minifb PRIVATE
            "-framework UIKit"
            "-framework QuartzCore"
            "-framework Metal"
            "-framework MetalKit"
        )
    else()
        target_link_libraries(minifb PRIVATE
            "-framework Cocoa"
            "-framework QuartzCore"
            "-framework Metal"
            "-framework MetalKit"
        )
    endif()

elseif(UNIX)

    if(USE_WAYLAND_API)
        target_link_libraries(minifb PRIVATE
            "-lwayland-client"
            "-lwayland-cursor"
        )
    elseif(EMSCRIPTEN)
        add_link_options(
            "-sSTRICT=1"
            "-sENVIRONMENT=web"
            "-sLLD_REPORT_UNDEFINED"
            "-sMODULARIZE=1"
            "-sALLOW_MEMORY_GROWTH=1"
            "-sALLOW_TABLE_GROWTH"
            "-sMALLOC=emmalloc"
            "-sEXPORT_ALL=1"
            "-sEXPORTED_FUNCTIONS=[\"_malloc\",\"_free\",\"_main\"]"
            "-sEXPORTED_RUNTIME_METHODS=ccall,cwrap"
            "-sASYNCIFY"
            "--no-entry"
            "-sSINGLE_FILE"
        )
    else()
        target_link_libraries(minifb PRIVATE
            "-lX11"
            #"-lxkbcommon"
            #"-lXrandr" DPI NOT WORKING
        )
        if(USE_OPENGL_API)
        target_link_libraries(minifb PRIVATE
            "-lGL"
        )
        endif()
    endif()

elseif(WIN32)

    if(USE_OPENGL_API)
        target_link_libraries(minifb PRIVATE
            "Opengl32.lib"
        )
    endif()

    target_link_libraries(minifb PRIVATE
        "winmm.lib"
    )

endif()

# For all projects
#--------------------------------------
target_include_directories(minifb PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_include_directories(minifb PRIVATE src)

link_libraries(minifb)

# Examples
#--------------------------------------
if(MINIFB_BUILD_EXAMPLES)
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

        add_executable(hidpi
            tests/hidpi.c
        )

        add_executable(fullscreen
            tests/fullscreen.c
        )

        add_executable(timer
            tests/timer.c
        )

        if(EMSCRIPTEN)
            add_custom_target(web_assets
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/web
                    ${CMAKE_CURRENT_BINARY_DIR}
            )
            add_dependencies(noise web_assets)
            target_link_options(noise PRIVATE "-sEXPORT_NAME=noise")
            add_dependencies(input_events web_assets)
            target_link_options(input_events PRIVATE "-sEXPORT_NAME=input_events")
            add_dependencies(hidpi web_assets)
            target_link_options(hidpi PRIVATE "-sEXPORT_NAME=hidpi")
            add_dependencies(multiple_windows web_assets)
            target_link_options(multiple_windows PRIVATE "-sEXPORT_NAME=multiple_windows")
            add_dependencies(timer web_assets)
            target_link_options(timer PRIVATE "-sEXPORT_NAME=timer")
        endif()

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

endif()

# Organize IDE Folders
#--------------------------------------
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(TARGET minifb PROPERTY FOLDER "Libs")

if(MINIFB_BUILD_EXAMPLES)
    set_property(TARGET noise PROPERTY FOLDER "Tests")
    set_property(TARGET input_events PROPERTY FOLDER "Tests")
    set_property(TARGET input_events_cpp PROPERTY FOLDER "Tests")
    set_property(TARGET multiple_windows PROPERTY FOLDER "Tests")
    set_property(TARGET hidpi PROPERTY FOLDER "Tests")
endif()

install(TARGETS minifb EXPORT minifb)
file(GLOB_RECURSE HEADERS "${CMAKE_CURRENT_LIST_DIR}/include/*.h")
install(FILES ${HEADERS} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(EXPORT minifb FILE minifb-config.cmake DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/minifb" NAMESPACE minifb::)

message(STATUS "Done " ${PROJECT_NAME})
