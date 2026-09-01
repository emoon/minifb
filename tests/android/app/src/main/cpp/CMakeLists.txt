cmake_minimum_required(VERSION 3.18.1)
project(mouse_events_test)

# tests/android/app/src/main/cpp -> repository root
set(MINIFB_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../../../../..)

# build native_app_glue as a static lib
add_library(native_app_glue STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)

add_library(minifb STATIC
    ${MINIFB_ROOT}/include/MiniFB.h
    ${MINIFB_ROOT}/include/MiniFB_cpp.h
    ${MINIFB_ROOT}/include/MiniFB_enums.h

    ${MINIFB_ROOT}/src/MiniFB_common.c
    ${MINIFB_ROOT}/src/MiniFB_cpp.cpp
    ${MINIFB_ROOT}/src/MiniFB_internal.c
    ${MINIFB_ROOT}/src/MiniFB_internal.h
    ${MINIFB_ROOT}/src/MiniFB_timer.c
    ${MINIFB_ROOT}/src/MiniFB_linux.c
    ${MINIFB_ROOT}/src/WindowData.h

    ${MINIFB_ROOT}/src/android/AndroidMiniFB.c
    ${MINIFB_ROOT}/src/android/WindowData_Android.h
)

target_include_directories(minifb PRIVATE
    ${ANDROID_NDK}/sources/android/native_app_glue
    ${MINIFB_ROOT}/include
    ${MINIFB_ROOT}/src
)

# The test source is shared with the desktop builds; only the packaging differs.
add_library(mouse_events_test SHARED
    ${MINIFB_ROOT}/tests/mouse_events.c
)

# Required for Android 15+ 16 KB page-size compatibility
target_link_options(mouse_events_test PRIVATE
    "-Wl,-z,max-page-size=16384"
    "-Wl,-z,common-page-size=16384"
)

target_include_directories(mouse_events_test PRIVATE
    ${ANDROID_NDK}/sources/android/native_app_glue
    ${MINIFB_ROOT}/include
)

# Export ANativeActivity_onCreate(),
# Refer to: https://github.com/android-ndk/ndk/issues/381.
set(CMAKE_SHARED_LINKER_FLAGS
    "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate"
)

target_link_libraries(mouse_events_test
    android
    minifb
    native_app_glue
    log
    m
)
