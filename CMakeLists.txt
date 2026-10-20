cmake_minimum_required(VERSION 3.21...4.3)

project(NpuTranscribe
    VERSION 0.3.0
    DESCRIPTION "Whisper speech transcription CLI that runs the encoder on the Intel NPU"
    LANGUAGES C
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(NPUT_IS_TOP_LEVEL ON)
else()
    set(NPUT_IS_TOP_LEVEL OFF)
endif()

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(NPUT_BUILD_CLI "Build the nput-transcribe executable" ${NPUT_IS_TOP_LEVEL})
option(NPUT_BUILD_TESTING "Build CTest tests" ${NPUT_IS_TOP_LEVEL})
option(NPUT_ENABLE_ASAN "Enable AddressSanitizer where supported" OFF)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompilerOptions.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/OpenVINO.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Sources.cmake")

add_library(nput_core ${NPUT_CORE_SOURCES})
add_library(NpuTranscribe::nput_core ALIAS nput_core)

target_compile_features(nput_core PUBLIC c_std_11)
target_compile_definitions(nput_core PUBLIC NPUT_VERSION="${PROJECT_VERSION}")
target_include_directories(nput_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/nput
)
if(BUILD_SHARED_LIBS)
    target_compile_definitions(nput_core
        PUBLIC NPUT_SHARED
        PRIVATE NPUT_BUILDING_LIBRARY)
    set_target_properties(nput_core PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        C_VISIBILITY_PRESET hidden)
endif()
target_link_libraries(nput_core PRIVATE nput_openvino)
if(nput_openvino_dlls)
    target_link_libraries(nput_core PRIVATE nput_openvino_dlls)
endif()
if(WIN32)
    # Media Foundation：为非 WAV 输入提供 MP3/AAC 解码。
    target_link_libraries(nput_core PRIVATE mfplat mfreadwrite mfuuid ole32)
endif()
if(UNIX)
    target_link_libraries(nput_core PRIVATE m)
endif()
nput_target_defaults(nput_core)
set_target_properties(nput_core PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(NPUT_BUILD_CLI)
    add_executable(nput-transcribe ${NPUT_CLI_SOURCES})
    target_link_libraries(nput-transcribe PRIVATE nput_core)
    if(WIN32)
        # CommandLineToArgvW：从宽字符命令行重建 UTF-8 argv。
        target_link_libraries(nput-transcribe PRIVATE shell32)
    endif()
    nput_target_defaults(nput-transcribe)
    set_target_properties(nput-transcribe PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    nput_install_runtime_dlls(nput-transcribe)
endif()

if(NPUT_BUILD_TESTING)
    enable_testing()
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Tests.cmake")
endif()

if(NPUT_IS_TOP_LEVEL)
    install(TARGETS nput_core
        EXPORT NpuTranscribeTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES LICENSE
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/licenses/NpuTranscribe)
    install(DIRECTORY include/nput
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h")
    install(FILES cmake/OpenVINO.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NpuTranscribe)
    install(EXPORT NpuTranscribeTargets
        FILE NpuTranscribeTargets.cmake
        NAMESPACE NpuTranscribe::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NpuTranscribe)

    configure_package_config_file(
        cmake/NpuTranscribeConfig.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/NpuTranscribeConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NpuTranscribe)
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/NpuTranscribeConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMinorVersion)
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/NpuTranscribeConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/NpuTranscribeConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NpuTranscribe)
endif()
