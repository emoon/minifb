cmake_minimum_required(VERSION 3.18.1)
project(noise)

# Set our flags
#--------------------------------------
#add_compile_options("$<$<CONFIG:Debug>:-g>")
#add_compile_options("$<IF:$<CONFIG:Debug>,-O0,-O2>")
#if(APPLE)
#    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic -Wno-switch -Wno-unused-function -Wno-implicit-fallthrough")
#else()
#    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic -Wno-switch -Wno-unused-function -Wno-implicit-fallthrough -Wno-cast-function-type")
#endif()
#set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}")
#set(CMAKE_OBJC_FLAGS "${CMAKE_C_FLAGS}")
#set(CMAKE_OBJCXX_FLAGS "${CMAKE_CXX_FLAGS}")

# build native_app_glue as a static lib
add_library(native_app_glue STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)

add_library(minifb STATIC
    ${CMAKE_CURRENT_LIST_DIR}/../../../../../../include/MiniFB.h
    ${CMAKE_CURRENT_LIST_DIR}/../../../../../../include/MiniFB_cpp.h
    ${CMAKE_CURRENT_LIST_DIR}/../../../../../../include/MiniFB_enums.h

    ${CMAKE_CURRENT_LIST_DIR}/../../../../../../src/MiniFB_common.c
    ${CMAKE_CURRENT_LIST_DIR}/../../../../../../src/MiniFB_cpp.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../../../../../src/MiniFB_internal.c
    ${CMAKE_CURRENT_LIST_DIR}/../../../../../../src/MiniFB_internal.h
    ${CMAKE_CURRENT_LIST_DIR}/../../../../../../src/MiniFB_timer.c
    ${CMAKE_CURRENT_LIST_DIR}/../../../../../../src/MiniFB_linux.c
    ${CMAKE_CURRENT_LIST_DIR}/../../../../../../src/WindowData.h

    ${CMAKE_CURRENT_LIST_DIR}/../../../../../../src/android/AndroidMiniFB.c
    ${CMAKE_CURRENT_LIST_DIR}/../../../../../../src/android/WindowData_Android.h
)

target_include_directories(minifb PRIVATE
    ${ANDROID_NDK}/sources/android/native_app_glue
    ${CMAKE_CURRENT_LIST_DIR}/../../../../../../include
    ${CMAKE_CURRENT_LIST_DIR}/../../../../../../src
)

# now build app's shared lib
add_library(noise SHARED
    noise.c
)

target_include_directories(noise PRIVATE
    ${ANDROID_NDK}/sources/android/native_app_glue
    ${CMAKE_CURRENT_LIST_DIR}/../../../../../../include
)

# Export ANativeActivity_onCreate(),
# Refer to: https://github.com/android-ndk/ndk/issues/381.
set(CMAKE_SHARED_LINKER_FLAGS
    "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate"
)

# add lib dependencies
target_link_libraries(noise
    android
    minifb
    native_app_glue
    log
    m
)
