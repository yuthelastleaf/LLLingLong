cmake_minimum_required(VERSION 3.15)
project(DesktopPet)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SDL2 library paths
set(SDL2_DIR "F:/ThirdLib/SDL2-2.32.10")
set(SDL2_IMAGE_DIR "F:/ThirdLib/SDL2_image-2.8.8")
set(LUA_DIR "F:/ThirdLib/lua-5.4.2_Win64_dll17_lib")

# Include directories - SDL2 headers might be in include/ or include/SDL2/
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${SDL2_DIR}/include
    ${SDL2_DIR}/include/SDL2
    ${SDL2_IMAGE_DIR}/include
    ${SDL2_IMAGE_DIR}/include/SDL2
    ${LUA_DIR}/include
    ${LUA_DIR}  # For sol.hpp
)

# Link directories
link_directories(
    ${SDL2_DIR}/lib/x64
    ${SDL2_IMAGE_DIR}/lib/x64
    ${LUA_DIR}
)

# Source files
set(SOURCES
    src/main.cpp
    src/pet_api.cpp
    src/lua_bindings.cpp
    src/lua_thread.cpp
    src/chat_bubble.cpp
)

# Add executable
add_executable(dpet ${SOURCES})

# Link SDL2 libraries
target_link_libraries(dpet
    SDL2main
    SDL2
    SDL2_image
    lua54
)

# Copy SDL2 DLLs to output directory after build
add_custom_command(TARGET dpet POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SDL2_DIR}/lib/x64/SDL2.dll"
        $<TARGET_FILE_DIR:dpet>
    COMMENT "Copying SDL2.dll to output directory"
)

add_custom_command(TARGET dpet POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SDL2_IMAGE_DIR}/lib/x64/SDL2_image.dll"
        $<TARGET_FILE_DIR:dpet>
    COMMENT "Copying SDL2_image.dll to output directory"
)

add_custom_command(TARGET dpet POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LUA_DIR}/lua54.dll"
        $<TARGET_FILE_DIR:dpet>
    COMMENT "Copying lua54.dll to output directory"
)

# Copy assets directory to output directory
add_custom_command(TARGET dpet POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:dpet>/assets"
    COMMENT "Copying assets folder to output directory"
)

# Copy scripts directory to output directory
add_custom_command(TARGET dpet POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/scripts"
        "$<TARGET_FILE_DIR:dpet>/scripts"
    COMMENT "Copying scripts folder to output directory"
)

# Set subsystem to WINDOWS to avoid console window (optional)
# Temporarily disabled to see debug output
# if(WIN32)
#     set_target_properties(dpet PROPERTIES
#         WIN32_EXECUTABLE TRUE
#     )
# endif()
