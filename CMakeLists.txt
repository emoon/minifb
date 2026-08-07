cmake_minimum_required(VERSION 3.16)
project(MiniFB VERSION 0.10.1 LANGUAGES C CXX)

message(STATUS "Processing " ${PROJECT_NAME})

# CMake configuration
#--------------------------------------

# prefer these over general `UNIX` or `APPLE`
# `UNIX` also overlaps MacOS, iOS, and Emscripten, which we don't want
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(LINUX TRUE)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(MACOS TRUE)
endif()

if (MACOS OR IOS)
    include(CheckLanguage)
    check_language(OBJC)
    if (CMAKE_OBJC_COMPILER)
        enable_language(OBJC)
    endif()
endif()

include(GNUInstallDirs)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/minifb_cmake_helpers.cmake")

# use IDE folders in build systems that support it. enabled by default in cmake 3.26+
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# directory to add generated headers (created during configuration)
set(MINIFB_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${output_dir}")

# generate the version header from the templace (.in)
minifb_compute_git_metadata(
    MINIFB_COMMITS_SINCE_TAG
    MINIFB_COMMIT_COUNT
    MINIFB_GIT_SHA
    MINIFB_GIT_DIRTY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/minifb_version.h.in"
    "${MINIFB_GENERATED_INCLUDE_DIR}/minifb_version.h"
    @ONLY
)

# Sources
#--------------------------------------
set(SrcLib
    include/MiniFB.h
    include/MiniFB_cpp.h
    include/MiniFB_enums.h
    include/MiniFB_keylist.h
    include/MiniFB_macros.h
    include/MiniFB_types.h

    # remember to include generated headers as source files
    ${MINIFB_GENERATED_INCLUDE_DIR}/minifb_version.h

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
set(SrcMacOS
    src/macos/MacMiniFB.m
    src/macos/OSXWindow.h
    src/macos/OSXWindow.m
    src/macos/OSXView.h
    src/macos/OSXView.m
    src/macos/WindowData_OSX.h
)

set(SrcMacOSMetal
    src/macos/OSXViewDelegate.h
    src/macos/OSXViewDelegate.m
)

#--
set(SrcIOS
    include/MiniFB_ios.h

    src/ios/WindowData_IOS.h
    src/ios/iOSMiniFB.m
    src/ios/iOSView.h
    src/ios/iOSView.m
    src/ios/iOSViewController.h
    src/ios/iOSViewController.m
    src/ios/iOSViewDelegate.h
    src/ios/iOSViewDelegate.m
)

#--
set(SrcWayland
    src/wayland/generated/xdg-shell-protocol.c
    src/wayland/generated/xdg-decoration-protocol.c
    src/wayland/generated/fractional-scale-v1-protocol.c
    src/wayland/WaylandMiniFB.c
    src/wayland/WindowData_Way.h
    src/MiniFB_linux.c
)

#--
set(SrcX11
    src/x11/X11MiniFB.c
    src/x11/X11MiniFB_scale_funcs.c
    src/x11/X11MiniFB_scale_funcs.h
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
    src/web/WindowData_Web.h
)

#--
set(SrcDOS
    src/dos/DOSMiniFB.c
    src/dos/vesa.c
)

# Options
#--------------------------------------
option(MINIFB_BUILD_EXAMPLES "Build minifb example programs" TRUE)
option(MINIFB_AVOID_CPP_HEADERS "Avoid including C++ Headers" FALSE)

# purely for internal use, never exposed to the user
set(MINIFB_USE_X11_API OFF)

# unfortunatly these old deprecated options can't fit nicely into dependent options due to our custom function, but when we remove these, we can remove this whole block
if (MACOS)
	minifb_apply_deprecated_option(USE_METAL_API MINIFB_USE_METAL_API "Build the project using Metal API code")
	minifb_apply_deprecated_option(USE_INVERTED_Y_ON_MACOS MINIFB_USE_INVERTED_Y_ON_MACOS "Use default mouse position: (0, 0) at (left, down)")
elseif (LINUX)
    minifb_apply_deprecated_option(USE_WAYLAND_API MINIFB_USE_WAYLAND_API "Build the project using Wayland API code")
	if (NOT MINIFB_USE_WAYLAND_API)
		set(MINIFB_USE_X11_API ON)
		minifb_apply_deprecated_option(USE_OPENGL_API MINIFB_USE_OPENGL_API "Build the project using OpenGL API code")
	endif()
elseif (WIN32)
    minifb_apply_deprecated_option(USE_OPENGL_API MINIFB_USE_OPENGL_API "Build the project using OpenGL API code")
endif()

if (NOT MINIFB_HAVE_SET_LEGACY_OPTION) # only use new options if no legacy options are used to prevent conflicts

if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.22)

    # requires 3.22+ to use full condition syntax, technically could use in 3.16, but it would be a lot worse
    include(CMakeDependentOption)

    # cmake_dependent_option is used to create a default value for options in case their specific condition isn't met.
    # the first 3 args are the same as a normal option(), but the 4th is a condition, where if it's true, expose the option, and if it isn't, the 5th argument is a default value to use.
    # setting the default value lets us de-nest `if(<platform>) if(<option>)` type stuff, into just `if(<option>)`.

    cmake_dependent_option(MINIFB_BUILD_VERSION_INFO "Build version info utility" ON
        "NOT IOS AND NOT ANDROID AND NOT EMSCRIPTEN" OFF)

    cmake_dependent_option(MINIFB_USE_METAL_API "Build the project using Metal API code" ON
        "MACOS" OFF)
    cmake_dependent_option(MINIFB_USE_INVERTED_Y_ON_MACOS "Use default mouse position: (0, 0) at (left, down)" OFF
        "MACOS" OFF)

    cmake_dependent_option(MINIFB_USE_WAYLAND_API "Build the project using Wayland API code" OFF
        "LINUX" OFF)

    if (NOT MINIFB_USE_WAYLAND_API AND LINUX)
        set(MINIFB_USE_X11_API ON)
    endif()

    cmake_dependent_option(MINIFB_USE_OPENGL_API "Build the project using OpenGL API code" ON
        "MINIFB_USE_X11_API OR WIN32" OFF)

else()

    # even if an `option()` isn't reached, if it's been reached in previous configurations then the old value still exists as a cache variable
    # this means it can still be seen and edited by tools such as cmake-gui, without any indication that it's not meant to be enabled
    # this can lead to cases where two conflicting options are active, so we need to manually `set()` values OFF if they're not reached
    # dependent options fix this, which is why they should be prioritised if available

    if (NOT IOS AND NOT ANDROID AND NOT EMSCRIPTEN)
        option(MINIFB_BUILD_VERSION_INFO "Build version info utility" ON)
    else()
        set(MINIFB_BUILD_VERSION_INFO OFF)
    endif()

    if (MACOS)
        option(MINIFB_USE_METAL_API "Build the project using Metal API code" ON)
        option(MINIFB_USE_INVERTED_Y_ON_MACOS "Use default mouse position: (0, 0) at (left, down)" OFF)
    else()
        set(MINIFB_USE_METAL_API OFF)
        set(MINIFB_USE_INVERTED_Y_ON_MACOS OFF)
    endif()

    if (LINUX)
        option(MINIFB_USE_WAYLAND_API "Build the project using Wayland API code" OFF)
    else()
        set(MINIFB_USE_WAYLAND_API OFF)
        set(MINIFB_USE_X11_API ON)
    endif()

    if (WIN32 OR MINIFB_USE_X11_API)
        option(MINIFB_USE_OPENGL_API "Build the project using OpenGL API code" ON)
    else()
        set(MINIFB_USE_OPENGL_API OFF)
    endif()

endif()

endif()

# Compilation configuration (library and examples)
#--------------------------------------
function(minifb_configure_target target)
    if (NOT MSVC)
        # GCC/Clang
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wno-switch
            -Wno-unused-function
            -Wno-unused-parameter
            -Wno-implicit-fallthrough
            "$<IF:$<CONFIG:Debug>,-O0,-O2>"
            "$<$<CONFIG:Debug>:-g>"
        )

    else()
        # MSVC
        target_compile_options(${target} PRIVATE
            # Security check
            /GS
            # Function level linking
            /Gy
            # Exceptions
            /EHsc
            # Fast floating point maths
            /fp:fast
            # Force Visual Studio to actualize __cplusplus version macro
            /Zc:__cplusplus
            # Additional security checks
            /sdl

            # Program database (edit and continue in debug mode)
            "$<IF:$<CONFIG:Debug>,/ZI,/Zi>"
            # Optimizations
            "$<IF:$<CONFIG:Debug>,/Od,/O2>"
            # Inline function expansion
            "$<IF:$<CONFIG:Debug>,/Ob0,/Ob2>"
            # Basic runtime checks
            "$<$<CONFIG:Debug>:/RTC1>"
        )

        # prevent annoying c standard lib unsafety msvc warnings
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()

    target_compile_definitions(${target} PRIVATE
        "$<$<CONFIG:Debug>:_DEBUG>"
        "$<$<CONFIG:Debug>:DEBUG>"
    )

    if (WIN32)
        target_compile_definitions(${target} PRIVATE _WIN32_WINNT=0x0601)  # minimum supported version (and header version) = windows 7 (we are in 2026)
    endif()

    # DJGPP headers expose key DOS APIs only in GNU (non-strict ANSI) mode.
    if (DJGPP)
        set(MINIFB_C_EXTENSIONS   ON)
        set(MINIFB_CXX_EXTENSIONS ON)
    else()
        set(MINIFB_C_EXTENSIONS   OFF)
        set(MINIFB_CXX_EXTENSIONS OFF)
    endif()

    set_target_properties(${target} PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS ${MINIFB_C_EXTENSIONS}
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS ${MINIFB_CXX_EXTENSIONS}
    )

