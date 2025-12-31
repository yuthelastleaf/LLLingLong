#include "../include/App.h"
#include <SDL_image.h>
#include <SDL_syswm.h>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace DesktopPet {

App::~App() {
    Shutdown();
}

bool App::Init() {
    std::cout << "=== Desktop Pet Tri-Core Architecture ===" << std::endl;
    std::cout << "Initializing..." << std::endl;
    
    // Initialize SDL
    if (!InitSDL()) {
        return false;
    }
    
    // Create managers
    uiManager_ = std::make_unique<UIManager>();
    aiEngine_ = std::make_unique<AIEngine>();
    audioManager_ = std::make_unique<AudioManager>();
    scriptRunner_ = std::make_unique<ScriptRunner>();
    
    // Initialize UI Manager
    if (!uiManager_->Init(window_, renderer_)) {
        std::cerr << "[App] Failed to initialize UIManager" << std::endl;
        return false;
    }
    
    // Load pet texture
    if (!uiManager_->LoadPetTexture("assets/pet.png")) {
        std::cerr << "[App] Failed to load pet texture" << std::endl;
        return false;
    }
    
    // Initialize Script Runner
    if (!scriptRunner_->Init(&eventQueue_)) {
        std::cerr << "[App] Failed to initialize ScriptRunner" << std::endl;
        return false;
    }
    
    // Initialize ASR
    std::string asrModelDir = "F:/ollama/model/SenseVoidSmall-onnx-official";
    std::cout << "[App] Initializing ASR..." << std::endl;
    if (!audioManager_->InitializeRecognizer(asrModelDir)) {
        std::cerr << "[App] Failed to initialize ASR" << std::endl;
        return false;
    }
    
    // Initialize LLM
    // std::string llmModelPath = "F:/ollama/model/qwen2.5_7b_q4k/qwen2.5-7b-instruct-q4_k_m-00001-of-00002.gguf";
    std::string llmModelPath = "F:/ollama/model/qwen2.5_7b_q4k/qwen2.5-3b-instruct-q4_k_m.gguf";
    std::cout << "[App] Initializing LLM..." << std::endl;
    if (!aiEngine_->InitializeLLM(llmModelPath)) {
        std::cerr << "[App] Failed to initialize LLM" << std::endl;
        return false;
    }
    
    // Start AI Engine thread
    std::cout << "[App] eventQueue_ address: " << &eventQueue_ << std::endl;
    aiEngine_->Start(&eventQueue_, &eventQueue_);
    
    // Start Audio Manager thread
    audioManager_->Start(&eventQueue_);
    
    std::cout << "[App] Initialization complete" << std::endl;
    std::cout << "Architecture:" << std::endl;
    std::cout << "  - Main Thread: UI rendering + Lua execution" << std::endl;
    std::cout << "  - Logic Thread: LLM (Qwen2.5-7B)" << std::endl;
    std::cout << "  - Audio Thread: ASR (SenseVoice)" << std::endl;
    std::cout << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  - SPACE: Start/Stop voice recording (max 5s)" << std::endl;
    std::cout << "  - H: Test hello message" << std::endl;
    std::cout << "  - T: Test time query" << std::endl;
    std::cout << "  - ESC: Exit" << std::endl;
    std::cout << "  - Drag with mouse to move pet" << std::endl;
    std::cout << "  - Logic Thread: AI thinking" << std::endl;
    std::cout << "  - Audio Thread: ASR/TTS simulation" << std::endl;
    
    return true;
}

bool App::InitSDL() {
#ifdef _WIN32
    // Enable UTF-8 console output
    SetConsoleOutputCP(CP_UTF8);
#endif
    
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "[App] SDL_Init failed: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Initialize SDL_image
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        std::cerr << "[App] IMG_Init failed: " << IMG_GetError() << std::endl;
        SDL_Quit();
        return false;
    }
    
    // Set quality hints
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");
    
    // Create window
    window_ = SDL_CreateWindow(
        "Desktop Pet - Tri-Core",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        windowWidth_,
        windowHeight_,
        SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALWAYS_ON_TOP
    );
    
    if (!window_) {
        std::cerr << "[App] SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        IMG_Quit();
        SDL_Quit();
        return false;
    }
    
#ifdef _WIN32
    // Enable transparency
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (SDL_GetWindowWMInfo(window_, &wmInfo)) {
        HWND hwnd = wmInfo.info.win.window;
        SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
        SetLayeredWindowAttributes(hwnd, RGB(255, 0, 255), 0, LWA_COLORKEY);
    }
#endif
    
    // Create renderer
    renderer_ = SDL_CreateRenderer(window_, -1, 
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    
    if (!renderer_) {
        std::cerr << "[App] SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window_);
        IMG_Quit();
        SDL_Quit();
        return false;
    }
    
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    
    std::cout << "[App] SDL initialized" << std::endl;
    return true;
}

