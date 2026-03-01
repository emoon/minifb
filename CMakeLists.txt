cmake_minimum_required(VERSION 3.16)
project(MiniFB VERSION 0.9.1 LANGUAGES C CXX)

if (APPLE)
    include(CheckLanguage)
    check_language(OBJC)
    if (CMAKE_OBJC_COMPILER)
        enable_language(OBJC)
    endif()
endif()

message(STATUS "Processing " ${PROJECT_NAME})

include(GNUInstallDirs)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/versioning.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/minifb_cmake_helpers.cmake")
if (POLICY CMP0072)
    cmake_policy(SET CMP0072 NEW)  # Prefer GLVND
endif()
set(OpenGL_GL_PREFERENCE "GLVND")

minifb_compute_git_metadata(
    MINIFB_COMMITS_SINCE_TAG
    MINIFB_COMMIT_COUNT
    MINIFB_GIT_SHA
    MINIFB_GIT_DIRTY
)

set(MINIFB_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
minifb_configure_version_header(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/minifb_version.h.in"
    "${MINIFB_GENERATED_INCLUDE_DIR}"
    "${MINIFB_GENERATED_INCLUDE_DIR}/minifb_version.h"
)

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

# Set features
#--------------------------------------
set(MINIFB_C_STANDARD 11)
set(MINIFB_CXX_STANDARD 11)

set(MINIFB_COMPILE_OPTIONS)
set(MINIFB_COMPILE_DEFINITIONS)
set(MINIFB_LINK_OPTIONS_LIB)
set(MINIFB_LINK_OPTIONS_EXE)

if (DJGPP)
    # DJGPP headers expose key DOS APIs only in GNU (non-strict ANSI) mode.
    set(MINIFB_C_EXTENSIONS   ON)
    set(MINIFB_CXX_EXTENSIONS ON)
else()
    set(MINIFB_C_EXTENSIONS   OFF)
    set(MINIFB_CXX_EXTENSIONS OFF)
endif()

# Options
#--------------------------------------
option(MINIFB_BUILD_EXAMPLES "Build minifb example programs" TRUE)
option(MINIFB_AVOID_CPP_HEADERS "Avoid including C++ Headers" FALSE)

if (NOT IOS AND NOT ANDROID AND NOT EMSCRIPTEN)
    option(MINIFB_BUILD_VERSION_INFO "Build version info utility" ON)
else()
    set(MINIFB_BUILD_VERSION_INFO OFF)
endif()

if (APPLE AND NOT IOS)
    option(MINIFB_USE_METAL_API "Build the project using metal API code" ON)
    option(MINIFB_USE_INVERTED_Y_ON_MACOS "Use default mouse position: (0, 0) at (left, down)" OFF)
    minifb_apply_deprecated_option(USE_METAL_API MINIFB_USE_METAL_API "Build the project using metal API code")
    minifb_apply_deprecated_option(USE_INVERTED_Y_ON_MACOS MINIFB_USE_INVERTED_Y_ON_MACOS "Use default mouse position: (0, 0) at (left, down)")
elseif (UNIX AND NOT EMSCRIPTEN)
    option(MINIFB_USE_WAYLAND_API "Build the project using wayland API code" OFF)
    option(MINIFB_USE_OPENGL_API "Build the project using OpenGL API code" ON)
    minifb_apply_deprecated_option(USE_WAYLAND_API MINIFB_USE_WAYLAND_API "Build the project using wayland API code")
    minifb_apply_deprecated_option(USE_OPENGL_API MINIFB_USE_OPENGL_API "Build the project using OpenGL API code")
elseif (WIN32)
    option(MINIFB_USE_OPENGL_API "Build the project using OpenGL API code" ON)
    minifb_apply_deprecated_option(USE_OPENGL_API MINIFB_USE_OPENGL_API "Build the project using OpenGL API code")
endif()

# Set compile flags depending on the compiler
#--------------------------------------
if (NOT MSVC)

    # GCC/Clang

    if (DJGPP)
        list(APPEND MINIFB_COMPILE_OPTIONS
            "$<$<CONFIG:Debug>:-gdwarf>"
            "$<$<CONFIG:Debug>:-save-temps>"
        )
    else()
        list(APPEND MINIFB_COMPILE_OPTIONS
            "$<$<CONFIG:Debug>:-g>"
        )
    endif()

    list(APPEND MINIFB_COMPILE_OPTIONS
        -Wall
        -Wextra
        -Wno-switch
        -Wno-unused-function
        -Wno-unused-parameter
        -Wno-implicit-fallthrough
        "$<IF:$<CONFIG:Debug>,-O0,-O2>"
    )

    if (NOT APPLE)
        list(APPEND MINIFB_COMPILE_OPTIONS
            -Wno-cast-function-type
        )
    endif()

else()

    # MSVC
    #--------------------------------------

    list(APPEND MINIFB_COMPILE_OPTIONS
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

        # Program database for edit and continue
        "$<IF:$<CONFIG:Debug>,/ZI,/Zi>"
        # Optimizations
        "$<IF:$<CONFIG:Debug>,/Od,/O2>"
        # Inline function expansion
        "$<IF:$<CONFIG:Debug>,/Ob0,/Ob2>"
        # Basic runtime checks
        "$<$<CONFIG:Debug>:/RTC1>"
    )

    if (MSVC_VERSION GREATER_EQUAL 1900)
        # SDL checks 2015+
        list(APPEND MINIFB_COMPILE_OPTIONS /sdl)
    endif()

    if (MSVC_VERSION LESS_EQUAL 1920)
        # Enable Minimal Rebuild (required for Edit and Continue) (deprecated)
        list(APPEND MINIFB_COMPILE_OPTIONS /Gm)
    endif()

endif()

list(APPEND MINIFB_COMPILE_DEFINITIONS
    "$<$<CONFIG:Debug>:_DEBUG>"
    "$<$<CONFIG:Debug>:DEBUG>"
)

if (EMSCRIPTEN)
    list(APPEND MINIFB_LINK_OPTIONS_EXE
        "$<$<CONFIG:Debug>:-g>"
    )
endif()

# Set compiler/platform specific flags and dependencies
#--------------------------------------
if (WIN32)

    if (MSVC)
        list(APPEND MINIFB_COMPILE_DEFINITIONS _CRT_SECURE_NO_WARNINGS)
    endif()
    list(APPEND MINIFB_COMPILE_DEFINITIONS _WIN32_WINNT=0x0601)  # Windows 7 (we are in 2026)

    if (USE_OPENGL_API)
        list(APPEND SrcLib ${SrcGL})

        list(APPEND MINIFB_COMPILE_DEFINITIONS USE_OPENGL_API)
    endif()

    list(APPEND SrcLib ${SrcWindows})

elseif (IOS)

    list(APPEND SrcLib ${SrcIOS})

elseif (APPLE)

    if (USE_METAL_API)
        list(APPEND MINIFB_COMPILE_DEFINITIONS USE_METAL_API)
    endif()

    if (USE_INVERTED_Y_ON_MACOS)
        list(APPEND MINIFB_COMPILE_DEFINITIONS USE_INVERTED_Y_ON_MACOS)
    endif()

    list(APPEND SrcLib ${SrcMacOSX})

elseif (UNIX)

    if (USE_WAYLAND_API)
        list(APPEND SrcLib ${SrcWayland})
    elseif (EMSCRIPTEN)
        list(APPEND SrcLib ${SrcWeb})
    else()
        if (USE_OPENGL_API)
            list(APPEND SrcLib ${SrcGL})

            list(APPEND MINIFB_COMPILE_DEFINITIONS USE_OPENGL_API)
        endif()
        list(APPEND SrcLib ${SrcX11})
    endif()

elseif (DJGPP)

    list(APPEND SrcLib ${SrcDOS})

endif()

# Library
#--------------------------------------
add_library(minifb STATIC
    ${SrcLib}
)
add_library(minifb::minifb ALIAS minifb)

# Link
#--------------------------------------
if (APPLE)

    if (IOS)
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

elseif (UNIX)

    if (USE_WAYLAND_API)
        find_package(PkgConfig)
        if (NOT PKG_CONFIG_FOUND)
            message(FATAL_ERROR "pkg-config is required to locate Wayland development files")
        endif()
        pkg_check_modules(WAYLAND REQUIRED wayland-client wayland-cursor xkbcommon)

        target_include_directories(minifb PRIVATE ${WAYLAND_INCLUDE_DIRS})
        target_link_directories(minifb PRIVATE ${WAYLAND_LIBRARY_DIRS})
        target_link_libraries(minifb PRIVATE
            ${WAYLAND_LIBRARIES}
        )
    elseif (EMSCRIPTEN)
        list(APPEND MINIFB_LINK_OPTIONS_EXE
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
    else()
        find_package(X11)
        if (NOT X11_FOUND)
            message(FATAL_ERROR "X11 development files not found (install X11 headers/libs, e.g. libx11-dev)")
        endif()

        target_link_libraries(minifb PRIVATE
            X11::X11
            #"-lxkbcommon"
            #"-lXrandr" DPI NOT WORKING
        )
        if (USE_OPENGL_API)
            find_package(OpenGL)
            if (NOT OpenGL_FOUND)
                message(FATAL_ERROR "OpenGL development files not found (install OpenGL headers/libs, e.g. mesa-common-dev/libgl1-mesa-dev)")
            endif()
            target_link_libraries(minifb PRIVATE
                OpenGL::GL
            )
        endif()
    endif()

elseif (WIN32)

    if (USE_OPENGL_API)
        find_package(OpenGL)
        if (NOT OpenGL_FOUND)
            message(FATAL_ERROR "OpenGL not found for Windows; install the required SDK components")
        endif()
        target_link_libraries(minifb PRIVATE
            "opengl32.lib"
        )
    endif()

    target_link_libraries(minifb PRIVATE
        "winmm.lib"
    )

endif()

# For all projects
#--------------------------------------
set_target_properties(minifb PROPERTIES
    C_STANDARD ${MINIFB_C_STANDARD}
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS ${MINIFB_C_EXTENSIONS}
    CXX_STANDARD ${MINIFB_CXX_STANDARD}
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS ${MINIFB_CXX_EXTENSIONS}
)

if (MINIFB_COMPILE_OPTIONS)
    target_compile_options(minifb PRIVATE ${MINIFB_COMPILE_OPTIONS})
endif()

if (MINIFB_COMPILE_DEFINITIONS)
    target_compile_definitions(minifb PRIVATE ${MINIFB_COMPILE_DEFINITIONS})
endif()

if (MINIFB_LINK_OPTIONS_LIB)
    target_link_options(minifb PRIVATE ${MINIFB_LINK_OPTIONS_LIB})
endif()

target_include_directories(minifb PUBLIC
    $<BUILD_INTERFACE:${MINIFB_GENERATED_INCLUDE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_include_directories(minifb PRIVATE src)

# Other targets
#--------------------------------------
set(MINIFB_TEST_TARGETS)

if (MINIFB_BUILD_VERSION_INFO)
    add_executable(minifb_version_info tests/version_info.c)
    list(APPEND MINIFB_TEST_TARGETS minifb_version_info)
endif()

# Examples
#--------------------------------------
if (MINIFB_BUILD_EXAMPLES)
    if (NOT IOS)

        add_executable(noise tests/noise.c)
        add_executable(input_events tests/input_events.c)
        add_executable(input_events_cpp tests/input_events_cpp.cpp)
        add_executable(multiple_windows tests/multiple_windows.c)
        add_executable(hidpi tests/hidpi.c)
        add_executable(fullscreen tests/fullscreen.c)
        add_executable(timer tests/timer.c)
        add_executable(hide_cursor tests/hide_cursor.c)

        list(APPEND MINIFB_TEST_TARGETS
            noise
            input_events
            input_events_cpp
            multiple_windows
            hidpi
            fullscreen
            timer
            hide_cursor
        )

        if (EMSCRIPTEN)

            add_custom_target(web_assets
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/web
                    ${CMAKE_CURRENT_BINARY_DIR}
            )

            add_dependencies(noise web_assets)
            add_dependencies(input_events web_assets)
            add_dependencies(hidpi web_assets)
            add_dependencies(multiple_windows web_assets)
            add_dependencies(timer web_assets)

            target_link_options(noise PRIVATE "-sEXPORT_NAME=noise")
            target_link_options(input_events PRIVATE "-sEXPORT_NAME=input_events")
            target_link_options(hidpi PRIVATE "-sEXPORT_NAME=hidpi")
            target_link_options(multiple_windows PRIVATE "-sEXPORT_NAME=multiple_windows")
            target_link_options(timer PRIVATE "-sEXPORT_NAME=timer")

        endif()

        if (DJGPP)

            add_executable(dos
                tests/dos/dos.c
            )
            list(APPEND MINIFB_TEST_TARGETS dos)

        endif()

    else() # iOS

        add_executable(noise
            tests/ios/main.m
            tests/ios/AppDelegate.h
            tests/ios/AppDelegate.m
        )
        list(APPEND MINIFB_TEST_TARGETS noise)

        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Set CMake deployment target" ${FORCE_CACHE})

        target_include_directories(noise PRIVATE src)
        target_include_directories(noise PRIVATE src/ios)

        target_compile_definitions(noise PRIVATE TVOS_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET})

    endif()

endif()

# Add options to targets
#--------------------------------------
foreach(minifb_test_target IN LISTS MINIFB_TEST_TARGETS)
    target_link_libraries(${minifb_test_target} PRIVATE minifb)

    set_target_properties(${minifb_test_target} PROPERTIES
        C_STANDARD ${MINIFB_C_STANDARD}
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS ${MINIFB_C_EXTENSIONS}
        CXX_STANDARD ${MINIFB_CXX_STANDARD}
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS ${MINIFB_CXX_EXTENSIONS}
    )

    if (MINIFB_COMPILE_OPTIONS)
        target_compile_options(${minifb_test_target} PRIVATE ${MINIFB_COMPILE_OPTIONS})
    endif()

    if (MINIFB_COMPILE_DEFINITIONS)
        target_compile_definitions(${minifb_test_target} PRIVATE ${MINIFB_COMPILE_DEFINITIONS})
    endif()

    if (MINIFB_LINK_OPTIONS_EXE)
        target_link_options(${minifb_test_target} PRIVATE ${MINIFB_LINK_OPTIONS_EXE})
    endif()
endforeach()

# Organize IDE Folders
#--------------------------------------
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(TARGET minifb PROPERTY FOLDER "Libs")

if (MINIFB_BUILD_EXAMPLES)
    set_property(TARGET noise PROPERTY FOLDER "Tests")
    set_property(TARGET timer PROPERTY FOLDER "Tests")
    set_property(TARGET fullscreen PROPERTY FOLDER "Tests")
    set_property(TARGET input_events PROPERTY FOLDER "Tests")
    set_property(TARGET input_events_cpp PROPERTY FOLDER "Tests")
    set_property(TARGET multiple_windows PROPERTY FOLDER "Tests")
    set_property(TARGET hidpi PROPERTY FOLDER "Tests")
    set_property(TARGET hide_cursor PROPERTY FOLDER "Tests")
endif()

if (MINIFB_BUILD_VERSION_INFO)
    set_property(TARGET minifb_version_info PROPERTY FOLDER "Tests")
endif()

#--------------------------------------
install(TARGETS minifb EXPORT minifb)
install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/include/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING
    PATTERN "*.h"
    PATTERN "*.hpp"
)
install(FILES "${MINIFB_GENERATED_INCLUDE_DIR}/minifb_version.h" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

set(MINIFB_CMAKE_CONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/minifb")
set(MINIFB_PACKAGE_FIND_OPENGL FALSE)
set(MINIFB_PACKAGE_FIND_X11 FALSE)

if (WIN32 AND USE_OPENGL_API)
    set(MINIFB_PACKAGE_FIND_OPENGL TRUE)
elseif (UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
    if (NOT USE_WAYLAND_API)
        set(MINIFB_PACKAGE_FIND_X11 TRUE)
        if (USE_OPENGL_API)
            set(MINIFB_PACKAGE_FIND_OPENGL TRUE)
        endif()
    endif()
endif()

minifb_configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/minifb-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/minifb-config.cmake"
    "${MINIFB_CMAKE_CONFIG_INSTALL_DIR}"
)

minifb_write_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/minifb-config-version.cmake"
)

install(EXPORT minifb FILE minifb-targets.cmake DESTINATION "${MINIFB_CMAKE_CONFIG_INSTALL_DIR}" NAMESPACE minifb::)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/minifb-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/minifb-config-version.cmake"
    DESTINATION "${MINIFB_CMAKE_CONFIG_INSTALL_DIR}"
)

message(STATUS "Done " ${PROJECT_NAME})

if (MSVC AND MINIFB_BUILD_EXAMPLES)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT noise)
endif()
