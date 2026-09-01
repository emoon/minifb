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

set_property(TARGET mouse_events_test PROPERTY FOLDER "Tests")

if (EMSCRIPTEN)
    target_link_options(mouse_events_test PRIVATE
        "-sEXPORT_NAME=mouse_events_test"
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

    set(html_src "${CMAKE_CURRENT_SOURCE_DIR}/web/mouse_events_test.html")
    set(html_dst "${CMAKE_CURRENT_BINARY_DIR}/mouse_events_test.html")
    add_custom_command(
        OUTPUT  "${html_dst}"
        COMMAND ${CMAKE_COMMAND} -E copy "${html_src}" "${html_dst}"
        DEPENDS "${html_src}"
        COMMENT "Copying mouse_events_test.html"
        VERBATIM
    )
    add_custom_target(mouse_events_test_html DEPENDS "${html_dst}")
    add_dependencies(mouse_events_test mouse_events_test_html)
endif()
