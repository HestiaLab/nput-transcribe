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
            ${CMAKE_CURRENT_SOURCE_DIR}/src/nput
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/vendor/unity
    )
    if(NPUT_TEST_LIBS)
        target_link_libraries(${target} PRIVATE ${NPUT_TEST_LIBS})
        # 链接 nput_core 的测试与 CLI 共用 bin/，需要 OpenVINO 运行时。
        if("nput_core" IN_LIST NPUT_TEST_LIBS)
            nput_install_runtime_dlls(${target})
        endif()
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

nput_add_test(nput_asr_backend_tests
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_asr_backend.c
    LIBS nput_core
)

nput_add_test(nput_error_tests
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_error.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/nput/core/status.c
)

nput_add_test(nput_cli_args_tests
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cli_args.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/cli_args.c
)

nput_add_test(nput_cli_output_tests
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cli_output.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/cli_output.c
)

add_test(NAME nput_api_surface
    COMMAND ${CMAKE_COMMAND}
        -DNPUT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check-api-surface.cmake)
set_tests_properties(nput_api_surface PROPERTIES LABELS "policy")

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