void App::Run() {
    running_ = true;
    lastFrameTime_ = SDL_GetTicks();
    
    std::cout << "[App] Entering main loop" << std::endl;
    
    while (running_) {
        // Calculate delta time
        uint32_t currentTime = SDL_GetTicks();
        deltaTime_ = (currentTime - lastFrameTime_) / 1000.0f;
        lastFrameTime_ = currentTime;
        
        // Process SDL events
        ProcessEvents();
        
        // Process application events from event queue
        ProcessAppEvents();
        
        // Update
        Update(deltaTime_);
        
        // Render
        Render();
        
        // Frame rate control (60 FPS)
        SDL_Delay(16);
    }
    
    std::cout << "[App] Main loop ended" << std::endl;
}

void App::ProcessEvents() {
    static bool isDragging = false;
    static int dragOffsetX = 0;
    static int dragOffsetY = 0;
    
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                running_ = false;
                break;
                
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running_ = false;
                } else if (event.key.keysym.sym == SDLK_SPACE) {
                    // Toggle recording: start or stop
                    if (audioManager_) {
                        if (audioManager_->IsRecording()) {
                            std::cout << "[App] Stopping recording..." << std::endl;
                            audioManager_->StopRecording();
                        } else {
                            std::cout << "[App] Starting voice recording..." << std::endl;
                            audioManager_->TriggerRecording();
                        }
                    }
                } else if (event.key.keysym.sym == SDLK_h) {
                    // Manual test: say hello
                    eventQueue_.push(AppEvent(EventType::AI_THINK, "hello"));
                } else if (event.key.keysym.sym == SDLK_t) {
                    // Manual test: ask time
                    eventQueue_.push(AppEvent(EventType::AI_THINK, "what time is it"));
                }
                break;
                
            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    isDragging = true;
                    dragOffsetX = event.button.x;
                    dragOffsetY = event.button.y;
                    std::cout << "[App] Pet clicked" << std::endl;
                    eventQueue_.push(AppEvent(EventType::AI_THINK, "user clicked me"));
                }
                break;
                
            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    isDragging = false;
                }
                break;
                
            case SDL_MOUSEMOTION:
                if (isDragging) {
                    int mouseX, mouseY;
                    SDL_GetGlobalMouseState(&mouseX, &mouseY);
                    int newX = mouseX - dragOffsetX;
                    int newY = mouseY - dragOffsetY;
                    SDL_SetWindowPosition(window_, newX, newY);
                }
                break;
        }
    }
}

void App::ProcessAppEvents() {
    // Process all events in the queue (non-blocking)
    static int callCount = 0;
    callCount++;
    if (callCount % 60 == 1) {  // Log every 60 calls (~1 second)
        std::cout << "[App] ProcessAppEvents called (" << callCount << "), queue size: " << eventQueue_.size() << std::endl;
    }
    
    int eventCount = 0;
    while (true) {
        auto eventOpt = eventQueue_.tryPop();
        if (!eventOpt.has_value()) {
            break;
        }
        
        eventCount++;
        AppEvent event = eventOpt.value();
        
        std::cout << "[App] Processing event #" << eventCount << ", type: " << static_cast<int>(event.type) << std::endl;
        
        switch (event.type) {
            case EventType::EXEC_LUA:
                std::cout << "[App] Executing Lua: " << event.payload << std::endl;
                scriptRunner_->RunScript(event.payload);
                break;
                
            case EventType::UI_UPDATE:
                uiManager_->HandleEvent(event);
                break;
                
            case EventType::SHOW_BUBBLE:
                std::cout << "[App] Showing bubble: " << event.payload << std::endl;
                uiManager_->ShowBubble(event.payload);
                break;
                
            case EventType::AUDIO_INPUT:
                std::cout << "[App] Audio input received: " << event.payload << std::endl;
                // Forward to AI for processing
                eventQueue_.push(AppEvent(EventType::AI_THINK, event.payload));
                break;
                
            case EventType::SHUTDOWN:
                running_ = false;
                break;
                
            default:
                break;
        }
    }
}

void App::Update(float deltaTime) {
    uiManager_->Update(deltaTime);
}

void App::Render() {
    uiManager_->Render();
}

void App::Shutdown() {
    if (!running_ && !aiEngine_ && !audioManager_) {
        return; // Already shut down
    }
    
    std::cout << "[App] Shutting down..." << std::endl;
    
    running_ = false;
    
    // Stop threads
    if (aiEngine_) {
        aiEngine_->Stop();
    }
    
    if (audioManager_) {
        audioManager_->Stop();
    }
    
    // Cleanup
    Cleanup();
    
    std::cout << "[App] Shutdown complete" << std::endl;
}

void App::Cleanup() {
    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    
    IMG_Quit();
    SDL_Quit();
}

} // namespace DesktopPet