endfunction()

# Create library target and configure
#--------------------------------------
add_library(minifb STATIC ${SrcLib})
add_library(minifb::minifb ALIAS minifb)

set_property(TARGET minifb PROPERTY FOLDER "Libs")
minifb_configure_target(minifb)

# headers required for users (differs depending on if it's been built or installed)
target_include_directories(minifb PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>  # most headers
    $<BUILD_INTERFACE:${MINIFB_GENERATED_INCLUDE_DIR}>    # auto-generated headers
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)     # (both of those are in the same location if installed)

target_include_directories(minifb PRIVATE src)  # include internal headers

# conditional main library target configuration

if (MINIFB_USE_INVERTED_Y_ON_MACOS)
    target_compile_definitions(minifb PRIVATE USE_INVERTED_Y_ON_MACOS)
endif()

# Add GL/Metal code if we're using GL/Metal, no matter the specific platform
if (MINIFB_USE_OPENGL_API)
    target_sources(minifb PRIVATE ${SrcGL})
    target_compile_definitions(minifb PRIVATE USE_OPENGL_API)
endif()
if (MINIFB_USE_METAL_API)
    target_compile_definitions(minifb PRIVATE USE_METAL_API)
    target_sources(minifb PRIVATE ${SrcMacOSMetal})
endif()

