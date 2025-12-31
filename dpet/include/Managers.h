#pragma once

#include <SDL.h>
#include <sol/sol.hpp>
#include <string>
#include <thread>
#include <atomic>
#include <memory>
#include <vector>
#include <mutex>
#include "Utils.h"
#include "ContextManager.h"
#include "chat_bubble.h"

// Forward declarations for ASR and LLM
struct SherpaOnnxOfflineRecognizer;
struct ma_device;
struct llama_model;
struct llama_context;
struct llama_adapter_lora;

namespace DesktopPet {

// Dialog history for LLM
struct DialogTurn {
    std::string user_message;
    std::string assistant_message;
};

constexpr int SAMPLE_RATE = 16000;
constexpr int CHANNELS = 1;
constexpr int DEFAULT_RECORDING_SECONDS = 20;  // Reduced from 20s for faster response

/**
 * @brief UI Manager - Handles SDL rendering and visual updates
 * Runs on Main Thread
 */
class UIManager {
public:
    UIManager() = default;
    ~UIManager() = default;
    
    /**
     * @brief Initialize UI with SDL window and renderer
     */
    bool Init(SDL_Window* window, SDL_Renderer* renderer);
    
    /**
     * @brief Render the pet and UI elements
     */
    void Render();
    
    /**
     * @brief Handle UI-related events (e.g., change expression)
     */
    void HandleEvent(const AppEvent& event);
    
    /**
     * @brief Update UI state (called each frame)
     */
    void Update(float deltaTime);
    
    /**
     * @brief Load pet texture
     */
    bool LoadPetTexture(const std::string& path);
    
    /**
     * @brief Set pet expression/animation
     */
    void SetExpression(const std::string& expression);
    
    /**
     * @brief Show chat bubble
     */
    void ShowBubble(const std::string& message);
    
private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* petTexture_ = nullptr;
    
    std::string currentExpression_ = "idle";
    std::string bubbleMessage_;
    float bubbleDisplayTime_ = 0.0f;
    bool bubbleVisible_ = false;
    
    // Chat bubble window
    std::unique_ptr<ChatBubble::Bubble> chatBubble_;
};

/**
 * @brief AI Engine - Handles AI thinking and script generation
 * Runs on Logic Thread
 */
class AIEngine {
public:
    AIEngine() = default;
    ~AIEngine();
    
    // Disable copy
    AIEngine(const AIEngine&) = delete;
    AIEngine& operator=(const AIEngine&) = delete;
    
    /**
     * @brief Initialize LLM model
     */
    bool InitializeLLM(const std::string& modelPath);
    
    /**
     * @brief Start the AI thread
     * @param inputQueue Queue to receive events (AUDIO_INPUT, AI_THINK)
     * @param outputQueue Queue to send events (EXEC_LUA, UI_UPDATE)
     */
    void Start(ThreadSafeQueue<AppEvent>* inputQueue, 
               ThreadSafeQueue<AppEvent>* outputQueue);
    
    /**
     * @brief Stop the AI thread
     */
    void Stop();
    
    /**
     * @brief Check if AI thread is running
     */
    bool IsRunning() const { return running_; }
    
private:
    /**
     * @brief AI thread loop
     */
    void ThreadLoop();
    
    /**
     * @brief Process AI thinking with real LLM
     */
    std::string ChatWithLLM(const std::string& input);
    
    /**
     * @brief Cleanup LLM resources
     */
    void CleanupLLM();
    
    std::thread thread_;
    std::atomic<bool> running_{false};
    ThreadSafeQueue<AppEvent>* inputQueue_ = nullptr;
    ThreadSafeQueue<AppEvent>* outputQueue_ = nullptr;
    
    // LLM resources
    llama_model* llama_model_ = nullptr;
    llama_context* llama_context_ = nullptr;
    llama_adapter_lora* lora_adapter_ = nullptr;
    
    // Context management with sliding window
    std::unique_ptr<ContextManager> context_manager_;
};

/**
 * @brief Script Runner - Executes Lua scripts
 * Runs on Main Thread (called from main loop)
 */
class ScriptRunner {
public:
    ScriptRunner() = default;
    ~ScriptRunner() = default;
    
    /**
     * @brief Initialize Lua state and bind C++ functions
     * @param eventQueue Optional queue for sending events from Lua
     */
    bool Init(ThreadSafeQueue<AppEvent>* eventQueue = nullptr);
    
    /**
     * @brief Execute Lua script
     * @param code Lua code to execute
     * @return true if execution succeeded
     */
    bool RunScript(const std::string& code);
    
    /**
     * @brief Load and execute Lua file
     */
    bool LoadFile(const std::string& path);
    
    /**
     * @brief Get Lua state (for advanced operations)
     */
    sol::state& GetLuaState() { return lua_; }
    
private:
    /**
     * @brief Bind C++ functions to Lua
     */
    void BindFunctions();
    
    sol::state lua_;
    bool initialized_ = false;
    ThreadSafeQueue<AppEvent>* eventQueue_ = nullptr;
};

/**
 * @class AudioManager
 * @brief Audio capture and ASR manager (Audio Thread)
 */
class AudioManager {
public:
    AudioManager() = default;
    ~AudioManager();
    
    /**
     * @brief Initialize ASR recognizer
     */
    bool InitializeRecognizer(const std::string& modelDir);
    
    /**
     * @brief Start the audio thread
     * @param outputQueue Queue to send AUDIO_INPUT events
     */
    void Start(ThreadSafeQueue<AppEvent>* outputQueue);
    
    /**
     * @brief Stop the audio thread
     */
    void Stop();
    
    /**
     * @brief Check if audio thread is running
     */
    bool IsRunning() const { return running_; }
    
    /**
     * @brief Check if currently recording
     */
    bool IsRecording() const { return recording_; }
    
    /**
     * @brief Trigger recording manually
     */
    void TriggerRecording();
    
    /**
     * @brief Stop current recording
     */
    void StopRecording();

    /**
     * @brief Set/get recording timeout in seconds (default 5s)
     */
    void SetRecordingSeconds(int seconds) { recording_seconds_ = seconds; }
    int GetRecordingSeconds() const { return recording_seconds_; }
    
    /**
     * @brief Play TTS audio
     */
    void Speak(const std::string& text);
    
private:
    /**
     * @brief Audio thread loop (ASR listening)
     */
    void ThreadLoop();
    
    /**
     * @brief Record and transcribe audio
     */
    std::string RecordAndTranscribe();
    
    /**
     * @brief Transcribe audio data
     */
    std::string TranscribeAudio(const std::vector<float>& audioData);
    
    /**
     * @brief Cleanup ASR resources
     */
    void CleanupRecognizer();
    
    /**
     * @brief Audio callback for miniaudio
     */
    static void AudioCallback(ma_device* pDevice, void* pOutput, 
                             const void* pInput, unsigned int frameCount);
    
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> recording_{false};
    std::atomic<bool> trigger_recording_{false};
    ThreadSafeQueue<AppEvent>* outputQueue_ = nullptr;
    int recording_seconds_ = DEFAULT_RECORDING_SECONDS;
    
    // ASR resources
    const SherpaOnnxOfflineRecognizer* recognizer_ = nullptr;
    ma_device* audio_device_ = nullptr;
    std::vector<float> audio_buffer_;
    std::mutex buffer_mutex_;
};

} // namespace DesktopPet
