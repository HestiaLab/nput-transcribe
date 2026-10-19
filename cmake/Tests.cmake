# 测试注册：每个测试文件一个可执行文件，由 Unity 驱动。

set(NPUT_UNITY_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/vendor/unity/unity.c)

function(nput_add_test target)
    cmake_parse_arguments(NPUT_TEST "" "" "SOURCES;LIBS" ${ARGN})
    if(NOT NPUT_TEST_SOURCES)
        message(FATAL_ERROR "nput_add_test(${target}) requires SOURCES")
    endif()

    add_executable(${target} ${NPUT_TEST_SOURCES} ${NPUT_UNITY_SOURCES})
    target_include_directories(${target}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/vendor/unity
    )
    if(NPUT_TEST_LIBS)
        target_link_libraries(${target} PRIVATE ${NPUT_TEST_LIBS})
    endif()
    nput_target_defaults(${target})
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    add_test(NAME ${target} COMMAND $<TARGET_FILE:${target}>)
    set_tests_properties(${target} PROPERTIES LABELS "unit")
endfunction()

nput_add_test(nput_wav_tests
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_wav.c
    LIBS nput_core
)

nput_add_test(nput_resample_tests
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_resample.c
    LIBS nput_core
)

nput_add_test(nput_mel_tests
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_mel.c
    LIBS nput_core
)

nput_add_test(nput_tokenizer_tests
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_tokenizer.c
    LIBS nput_core
)

nput_add_test(nput_segment_tests
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_segment.c
    LIBS nput_core
)

nput_add_test(nput_decode_guard_tests
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_decode_guard.c
)

if(TARGET nput-transcribe)
    add_test(NAME nput_cli_help COMMAND $<TARGET_FILE:nput-transcribe> --help)
    set_tests_properties(nput_cli_help PROPERTIES
        LABELS "smoke"
        PASS_REGULAR_EXPRESSION "用法："
    )
    add_test(NAME nput_cli_version COMMAND $<TARGET_FILE:nput-transcribe> --version)
    set_tests_properties(nput_cli_version PROPERTIES
        LABELS "smoke"
        PASS_REGULAR_EXPRESSION "[0-9]+\\.[0-9]+\\.[0-9]+"
    )
endif()
