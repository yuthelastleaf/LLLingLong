cmake_minimum_required(VERSION 3.15)
project(AudioTranscriptionDemo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC UTF-8 支持
if(MSVC)
    add_compile_options(/utf-8)
endif()

# 设置第三方库路径
set(SHERPA_ONNX_DIR "F:/ThirdLib/sherpa-onnx-v1.12.20-win-x64-shared" CACHE PATH "Sherpa-ONNX library path")
set(LLAMA_CPP_DIR "F:/ThirdLib/llama13.1" CACHE PATH "llama.cpp library path")

# 包含目录
include_directories(
    ${CMAKE_SOURCE_DIR}/thirdlib/miniaudio
    ${SHERPA_ONNX_DIR}/include
    ${LLAMA_CPP_DIR}/include
    ${LLAMA_CPP_DIR}/include/ggml/include
)

# 库目录
link_directories(
    ${SHERPA_ONNX_DIR}/lib
    ${LLAMA_CPP_DIR}/lib
)

# 添加可执行文件
add_executable(audio_transcription_demo
    src/main.cpp
)

# 链接库
target_link_libraries(audio_transcription_demo
    sherpa-onnx-c-api
    llama
    ggml
)

# Windows平台特定设置
if(WIN32)
    target_link_libraries(audio_transcription_demo
        winmm
    )
endif()

# 复制动态库到输出目录
add_custom_command(TARGET audio_transcription_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Copying DLLs..."
    
    # 复制 sherpa-onnx DLLs (从 lib 目录)
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SHERPA_ONNX_DIR}/lib/sherpa-onnx-c-api.dll"
        $<TARGET_FILE_DIR:audio_transcription_demo>
    
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SHERPA_ONNX_DIR}/lib/sherpa-onnx-cxx-api.dll"
        $<TARGET_FILE_DIR:audio_transcription_demo>
        
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SHERPA_ONNX_DIR}/lib/cargs.dll"
        $<TARGET_FILE_DIR:audio_transcription_demo>
    
    # 复制 sherpa-onnx 自带的配套 ONNX Runtime DLLs (从 bin 目录)
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SHERPA_ONNX_DIR}/bin/onnxruntime.dll"
        $<TARGET_FILE_DIR:audio_transcription_demo>
    
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SHERPA_ONNX_DIR}/bin/onnxruntime_providers_shared.dll"
        $<TARGET_FILE_DIR:audio_transcription_demo>
    
    # 复制 llama.cpp DLLs
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LLAMA_CPP_DIR}/bin/llama.dll"
        $<TARGET_FILE_DIR:audio_transcription_demo>
    
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LLAMA_CPP_DIR}/bin/ggml.dll"
        $<TARGET_FILE_DIR:audio_transcription_demo>
        
    COMMAND ${CMAKE_COMMAND} -E echo "DLLs copied successfully!"
)

# 设置输出目录
set_target_properties(audio_transcription_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
