// Desktop Pet - Draggable transparent window with SDL2
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_syswm.h>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif


int main(int argc, char* argv[]) {
    // Initialize SDL2 with video subsystem
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    // Initialize SDL2_image with PNG support
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        std::cerr << "IMG_Init failed: " << IMG_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    // IMPORTANT: Set highest quality scaling BEFORE creating renderer
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2"); // 2 = best quality (anisotropic)
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1"); // Enable vsync for smooth rendering

    // Load pet image first to get dimensions
    SDL_Surface* petSurface = IMG_Load("assets/pet.png");
    if (!petSurface) {
        std::cerr << "IMG_Load failed for assets/pet.png: " << IMG_GetError() << std::endl;
        std::cerr << "Make sure assets/pet.png exists in the executable directory." << std::endl;
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // Calculate window size: scale down large images to reasonable desktop pet size
    // Target size: 500 pixels for better quality on modern displays
    int targetSize = 500;
    int imgWidth = petSurface->w;
    int imgHeight = petSurface->h;
    float scale = (float)targetSize / std::max(imgWidth, imgHeight);
    int windowWidth = (int)(imgWidth * scale);
    int windowHeight = (int)(imgHeight * scale);

    std::cout << "Original image: " << imgWidth << "x" << imgHeight << std::endl;
    std::cout << "Window size: " << windowWidth << "x" << windowHeight << " (scale: " << scale << ")" << std::endl;

    // Create window: borderless, always on top, centered
    // Note: SDL_WINDOW_ALWAYS_ON_TOP requires SDL 2.0.5+
    SDL_Window* window = SDL_CreateWindow(
        "Desktop Pet",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        windowWidth,
        windowHeight,
        SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALWAYS_ON_TOP
    );

    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

#ifdef _WIN32
    // CRUCIAL: Enable true transparency on Windows using layered window
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (SDL_GetWindowWMInfo(window, &wmInfo)) {
        HWND hwnd = wmInfo.info.win.window;
        // Set window as layered to support per-pixel alpha
        SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
        // Use magenta (255,0,255) as transparent color key - unlikely to appear in pet images
        SetLayeredWindowAttributes(hwnd, RGB(255, 0, 255), 0, LWA_COLORKEY);
    }
#endif

    // Create renderer with best quality settings
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // CRUCIAL: Enable blend mode for transparency support
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Debug: Check if surface has alpha channel
    std::cout << "Bits per pixel: " << (int)petSurface->format->BitsPerPixel << std::endl;
    std::cout << "Has alpha: " << (SDL_ISPIXELFORMAT_ALPHA(petSurface->format->format) ? "Yes" : "No") << std::endl;

    // If PNG doesn't have proper alpha, convert it to RGBA format
    SDL_Surface* convertedSurface = nullptr;
    if (!SDL_ISPIXELFORMAT_ALPHA(petSurface->format->format)) {
        std::cout << "Converting surface to RGBA format..." << std::endl;
        convertedSurface = SDL_ConvertSurfaceFormat(petSurface, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_FreeSurface(petSurface);
        petSurface = convertedSurface;
    }

    SDL_Texture* petTexture = SDL_CreateTextureFromSurface(renderer, petSurface);
    SDL_FreeSurface(petSurface);

    if (!petTexture) {
        std::cerr << "SDL_CreateTextureFromSurface failed: " << SDL_GetError() << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // CRUCIAL: Enable alpha blending for the texture to support PNG transparency
    SDL_SetTextureBlendMode(petTexture, SDL_BLENDMODE_BLEND);

    // Dragging state
    bool isDragging = false;
    int dragOffsetX = 0;
    int dragOffsetY = 0;

    // Main loop
    bool running = true;
    SDL_Event event;

    while (running) {
        // Event handling
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        isDragging = true;
                        int windowX, windowY;
                        SDL_GetWindowPosition(window, &windowX, &windowY);
                        // Store offset from window position to mouse position
                        dragOffsetX = event.button.x;
                        dragOffsetY = event.button.y;
                    }
                    break;

                case SDL_MOUSEBUTTONUP:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        isDragging = false;
                    }
                    break;

                case SDL_KEYDOWN:
                    // ESC key to quit
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        running = false;
                    }
                    break;
            }
        }

        // Handle dragging
        if (isDragging) {
            int mouseX, mouseY;
            SDL_GetGlobalMouseState(&mouseX, &mouseY);
            // Set window position: global mouse position minus the drag offset
            SDL_SetWindowPosition(window, mouseX - dragOffsetX, mouseY - dragOffsetY);
        }

        // Rendering
        // CRUCIAL: Clear with magenta (255,0,255) which becomes transparent via color key
        // Magenta is chosen because it's unlikely to appear in natural pet images
        SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
        SDL_RenderClear(renderer);

        // Render pet image to fill the entire window
        SDL_RenderCopy(renderer, petTexture, nullptr, nullptr);

        // Present the rendered frame
        SDL_RenderPresent(renderer);

        // Small delay to avoid consuming 100% CPU
        SDL_Delay(16); // ~60 FPS
    }

    // Cleanup
    SDL_DestroyTexture(petTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();

    return 0;
}
