# nput_core 库与 CLI 可执行文件的源文件列表。

set(NPUT_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nput/core/status.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nput/platform/platform.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nput/platform/clock.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nput/audio/wav.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nput/platform/media_foundation.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nput/audio/audio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nput/audio/resample.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nput/audio/mel.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nput/text/tokenizer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nput/audio/segment.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nput/asr/ov_runtime.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nput/asr/asr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nput/asr/backend_whisper.c
)

set(NPUT_CLI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cli_args.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cli_output.c
)

# 频谱计算是密集的 DFT 循环，Debug 之外的配置保持优化。
set(NPUT_MEL_OPT_FLAG "$<$<NOT:$<CONFIG:Debug>>:-O2>")
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/src/nput/audio/mel.c
    PROPERTIES COMPILE_OPTIONS "${NPUT_MEL_OPT_FLAG}")
