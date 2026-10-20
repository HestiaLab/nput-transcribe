# 公共/私有 API 边界检查。
#
# - NPUT_API 只能出现在安装的公共头文件（include/nput）中。
# - NPUT_PRIVATE_API 只能出现在 src/ 中。
# - 公共头文件不得包含内部头文件。

if(NOT NPUT_SOURCE_DIR)
    message(FATAL_ERROR "NPUT_SOURCE_DIR is required")
endif()

file(GLOB_RECURSE _private_files
    "${NPUT_SOURCE_DIR}/src/*.c"
    "${NPUT_SOURCE_DIR}/src/*.h")
file(GLOB_RECURSE _test_files
    "${NPUT_SOURCE_DIR}/tests/*.c"
    "${NPUT_SOURCE_DIR}/tests/*.h")
file(GLOB_RECURSE _public_headers "${NPUT_SOURCE_DIR}/include/nput/*.h")

foreach(_file IN LISTS _private_files _test_files)
    file(READ "${_file}" _content)

    string(REGEX MATCH "NPUT_API" _match "${_content}")
    if(_match)
        message(FATAL_ERROR "NPUT_API must only appear in include/nput: ${_file}")
    endif()

    string(REGEX MATCH
        "#include[ \t]+\"nput/(backend|decode|tokenizer_backend|error|private_export|trigram|internal)\\.h\""
        _match "${_content}")
    if(_match)
        message(FATAL_ERROR "private header referenced through the public nput/ prefix: ${_file}")
    endif()

    string(REGEX MATCH "#include[ \t]+\"nput/(asr|audio|core|platform|text)/" _match "${_content}")
    if(_match)
        message(FATAL_ERROR "module-private header referenced through nput/: ${_file}")
    endif()
endforeach()

foreach(_file IN LISTS _public_headers)
    file(READ "${_file}" _content)

    string(REGEX MATCH "NPUT_PRIVATE_API" _match "${_content}")
    if(_match)
        message(FATAL_ERROR "NPUT_PRIVATE_API must not appear in public headers: ${_file}")
    endif()

    string(REGEX MATCH
        "#include[ \t]+\"nput/(backend|decode|tokenizer_backend|error|private_export|trigram|internal)\\.h\""
        _match "${_content}")
    if(_match)
        message(FATAL_ERROR "public header includes a private header: ${_file}")
    endif()

    string(REGEX MATCH "#include[ \t]+\"nput/(asr|audio|core|platform|text)/" _match "${_content}")
    if(_match)
        message(FATAL_ERROR "public header includes a module-private header: ${_file}")
    endif()
endforeach()
