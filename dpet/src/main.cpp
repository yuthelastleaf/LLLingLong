// Desktop Pet - SDL2 + Lua scripting (Multi-threaded)
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_syswm.h>
#include <iostream>
#include <thread>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#endif

#include <sol/sol.hpp>
#include "include/pet_api.h"
#include "include/lua_bindings.h"
#include "include/lua_thread.h"

constexpr int DEFAULT_SIZE = 500;

// Global state for thread communication
std::atomic<bool> g_running{true};
Threading::LuaCommandQueue g_luaCommandQueue;

// Lua execution thread function
void luaThreadFunc() {
    try {
        // Initialize Lua in this thread
        sol::state lua;
        LuaBindings::registerPetAPI(lua);
        
        // Load init script
        if (LuaBindings::loadScript(lua, "scripts/init.lua")) {
            std::cout << "[Lua Thread] Init script loaded" << std::endl;
            
            // Call onInit if exists
            sol::optional<sol::function> onInit = lua["onInit"];
            if (onInit) {
                (*onInit)();
            }
        }
        
        // Command processing loop
        while (g_running) {
            Threading::LuaCommand cmd;
            if (g_luaCommandQueue.pop(cmd)) {
                if (cmd.type == Threading::LuaCommandType::SHUTDOWN) {
                    std::cout << "[Lua Thread] Shutdown requested" << std::endl;
                    break;
                }
                
                try {
                    if (cmd.type == Threading::LuaCommandType::CALL_FUNCTION) {
                        sol::optional<sol::function> func = lua[cmd.functionName];
                        if (func) {
                            // Call with arguments
                            if (cmd.args.empty()) {
                                (*func)();
                            } else if (cmd.args.size() == 1) {
                                (*func)(cmd.args[0]);
                            } else if (cmd.args.size() == 2) {
                                (*func)(cmd.args[0], cmd.args[1]);
                            }
                        }
                    } else if (cmd.type == Threading::LuaCommandType::EXECUTE_CODE) {
                        lua.script(cmd.code);
                    }
                } catch (const sol::error& e) {
                    std::cerr << "[Lua Thread] Error: " << e.what() << std::endl;
                }
            }
        }
        
        std::cout << "[Lua Thread] Exiting" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[Lua Thread] Fatal error: " << e.what() << std::endl;
    }
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    // Fix Chinese character display in console
    SetConsoleOutputCP(CP_UTF8);
    setvbuf(stdout, nullptr, _IOFBF, 1000);
#endif

    // Get API instance
    auto& api = PetAPI::API::getInstance();
    
    // Start Lua thread
    std::cout << "[Main Thread] Starting Lua thread..." << std::endl;
    std::thread luaThread(luaThreadFunc);
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Give Lua time to init
    
    // Initialize SDL2
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
    
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        std::cerr << "IMG_Init failed: " << IMG_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    // Load pet image
    SDL_Surface* petSurface = IMG_Load("assets/pet.png");
    if (!petSurface) {
        std::cerr << "IMG_Load failed: " << IMG_GetError() << std::endl;
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    // Debug: Check surface format
    std::cout << "Loaded PNG: " << petSurface->w << "x" << petSurface->h << std::endl;
    std::cout << "Bits per pixel: " << (int)petSurface->format->BitsPerPixel << std::endl;
    std::cout << "Has alpha: " << (SDL_ISPIXELFORMAT_ALPHA(petSurface->format->format) ? "Yes" : "No") << std::endl;

    // If PNG doesn't have proper alpha, convert it to RGBA format
    if (!SDL_ISPIXELFORMAT_ALPHA(petSurface->format->format)) {
        std::cout << "Converting surface to RGBA format..." << std::endl;
        SDL_Surface* convertedSurface = SDL_ConvertSurfaceFormat(petSurface, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_FreeSurface(petSurface);
        petSurface = convertedSurface;
    }

    // Calculate window size
    int targetSize = DEFAULT_SIZE;
    int imgWidth = petSurface->w;
    int imgHeight = petSurface->h;
    float scale = (float)targetSize / std::max(imgWidth, imgHeight);
    int windowWidth = (int)(imgWidth * scale);
    int windowHeight = (int)(imgHeight * scale);
    
    // Update API state
    api.getState().windowWidth = windowWidth;
    api.getState().windowHeight = windowHeight;

    std::cout << "Window size: " << windowWidth << "x" << windowHeight << std::endl;

    // Create window
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
        SDL_FreeSurface(petSurface);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

#ifdef _WIN32
    // Enable transparency
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (SDL_GetWindowWMInfo(window, &wmInfo)) {
        HWND hwnd = wmInfo.info.win.window;
        SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
        SetLayeredWindowAttributes(hwnd, RGB(255, 0, 255), 0, LWA_COLORKEY);
    }
#endif

    // Create renderer
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_FreeSurface(petSurface);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Create texture
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

    SDL_SetTextureBlendMode(petTexture, SDL_BLENDMODE_BLEND);

    // Set SDL window reference in API
    api.setSDLWindow(window);
    
    api.log("Desktop Pet initialized");

    // Main loop (rendering thread)
    std::cout << "[Main Thread] Entering render loop" << std::endl;
    bool running = true;
    bool isDragging = false;
    int dragOffsetX = 0;
    int dragOffsetY = 0;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        isDragging = true;
                        api.getState().isDragging = true;
                        dragOffsetX = event.button.x;
                        dragOffsetY = event.button.y;
                        
                        // Send onClick to Lua thread
                        Threading::LuaCommand cmd;
                        cmd.type = Threading::LuaCommandType::CALL_FUNCTION;
                        cmd.functionName = "onClick";
                        cmd.args.push_back(std::to_string(event.button.x));
                        cmd.args.push_back(std::to_string(event.button.y));
                        g_luaCommandQueue.push(cmd);
                    }
                    break;

                case SDL_MOUSEBUTTONUP:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        isDragging = false;
                        api.getState().isDragging = false;
                    }
                    break;

                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        running = false;
                    }
                    
                    // Send onKeyPress to Lua thread
                    Threading::LuaCommand cmd;
                    cmd.type = Threading::LuaCommandType::CALL_FUNCTION;
                    cmd.functionName = "onKeyPress";
                    cmd.args.push_back(SDL_GetKeyName(event.key.keysym.sym));
                    g_luaCommandQueue.push(cmd);
                    break;
            }
        }

        // Handle dragging
        if (isDragging) {
            int mouseX, mouseY;
            SDL_GetGlobalMouseState(&mouseX, &mouseY);
            int newX = mouseX - dragOffsetX;
            int newY = mouseY - dragOffsetY;
            SDL_SetWindowPosition(window, newX, newY);
            api.setPetPosition(newX, newY);
        }

        // Rendering (main thread only handles rendering)
        SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, petTexture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
        
        // Update bubble timer
        api.updateBubble(0.016f); // ~16ms per frame

        SDL_Delay(16);
    }

    // Cleanup
    std::cout << "[Main Thread] Shutting down..." << std::endl;
    g_running = false;
    
    // Send shutdown command to Lua thread
    Threading::LuaCommand shutdownCmd;
    shutdownCmd.type = Threading::LuaCommandType::SHUTDOWN;
    g_luaCommandQueue.push(shutdownCmd);
    
    // Wait for Lua thread to finish
    if (luaThread.joinable()) {
        luaThread.join();
    }
    
    api.log("Shutting down...");
    SDL_DestroyTexture(petTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();

    return 0;
}
