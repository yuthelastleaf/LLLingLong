#pragma once

#include <SDL.h>
#include <memory>
#include <atomic>
#include "Utils.h"
#include "Managers.h"

namespace DesktopPet {

/**
 * @brief Main Application Class - Orchestrates the Tri-Core architecture
 * 
 * Architecture:
 * - Main Thread (UI): SDL rendering, event handling, Lua script execution
 * - Logic Thread (AI): AI thinking, script generation
 * - Audio Thread (Sensors): ASR/TTS
 * 
 * Communication via ThreadSafeQueue<AppEvent>
 */
class App {
public:
    App() = default;
    ~App();
    
    // Disable copy
    App(const App&) = delete;
    App& operator=(const App&) = delete;
    
    /**
     * @brief Initialize the application
     */
    bool Init();
    
    /**
     * @brief Run the main application loop
     */
    void Run();
    
    /**
     * @brief Shutdown the application
     */
    void Shutdown();
    
private:
    /**
     * @brief Initialize SDL
     */
    bool InitSDL();
    
    /**
     * @brief Process SDL events
     */
    void ProcessEvents();
    
    /**
     * @brief Process application events from event queue
     */
    void ProcessAppEvents();
    
    /**
     * @brief Update application state
     */
    void Update(float deltaTime);
    
    /**
     * @brief Render the application
     */
    void Render();
    
    /**
     * @brief Clean up resources
     */
    void Cleanup();
    
    // SDL resources
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    
    // Managers (Tri-Core components)
    std::unique_ptr<UIManager> uiManager_;
    std::unique_ptr<AIEngine> aiEngine_;
    std::unique_ptr<AudioManager> audioManager_;
    std::unique_ptr<ScriptRunner> scriptRunner_;
    
    // Global Event Bus
    ThreadSafeQueue<AppEvent> eventQueue_;
    
    // Application state
    std::atomic<bool> running_{false};
    
    // Window properties
    int windowWidth_ = 500;
    int windowHeight_ = 500;
    
    // Timing
    uint32_t lastFrameTime_ = 0;
    float deltaTime_ = 0.0f;
};

} // namespace DesktopPet