if (WIN32)
    target_sources(minifb PRIVATE ${SrcWindows})

elseif (IOS)
    target_sources(minifb PRIVATE ${SrcIOS})

elseif (MACOS)
    target_sources(minifb PRIVATE ${SrcMacOS})

elseif (MINIFB_USE_WAYLAND_API)
    target_sources(minifb PRIVATE ${SrcWayland})

elseif (MINIFB_USE_X11_API)
    target_sources(minifb PRIVATE ${SrcX11})

elseif (EMSCRIPTEN)
    target_sources(minifb PRIVATE ${SrcWeb})

elseif (DJGPP)
    target_sources(minifb PRIVATE ${SrcDOS})
endif()

# Linking main library
#--------------------------------------
if (MINIFB_USE_OPENGL_API)
    find_package(OpenGL)
    if (NOT OpenGL_FOUND)
        if (WIN32)
            message(FATAL_ERROR "OpenGL not found for Windows; install the required SDK components")
        elseif (MINIFB_USE_X11_API)
            message(FATAL_ERROR "OpenGL development files not found (install OpenGL headers/libs, e.g. mesa-common-dev/libgl1-mesa-dev)")
        endif()
    endif()

    target_link_libraries(minifb PRIVATE OpenGL::GL)
endif()

if (IOS)
    target_link_libraries(minifb PRIVATE
        "-framework UIKit"
        "-framework QuartzCore"
        "-framework Metal"
        "-framework MetalKit"
    )

