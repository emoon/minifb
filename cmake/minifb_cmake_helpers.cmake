function(minifb_apply_deprecated_option legacy_var new_var new_help)
    # Keep legacy -D flags working while steering users to MINIFB_* names.
    get_property(_legacy_is_set CACHE ${legacy_var} PROPERTY TYPE SET)
    if (_legacy_is_set)
        get_property(_legacy_value CACHE ${legacy_var} PROPERTY VALUE)
        message(DEPRECATION "${legacy_var} is deprecated; use ${new_var} instead.")
        set(${new_var} "${_legacy_value}" CACHE BOOL "${new_help}" FORCE)
    endif()
    set(${legacy_var} "${${new_var}}" PARENT_SCOPE)
	set(MINIFB_HAVE_USED_LEGACY_OPTION TRUE PARENT_SCOPE)
endfunction()

function(minifb_compute_git_metadata out_commits_since_tag out_commit_count out_git_sha out_git_dirty)
    set(_commits_since_tag 0)
    set(_commit_count 0)
    set(_git_sha "unknown")
    set(_git_dirty 0)

    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
        find_package(Git QUIET)
        if (GIT_FOUND)
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" rev-list --count HEAD
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                RESULT_VARIABLE _git_count_result
                OUTPUT_VARIABLE _git_count_output
                ERROR_QUIET
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            if (_git_count_result EQUAL 0 AND _git_count_output MATCHES "^[0-9]+$")
                set(_commit_count "${_git_count_output}")
            endif()

            execute_process(
                COMMAND "${GIT_EXECUTABLE}" describe --tags --long --dirty
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                RESULT_VARIABLE _git_describe_result
                OUTPUT_VARIABLE _git_describe_output
                ERROR_QUIET
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            if (_git_describe_result EQUAL 0)
                if (_git_describe_output MATCHES ".*-([0-9]+)-g([0-9a-fA-F]+)(-dirty)?$")
                    set(_commits_since_tag "${CMAKE_MATCH_1}")
                    set(_git_sha "${CMAKE_MATCH_2}")
                    if (CMAKE_MATCH_3 STREQUAL "-dirty")
                        set(_git_dirty 1)
                    endif()
                endif()
            endif()
        endif()
    endif()

    set(${out_commits_since_tag} "${_commits_since_tag}" PARENT_SCOPE)
    set(${out_commit_count} "${_commit_count}" PARENT_SCOPE)
    set(${out_git_sha} "${_git_sha}" PARENT_SCOPE)
    set(${out_git_dirty} "${_git_dirty}" PARENT_SCOPE)
endfunction()
