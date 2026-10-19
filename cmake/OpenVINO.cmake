# 定位 OpenVINO C 运行时（随 `openvino` Python wheel 或独立安装提供），
# 并以导入目标的形式暴露。
#
# 缓存变量 / 环境变量：
#   OPENVINO_ROOT  - OpenVINO 运行时根目录，包含
#                    include/openvino/c/openvino.h 与 libs/openvino_c.lib。

set(OPENVINO_ROOT "" CACHE PATH "OpenVINO runtime root (contains include/ and libs/)")
if(NOT OPENVINO_ROOT AND DEFINED ENV{OPENVINO_ROOT})
    set(OPENVINO_ROOT "$ENV{OPENVINO_ROOT}")
endif()

if(NOT OPENVINO_ROOT)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        execute_process(
            COMMAND "${Python3_EXECUTABLE}" -c
                    "import openvino, os; print(os.path.dirname(openvino.__file__))"
            OUTPUT_VARIABLE _nput_ov_python_dir
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE _nput_ov_python_result
        )
        if(_nput_ov_python_result EQUAL 0 AND EXISTS "${_nput_ov_python_dir}/include/openvino/c/openvino.h")
            set(OPENVINO_ROOT "${_nput_ov_python_dir}")
        endif()
    endif()
endif()

if(NOT OPENVINO_ROOT)
    message(FATAL_ERROR
        "OpenVINO runtime not found. Install it with `pip install openvino` or set "
        "OPENVINO_ROOT to a directory containing include/openvino/c/openvino.h.")
endif()

set(OPENVINO_INCLUDE_DIR "${OPENVINO_ROOT}/include")
set(OPENVINO_LIBS_DIR "${OPENVINO_ROOT}/libs")
if(NOT EXISTS "${OPENVINO_LIBS_DIR}")
    set(OPENVINO_LIBS_DIR "${OPENVINO_ROOT}/lib")
endif()

if(WIN32)
    # wheel 提供的是 MSVC 导入库；MinGW 的 ld 也能直接使用，因此两种工具链
    # 都通过 IMPORTED_IMPLIB 链接到 DLL。
    set(_nput_ov_implib "${OPENVINO_LIBS_DIR}/openvino_c.lib")
    if(NOT EXISTS "${_nput_ov_implib}")
        message(FATAL_ERROR "openvino_c.lib not found in ${OPENVINO_LIBS_DIR}")
    endif()
    add_library(nput_openvino SHARED IMPORTED)
    set_target_properties(nput_openvino PROPERTIES
        IMPORTED_IMPLIB "${_nput_ov_implib}"
        IMPORTED_LOCATION "${OPENVINO_LIBS_DIR}/openvino_c.dll"
    )
else()
    find_library(NPUT_OPENVINO_C_LIBRARY NAMES openvino_c PATHS "${OPENVINO_LIBS_DIR}" NO_DEFAULT_PATH)
    if(NOT NPUT_OPENVINO_C_LIBRARY)
        message(FATAL_ERROR "openvino_c library not found in ${OPENVINO_LIBS_DIR}")
    endif()
    add_library(nput_openvino SHARED IMPORTED)
    set_target_properties(nput_openvino PROPERTIES
        IMPORTED_LOCATION "${NPUT_OPENVINO_C_LIBRARY}"
    )
endif()

set_target_properties(nput_openvino PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${OPENVINO_INCLUDE_DIR}"
)

message(STATUS "OpenVINO runtime: ${OPENVINO_ROOT}")

# 把 OpenVINO 运行时复制到可执行文件旁边，运行时无需配置 PATH。
function(nput_install_runtime_dlls target)
    if(NOT WIN32)
        return()
    endif()
    file(GLOB _nput_ov_dlls "${OPENVINO_LIBS_DIR}/*.dll")
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_nput_ov_dlls}
                "$<TARGET_FILE_DIR:${target}>"
        COMMENT "Copying OpenVINO runtime DLLs next to ${target}"
        VERBATIM
    )
endfunction()
