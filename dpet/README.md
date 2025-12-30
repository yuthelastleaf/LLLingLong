# Desktop Pet (dpet)

A transparent, draggable desktop pet window using SDL2.

## Features

- **Transparent Background**: Window background is fully transparent, allowing the desktop to show through
- **Borderless Window**: No title bar or borders
- **Always On Top**: Stays above other windows
- **Draggable**: Click and drag anywhere on the window to move it
- **Image Support**: Currently uses BMP format (see SDL2_IMAGE_GUIDE.md for PNG support)

## Requirements

- SDL2 library (located at `F:\ThirdLib\SDL2-2.32.10`)
- SDL2_image library (included with SDL2)
- CMake 3.15+
- Visual Studio 2022 or compatible C++ compiler

## Build Instructions

### 1. Configure with CMake

```powershell
cd dpet
cmake -B build -G "Visual Studio 17 2022" -A x64
```

### 2. Build the project

```powershell
cmake --build build --config Release
```

### 3. Run

The executable and required DLLs will be in `build/Release/`:

```powershell
.\build\Release\dpet.exe
```

**Note**: Make sure you have an image at `assets/pet.bmp`. The build system automatically copies the assets folder to the output directory.

For PNG support with transparency, see `SDL2_IMAGE_GUIDE.md`.

## Creating a Test Image

You can create a simple test BMP file or download one. The image should be 300x300 pixels for best results.

To create a transparent effect with BMP, use a solid background color (like magenta #FF00FF) and add color-key transparency in the code.

## Usage

- **Drag**: Left-click and hold anywhere on the window to drag it
- **Close**: Press ESC key or close from Task Manager

## Transparency Notes

The window transparency is achieved by:
1. Setting render draw blend mode to `SDL_BLENDMODE_BLEND`
2. Clearing with fully transparent color `(0, 0, 0, 0)` - alpha = 0 means fully transparent
3. The image itself is opaque (BMP format)

For true alpha channel transparency (PNG with transparent pixels showing desktop), you need SDL2_image - see `SDL2_IMAGE_GUIDE.md`.

The current setup makes the window background transparent, so areas not covered by the image will show the desktop.

## File Structure

```
dpet/
├── main.cpp                # Main application code
├── CMakeLists.txt          # Build configuration
├── SDL2_IMAGE_GUIDE.md     # Guide for adding PNG support
├── assets/
│   └── pet.bmp             # Pet image (place your BMP here)
└── README.md               # This file
```