elseif (MACOS)
    # TODO: do we need to link cocoa if we're using metal, or metal if we're using cocoa?
    target_link_libraries(minifb PRIVATE
        "-framework Cocoa"
        "-framework QuartzCore"
        "-framework Metal"
        "-framework MetalKit"
    )

elseif (MINIFB_USE_WAYLAND_API)
    # wayland doesn't as of yet have a built-in cmake finder, so we use PkgConfig instead
    find_package(PkgConfig)
    if (NOT PKG_CONFIG_FOUND)
        message(FATAL_ERROR "pkg-config is required to locate Wayland development files")
    endif()
    pkg_check_modules(WAYLAND REQUIRED wayland-client wayland-cursor xkbcommon)

    target_include_directories(minifb PRIVATE ${WAYLAND_INCLUDE_DIRS})
    target_link_directories(minifb PRIVATE ${WAYLAND_LIBRARY_DIRS})
    target_link_libraries(minifb PRIVATE ${WAYLAND_LIBRARIES})

elseif(MINIFB_USE_X11_API)
    find_package(X11)
    if (NOT X11_FOUND)
        message(FATAL_ERROR "X11 development files not found (install X11 headers/libs, e.g. libx11-dev)")
    endif()
    target_link_libraries(minifb PRIVATE X11::X11)

    if (X11_Xrandr_FOUND)
        target_link_libraries(minifb PRIVATE X11::Xrandr)
        target_compile_definitions(minifb PRIVATE MINIFB_HAS_XRANDR)
    endif()

elseif (EMSCRIPTEN)
    # emscripten has no dedicated linking code

elseif (WIN32)
    target_link_libraries(minifb PRIVATE "winmm.lib")  # windows multimedia library, used in our case for precise timing

endif()

# Examples and other targets
#--------------------------------------

