# Adding SDL2_image Support for PNG Files

The current build uses SDL2's built-in BMP loader. To use PNG files with transparency:

## Option 1: Download SDL2_image

1. Download SDL2_image from: https://github.com/libsdl-org/SDL_image/releases
2. Extract to a folder (e.g., `F:/ThirdLib/SDL2_image-2.x.x`)
3. Update `CMakeLists.txt`:

```cmake
# Add SDL2_image path
set(SDL2_IMAGE_DIR "F:/ThirdLib/SDL2_image-2.x.x")

# Add to include directories
include_directories(
    ${SDL2_DIR}/include
    ${SDL2_DIR}/include/SDL2
    ${SDL2_IMAGE_DIR}/include
)

# Add to link directories
link_directories(
    ${SDL2_DIR}/lib/x64
    ${SDL2_IMAGE_DIR}/lib/x64
)

# Add SDL2_image to link libraries
target_link_libraries(dpet
    SDL2main
    SDL2
    SDL2_image
)

# Add DLL copy command
add_custom_command(TARGET dpet POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SDL2_IMAGE_DIR}/lib/x64/SDL2_image.dll"
        $<TARGET_FILE_DIR:dpet>
)
```

4. Update `main.cpp`:

```cpp
#include <SDL.h>
#include <SDL_image.h>

// In main(), after SDL_Init:
if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
    std::cerr << "IMG_Init failed: " << IMG_GetError() << std::endl;
    SDL_Quit();
    return 1;
}

// Replace SDL_LoadBMP with:
SDL_Surface* petSurface = IMG_Load("assets/pet.png");

// In cleanup, add:
IMG_Quit();
```

## Option 2: Use BMP with Color Key Transparency

If you want to stick with BMP, you can use color-key transparency:

1. Convert your PNG to 24-bit BMP
2. Choose a transparent color (e.g., magenta: #FF00FF)
3. In code, after loading:

```cpp
SDL_Surface* petSurface = SDL_LoadBMP("assets/pet.bmp");
SDL_SetColorKey(petSurface, SDL_TRUE, SDL_MapRGB(petSurface->format, 255, 0, 255));
```

This makes all magenta pixels transparent.

## Current Setup

Currently configured to use `assets/pet.bmp`. Place a BMP file there to test the application.
