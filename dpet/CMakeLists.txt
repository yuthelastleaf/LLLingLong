cmake_minimum_required(VERSION 3.15)
project(DesktopPet_TriCore)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SDL2 library paths
set(SDL2_DIR "F:/ThirdLib/SDL2-2.32.10")
set(SDL2_IMAGE_DIR "F:/ThirdLib/SDL2_image-2.8.8")
set(LUA_DIR "F:/ThirdLib/lua-5.4.2_Win64_dll17_lib")

# ASR and LLM paths
set(SHERPA_ONNX_DIR "F:/ThirdLib/sherpa-onnx-v1.12.20-win-x64-shared")
set(LLAMA_CPP_DIR "F:/ThirdLib/llama12.4")

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${SDL2_DIR}/include
    ${SDL2_DIR}/include/SDL2
    ${SDL2_IMAGE_DIR}/include
    ${SDL2_IMAGE_DIR}/include/SDL2
    ${LUA_DIR}/include
    ${LUA_DIR}  # For sol.hpp
    ${SHERPA_ONNX_DIR}/include
    ${LLAMA_CPP_DIR}/include
    ${LLAMA_CPP_DIR}/include/ggml/include
    F:/GitProject/LLLingLong/thirdlib  # For miniaudio.h
)

# Link directories
link_directories(
    ${SDL2_DIR}/lib/x64
    ${SHERPA_ONNX_DIR}/lib
    ${LLAMA_CPP_DIR}
    ${SDL2_IMAGE_DIR}/lib/x64
    ${LUA_DIR}
)

# Source files for Tri-Core architecture
set(SOURCES_TRICORE
    ../src/main_tricore.cpp
    ../src/App.cpp
    ../src/Managers.cpp
    ../src/ContextManager.cpp
    ../src/chat_bubble.cpp
)

# Add executable for Tri-Core
add_executable(dpet_tricore ${SOURCES_TRICORE})

# Link libraries
target_link_libraries(dpet_tricore
    SDL2main
    SDL2
    SDL2_image
    lua54
    sherpa-onnx-c-api
    ${LLAMA_CPP_DIR}/lib/llama.lib
    ${LLAMA_CPP_DIR}/lib/ggml.lib
)

# Copy DLLs to output directory
add_custom_command(TARGET dpet_tricore POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Copying SDL2.dll to output directory"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SDL2_DIR}/lib/x64/SDL2.dll"
        $<TARGET_FILE_DIR:dpet_tricore>
        
    COMMAND ${CMAKE_COMMAND} -E echo "Copying SDL2_image.dll to output directory"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SDL2_IMAGE_DIR}/lib/x64/SDL2_image.dll"
        $<TARGET_FILE_DIR:dpet_tricore>
        
    COMMAND ${CMAKE_COMMAND} -E echo "Copying sherpa-onnx DLLs to output directory"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SHERPA_ONNX_DIR}/lib/sherpa-onnx-c-api.dll"
        $<TARGET_FILE_DIR:dpet_tricore>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SHERPA_ONNX_DIR}/lib/onnxruntime.dll"
        $<TARGET_FILE_DIR:dpet_tricore>
        
    COMMAND ${CMAKE_COMMAND} -E echo "Copying llama.cpp DLLs to output directory"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LLAMA_CPP_DIR}/bin/llama.dll"
        $<TARGET_FILE_DIR:dpet_tricore>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LLAMA_CPP_DIR}/bin/ggml.dll"
        $<TARGET_FILE_DIR:dpet_tricore>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LLAMA_CPP_DIR}/bin/ggml-base.dll"
        $<TARGET_FILE_DIR:dpet_tricore>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LLAMA_CPP_DIR}/bin/ggml-cuda.dll"
        $<TARGET_FILE_DIR:dpet_tricore>
        
    COMMAND ${CMAKE_COMMAND} -E echo "Copying CUDA 12.4 DLLs for GPU support"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LLAMA_CPP_DIR}/bin/cudart64_12.dll"
        $<TARGET_FILE_DIR:dpet_tricore>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LLAMA_CPP_DIR}/bin/cublas64_12.dll"
        $<TARGET_FILE_DIR:dpet_tricore>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LLAMA_CPP_DIR}/bin/cublasLt64_12.dll"
        $<TARGET_FILE_DIR:dpet_tricore>
        
    COMMAND ${CMAKE_COMMAND} -E echo "Copying lua54.dll to output directory"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LUA_DIR}/lua54.dll"
        $<TARGET_FILE_DIR:dpet_tricore>
        
    COMMAND ${CMAKE_COMMAND} -E echo "Copying assets folder to output directory"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        $<TARGET_FILE_DIR:dpet_tricore>/assets
        
    COMMAND ${CMAKE_COMMAND} -E echo "Copying scripts folder to output directory"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/scripts"
        $<TARGET_FILE_DIR:dpet_tricore>/scripts
)