# example use: minifb_add_example(NAME noise SOURCE examples/noise.c)
function(minifb_add_example)
    cmake_parse_arguments(PARSE_ARGV 0 arg "IOS_STORYBOARD IOS_NO_STORYBOARD" "NAME" "SOURCE")

    add_executable(${arg_NAME} ${arg_SOURCE})
    target_link_libraries(${arg_NAME} PRIVATE minifb)
    minifb_configure_target(${arg_NAME})

    set_property(TARGET ${arg_NAME} PROPERTY FOLDER "Examples")

    # conditional configuration
    if (EMSCRIPTEN)
        target_link_options(${arg_NAME} PRIVATE
            "-sEXPORT_NAME=${arg_NAME}"
            "-sSTRICT=1"
            "-sENVIRONMENT=web"
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

        add_custom_command(TARGET ${arg_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
                "${CMAKE_CURRENT_SOURCE_DIR}/examples/web/${arg_NAME}.html"
                "${CMAKE_CURRENT_BINARY_DIR}"
            )

    elseif (IOS)
        # TODO: why do examples need to include internal headers?
        target_include_directories(${arg_NAME} PRIVATE src src/ios)

        if (${arg_IOS_STORYBOARD})
            set_target_properties(${arg_NAME} PROPERTIES
                MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/examples/ios/Info.plist"
                RESOURCE "examples/ios/LaunchScreen.storyboard"
            )
        elseif (${arg_IOS_NO_STORYBOARD})
            set_target_properties(${arg_NAME} PROPERTIES
                MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/examples/ios/Info.no_storyboard.plist"
            )
        endif()
    endif()
endfunction()

if (MINIFB_BUILD_VERSION_INFO)
    minifb_add_example(NAME minifb_version_info SOURCE examples/version_info.c)
endif()

if (MINIFB_BUILD_EXAMPLES)
    if (NOT IOS)
        minifb_add_example(NAME noise                SOURCE examples/noise.c)
        minifb_add_example(NAME input_events         SOURCE examples/input_events.c)
        minifb_add_example(NAME hidpi                SOURCE examples/hidpi.c)
        minifb_add_example(NAME multiple_windows     SOURCE examples/multiple_windows.c)
        minifb_add_example(NAME timer                SOURCE examples/timer.c)
        if (NOT EMSCRIPTEN)
            minifb_add_example(NAME fullscreen       SOURCE examples/fullscreen.c)
            minifb_add_example(NAME hide_cursor      SOURCE examples/hide_cursor.c)
            minifb_add_example(NAME input_events_cpp SOURCE examples/input_events_cpp.cpp)
        endif()
        if (DJGPP)
            minifb_add_example(NAME debug_dos        SOURCE examples/dos/debug_dos.c)
        endif()

    else() # iOS
        set(IOS_NOISE_COMMON_SOURCES
            examples/ios/main.m
            examples/ios/AppDelegate.h
            examples/ios/AppDelegate.m
        )

        minifb_add_example(NAME noise               SOURCE ${IOS_NOISE_COMMON_SOURCES} IOS_STORYBOARD)
        minifb_add_example(NAME noise_no_storyboard SOURCE ${IOS_NOISE_COMMON_SOURCES} IOS_NO_STORYBOARD)

        # TODO: this is an IOS build, why do we need to set a minimum macos version?
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Set CMake deployment target" ${FORCE_CACHE})

    endif()

    # example that will be ran on actions such as f5 in visual studio
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT noise)
endif()

# Configure installation
#--------------------------------------

# install the library, and all headers
install(TARGETS minifb EXPORT minifb)
install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/include/" "${MINIFB_GENERATED_INCLUDE_DIR}"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# generate and install files required to find_package minifb
include(CMakePackageConfigHelpers)
set(MINIFB_CMAKE_CONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/minifb")

# generate the cmake code that will create the minifb:: target when found, to minifb-targets.cmake
install(EXPORT minifb
    FILE minifb-targets.cmake
    DESTINATION "${MINIFB_CMAKE_CONFIG_INSTALL_DIR}"
    NAMESPACE minifb::
)

# generate file from our template that will be found directly by find_package (<name>-config.cmake is one of file filenames it searches for)
# the file will find dependancies and `include`s the previously created minifb-targets.cmake
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/minifb-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/minifb-config.cmake"
    INSTALL_DESTINATION "${MINIFB_CMAKE_CONFIG_INSTALL_DIR}"
)
# in case find_package is called with the VERSION flag, generate the file that checks the version
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/minifb-config-version.cmake"
    COMPATIBILITY SameMajorVersion
)
# install the previous two files
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/minifb-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/minifb-config-version.cmake"
    DESTINATION "${MINIFB_CMAKE_CONFIG_INSTALL_DIR}"
)

message(STATUS "Done " ${PROJECT_NAME})
