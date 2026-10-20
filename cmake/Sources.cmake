# nput_core 库与 CLI 可执行文件的源文件列表。

set(NPUT_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nput/status.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nput/platform.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nput/wav.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nput/media_foundation.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nput/audio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nput/resample.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nput/mel.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nput/tokenizer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nput/segment.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nput/ov_runtime.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nput/asr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nput/backend_whisper.c
)

set(NPUT_CLI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
)

# 频谱计算是密集的 DFT 循环，Debug 之外的配置保持优化。
set(NPUT_MEL_OPT_FLAG "$<$<NOT:$<CONFIG:Debug>>:-O2>")
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/src/nput/mel.c
    PROPERTIES COMPILE_OPTIONS "${NPUT_MEL_OPT_FLAG}")
