# Merge static archives into LOCHFOLK_OUTPUT.
#
# Invoked from a POST_BUILD step on the lochfolk static library:
#   LOCHFOLK_OUTPUT   - archive to modify (the lochfolk static library)
#   LOCHFOLK_MINIZIP  - minizip-ng archive to embed
#   LOCHFOLK_ZLIB     - optional static zlib archive to embed (Windows)
#   LOCHFOLK_WORK_DIR - scratch directory used by the portable ar path
#   LOCHFOLK_AR       - archive tool (CMAKE_AR)
#   LOCHFOLK_RANLIB   - ranlib tool (CMAKE_RANLIB)
#   LOCHFOLK_MSVC     - set when the MSVC toolchain is used

if(NOT LOCHFOLK_OUTPUT OR NOT EXISTS "${LOCHFOLK_OUTPUT}")
    message(FATAL_ERROR "bundle_static_archive: LOCHFOLK_OUTPUT is not set or does not exist")
endif()

set(_archives "${LOCHFOLK_OUTPUT}" "${LOCHFOLK_MINIZIP}")
if(LOCHFOLK_ZLIB)
    list(APPEND _archives "${LOCHFOLK_ZLIB}")
endif()

if(LOCHFOLK_MSVC)
    # lib.exe can merge archives directly. Write to a temporary file first
    # because the output must not be one of the inputs.
    get_filename_component(_output_dir "${LOCHFOLK_OUTPUT}" DIRECTORY)
    get_filename_component(_output_name "${LOCHFOLK_OUTPUT}" NAME)
    set(_merged "${_output_dir}/.${_output_name}.bundled")
    file(REMOVE "${_merged}")
    execute_process(
        COMMAND "${LOCHFOLK_AR}" "/OUT:${_merged}" ${_archives}
        RESULT_VARIABLE _result
        OUTPUT_VARIABLE _stdout
        ERROR_VARIABLE _stderr
    )
    if(NOT _result EQUAL 0)
        message(FATAL_ERROR
            "bundle_static_archive: merging archives failed (${_result})\n"
            "${_stdout}\n${_stderr}"
        )
    endif()
    file(REMOVE "${LOCHFOLK_OUTPUT}")
    file(RENAME "${_merged}" "${LOCHFOLK_OUTPUT}")
else()
    # Portable ar path: extract every archive into a scratch directory and
    # recreate the combined archive, keeping the dependency objects after the
    # lochfolk objects.
    file(REMOVE_RECURSE "${LOCHFOLK_WORK_DIR}")
    file(MAKE_DIRECTORY "${LOCHFOLK_WORK_DIR}")
    set(_objects)
    set(_index 0)
    foreach(_archive IN LISTS _archives)
        set(_dir "${LOCHFOLK_WORK_DIR}/${_index}")
        file(MAKE_DIRECTORY "${_dir}")
        execute_process(
            COMMAND "${LOCHFOLK_AR}" x "${_archive}"
            WORKING_DIRECTORY "${_dir}"
            RESULT_VARIABLE _result
            OUTPUT_VARIABLE _stdout
            ERROR_VARIABLE _stderr
        )
        if(NOT _result EQUAL 0)
            message(FATAL_ERROR
                "bundle_static_archive: extracting ${_archive} failed (${_result})\n"
                "${_stdout}\n${_stderr}"
            )
        endif()
        file(GLOB _extracted LIST_DIRECTORIES false "${_dir}/*")
        # BSD/macOS ar extracts the archive's symbol-table members
        # (__.SYMDEF, __.SYMDEF SORTED) as ordinary files. Drop them; ranlib
        # recreates a single index for the merged archive.
        list(FILTER _extracted EXCLUDE REGEX "/(__\\.SYMDEF|SYMDEF)")
        list(APPEND _objects ${_extracted})
        math(EXPR _index "${_index} + 1")
    endforeach()

    file(REMOVE "${LOCHFOLK_OUTPUT}")
    execute_process(
        COMMAND "${LOCHFOLK_AR}" qc "${LOCHFOLK_OUTPUT}" ${_objects}
        RESULT_VARIABLE _result
        OUTPUT_VARIABLE _stdout
        ERROR_VARIABLE _stderr
    )
    if(NOT _result EQUAL 0)
        message(FATAL_ERROR
            "bundle_static_archive: recreating ${LOCHFOLK_OUTPUT} failed (${_result})\n"
            "${_stdout}\n${_stderr}"
        )
    endif()

    if(LOCHFOLK_RANLIB)
        execute_process(
            COMMAND "${LOCHFOLK_RANLIB}" "${LOCHFOLK_OUTPUT}"
            RESULT_VARIABLE _result
            OUTPUT_VARIABLE _stdout
            ERROR_VARIABLE _stderr
        )
        if(NOT _result EQUAL 0)
            message(FATAL_ERROR
                "bundle_static_archive: ranlib failed (${_result})\n"
                "${_stdout}\n${_stderr}"
            )
        endif()
    endif()
endif()
