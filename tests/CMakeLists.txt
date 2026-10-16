if (IOS)
    message(STATUS "Interactive tests are not yet available in the iOS app build")
    return()
endif()

if (ANDROID)
    message(STATUS "Interactive tests build from tests/android with Gradle, not from here")
    return()
endif()

if (NOT EMSCRIPTEN)
    add_executable(scaling_test scaling.c)
    target_link_libraries(scaling_test PRIVATE minifb)
    target_include_directories(scaling_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
    minifb_configure_target(scaling_test)
    minifb_set_dos_name(scaling_test scaling)
    set_property(TARGET scaling_test PROPERTY FOLDER "Tests")
    if (NOT CMAKE_CROSSCOMPILING OR CMAKE_CROSSCOMPILING_EMULATOR)
        add_test(NAME scaling COMMAND scaling_test)
    endif()
endif()

# This test needs two windows, which DOS does not support
if (NOT DJGPP)
    add_executable(stretch_mode_test stretch_mode.c)
    target_link_libraries(stretch_mode_test PRIVATE minifb)
    target_include_directories(stretch_mode_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
    minifb_configure_target(stretch_mode_test)
    set_property(TARGET stretch_mode_test PROPERTY FOLDER "Tests")
endif()

add_executable(mouse_events_test
    mouse_events.c
)
target_link_libraries(mouse_events_test
PRIVATE
    minifb
)
minifb_configure_target(mouse_events_test)
minifb_set_dos_name(mouse_events_test mouseevt)

set_property(TARGET mouse_events_test PROPERTY FOLDER "Tests")

add_executable(keyboard_events_test
    keyboard_events.c
)
target_link_libraries(keyboard_events_test
PRIVATE
    minifb
)
minifb_configure_target(keyboard_events_test)
minifb_set_dos_name(keyboard_events_test keyevent)

set_property(TARGET keyboard_events_test PROPERTY FOLDER "Tests")

set(minifb_interactive_tests mouse_events_test keyboard_events_test)

# DOS has no window to focus, resize or close.
if (NOT DJGPP)
    add_executable(window_events_test
        window_events.c
    )
    target_link_libraries(window_events_test
    PRIVATE
        minifb
    )
    minifb_configure_target(window_events_test)

    set_property(TARGET window_events_test PROPERTY FOLDER "Tests")

    list(APPEND minifb_interactive_tests window_events_test)
endif()

# Runs on its own and needs no page to host it, so it stays out of the web list below.
if (NOT EMSCRIPTEN)
    add_executable(window_title_test
        window_title.c
    )
    target_link_libraries(window_title_test
    PRIVATE
        minifb
    )
    minifb_configure_target(window_title_test)
    minifb_set_dos_name(window_title_test wintitle)

    set_property(TARGET window_title_test PROPERTY FOLDER "Tests")
endif()

if (EMSCRIPTEN)
    target_link_options(stretch_mode_test PRIVATE "-sINCOMING_MODULE_JS_API=print,printErr,noInitialRun")
    set(minifb_web_tests ${minifb_interactive_tests} stretch_mode_test)
    foreach(test_target IN LISTS minifb_web_tests)
        target_link_options(${test_target} PRIVATE
            "-sEXPORT_NAME=${test_target}"
            "-sSTRICT=1"
            "-sENVIRONMENT=web"
            "-sMODULARIZE=1"
            "-sALLOW_MEMORY_GROWTH=1"
            "-sALLOW_TABLE_GROWTH"
            "-sMALLOC=emmalloc"
            "-sEXPORTED_FUNCTIONS=[\"_malloc\",\"_free\",\"_main\"]"
            "-sEXPORTED_RUNTIME_METHODS=ccall,cwrap"
            "-sASYNCIFY"
            "--no-entry"
            "-sSINGLE_FILE"
        )

        set(html_src "${CMAKE_CURRENT_SOURCE_DIR}/web/${test_target}.html")
        set(html_dst "${CMAKE_CURRENT_BINARY_DIR}/${test_target}.html")
        add_custom_command(
            OUTPUT  "${html_dst}"
            COMMAND ${CMAKE_COMMAND} -E copy "${html_src}" "${html_dst}"
            DEPENDS "${html_src}"
            COMMENT "Copying ${test_target}.html"
            VERBATIM
        )
        add_custom_target(${test_target}_html DEPENDS "${html_dst}")
        add_dependencies(${test_target} ${test_target}_html)
    endforeach()
endif()
