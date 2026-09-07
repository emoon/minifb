if (IOS)
    message(STATUS "Interactive tests are not yet available in the iOS app build")
    return()
endif()

if (ANDROID)
    message(STATUS "Interactive tests build from tests/android with Gradle, not from here")
    return()
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

if (EMSCRIPTEN)
    foreach(test_target IN ITEMS mouse_events_test keyboard_events_test)
        target_link_options(${test_target} PRIVATE
            "-sEXPORT_NAME=${test_target}"
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
