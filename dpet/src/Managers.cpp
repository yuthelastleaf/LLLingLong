#include "../include/Managers.h"
#include <SDL_image.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <ctime>

#ifdef _WIN32
#include <windows.h>
#endif

// Include ASR and LLM headers
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio/miniaudio.h"
#include "sherpa-onnx/c-api/c-api.h"
#include "llama.h"

namespace DesktopPet {

constexpr int MAX_CONTEXT_TOKENS = 1800;

// ============================================================================
// UIManager Implementation
// ============================================================================

bool UIManager::Init(SDL_Window* window, SDL_Renderer* renderer) {
    window_ = window;
    renderer_ = renderer;
    
    if (!window_ || !renderer_) {
        std::cerr << "[UIManager] Invalid window or renderer" << std::endl;
        return false;
    }
    
    // Initialize chat bubble
    chatBubble_ = std::make_unique<ChatBubble::Bubble>();
    std::cout << "[UIManager] Chat bubble initialized: " << (chatBubble_ ? "OK" : "FAILED") << std::endl;
    
    std::cout << "[UIManager] Initialized" << std::endl;
    return true;
}

bool UIManager::LoadPetTexture(const std::string& path) {
    SDL_Surface* surface = IMG_Load(path.c_str());
    if (!surface) {
        std::cerr << "[UIManager] Failed to load texture: " << IMG_GetError() << std::endl;
        return false;
    }
    
    // Convert to RGBA if needed
    if (!SDL_ISPIXELFORMAT_ALPHA(surface->format->format)) {
        SDL_Surface* converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_FreeSurface(surface);
        surface = converted;
    }
    
    petTexture_ = SDL_CreateTextureFromSurface(renderer_, surface);
    SDL_FreeSurface(surface);
    
    if (!petTexture_) {
        std::cerr << "[UIManager] Failed to create texture: " << SDL_GetError() << std::endl;
        return false;
    }
    
    SDL_SetTextureBlendMode(petTexture_, SDL_BLENDMODE_BLEND);
    std::cout << "[UIManager] Pet texture loaded: " << path << std::endl;
    return true;
}

void UIManager::Render() {
    // Clear with magenta (color key for transparency)
    SDL_SetRenderDrawColor(renderer_, 255, 0, 255, 255);
    SDL_RenderClear(renderer_);
    
    // Render pet texture
    if (petTexture_) {
        SDL_RenderCopy(renderer_, petTexture_, nullptr, nullptr);
    }
    
    // Render bubble
    if (chatBubble_) {
        chatBubble_->render();
    }
    
    SDL_RenderPresent(renderer_);
}

void UIManager::HandleEvent(const AppEvent& event) {
    switch (event.type) {
        case EventType::UI_UPDATE:
            std::cout << "[UIManager] UI Update: " << event.payload << std::endl;
            SetExpression(event.payload);
            break;
            
        default:
            break;
    }
}

void UIManager::Update(float deltaTime) {
    // Update bubble timer
    if (chatBubble_) {
        chatBubble_->update(deltaTime);
        
        // Update bubble position to follow pet window
        if (window_) {
            int x, y, w, h;
            SDL_GetWindowPosition(window_, &x, &y);
            SDL_GetWindowSize(window_, &w, &h);
            chatBubble_->updatePosition(x, y, w, h);
        }
    }
}

void UIManager::SetExpression(const std::string& expression) {
    currentExpression_ = expression;
    std::cout << "[UIManager] Expression changed to: " << expression << std::endl;
}

void UIManager::ShowBubble(const std::string& message) {
    bubbleMessage_ = message;
    bubbleVisible_ = true;
    bubbleDisplayTime_ = 0.0f;
    std::cout << "[UIManager] ShowBubble called with message: \"" << message << "\"" << std::endl;
    std::cout << "[UIManager] chatBubble_ exists: " << (chatBubble_ ? "YES" : "NO") << std::endl;
    std::cout << "[UIManager] window_ exists: " << (window_ ? "YES" : "NO") << std::endl;
    
    if (chatBubble_ && window_) {
        int x, y, w, h;
        SDL_GetWindowPosition(window_, &x, &y);
        SDL_GetWindowSize(window_, &w, &h);
        std::cout << "[UIManager] Calling bubble.show() at position (" << x << ", " << y << "), size (" << w << ", " << h << ")" << std::endl;
        chatBubble_->show(message, x, y, w, h);
        std::cout << "[UIManager] bubble.show() completed" << std::endl;
    } else {
        std::cout << "[UIManager] ERROR: Cannot show bubble - missing chatBubble_ or window_" << std::endl;
    }
}

// ============================================================================
// AIEngine Implementation
// ============================================================================

AIEngine::~AIEngine() {
    Stop();
    CleanupLLM();
}

bool AIEngine::InitializeLLM(const std::string& modelPath) {
    std::cout << "[AIEngine] Initializing LLM..." << std::endl;
    std::cout << "[AIEngine]   Model path: " << modelPath << std::endl;
    
    // Initialize llama backend
    llama_backend_init();
    llama_numa_init(GGML_NUMA_STRATEGY_DISABLED);
    
    // Load all backends including CUDA
    std::cout << "[AIEngine] Loading GPU backends..." << std::endl;
    ggml_backend_load_all();
    
    // Try to get CUDA device count for diagnostics
    std::cout << "[AIEngine] Checking CUDA availability..." << std::endl;
    
    // Configure model parameters with GPU
    llama_model_params model_params = llama_model_default_params();
    // model_params.n_gpu_layers = 99;  // Offload all layers to GPU
    model_params.n_gpu_layers = 0;  // Offload all layers to GPU
    
    std::cout << "[AIEngine] GPU acceleration enabled (n_gpu_layers=99, CUDA 12.4)" << std::endl;
    
    // Load model
    llama_model_ = llama_load_model_from_file(modelPath.c_str(), model_params);
    if (!llama_model_) {
        std::cerr << "[AIEngine] LLM model load failed" << std::endl;
        llama_backend_free();
        return false;
    }
    
    // Configure context parameters
    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = 2048;
    ctx_params.n_threads = 4;
    ctx_params.n_batch = 2048;
    
    // Create context
    llama_context_ = llama_new_context_with_model(llama_model_, ctx_params);
    if (!llama_context_) {
        std::cerr << "[AIEngine] LLM context creation failed" << std::endl;
        llama_free_model(llama_model_);
        llama_model_ = nullptr;
        llama_backend_free();
        return false;
    }
    
    std::cout << "[AIEngine] LLM loaded successfully" << std::endl;
    
    // Initialize ContextManager with sliding window (keep last 10 turns = 20 messages)
    context_manager_ = std::make_unique<ContextManager>(
        "You are a helpful AI assistant. Keep responses concise and friendly.",
        10  // Max turns to keep in memory
    );
    
    // LoRA adapter loading (commented out for testing base model)
    /*
    std::string loraPath = "F:/ollama/model/qwen2.5_7b_q4k/Shen_Lingshuang_Lora-F16-LoRA.gguf";
    FILE* loraFile = fopen(loraPath.c_str(), "rb");
    if (loraFile) {
        fclose(loraFile);
        std::cout << "[AIEngine] Loading LoRA model..." << std::endl;
        
        lora_adapter_ = llama_adapter_lora_init(llama_model_, loraPath.c_str());
        if (lora_adapter_) {
            int result = llama_set_adapter_lora(llama_context_, lora_adapter_, 1.0f);
            if (result == 0) {
                std::cout << "[AIEngine] LoRA loaded successfully (Shen_Lingshuang)" << std::endl;
            } else {
                std::cerr << "[AIEngine] LoRA application failed" << std::endl;
                llama_adapter_lora_free(lora_adapter_);
                lora_adapter_ = nullptr;
            }
        }
    } else {
        std::cout << "[AIEngine] No LoRA model detected, using base model" << std::endl;
    }
    */
    std::cout << "[AIEngine] Using base model (LoRA disabled)" << std::endl;
    
    return true;
}

void AIEngine::Start(ThreadSafeQueue<AppEvent>* inputQueue, 
                     ThreadSafeQueue<AppEvent>* outputQueue) {
    if (running_) {
        std::cout << "[AIEngine] Already running" << std::endl;
        return;
    }
    
    inputQueue_ = inputQueue;
    outputQueue_ = outputQueue;
    running_ = true;
    
    thread_ = std::thread(&AIEngine::ThreadLoop, this);
    std::cout << "[AIEngine] Started" << std::endl;
}

void AIEngine::Stop() {
    if (!running_) return;
    
    running_ = false;
    if (inputQueue_) {
        inputQueue_->shutdown();
    }
    
    if (thread_.joinable()) {
        thread_.join();
    }
    
    std::cout << "[AIEngine] Stopped" << std::endl;
}

void AIEngine::ThreadLoop() {
    std::cout << "[AIEngine] Thread loop started" << std::endl;
    
    while (running_) {
        auto eventOpt = inputQueue_->pop();
        if (!eventOpt.has_value()) {
            break;
        }
        
        AppEvent event = eventOpt.value();
        
        if (event.type == EventType::AUDIO_INPUT || event.type == EventType::AI_THINK) {
            std::cout << "[AIEngine] Processing: " << event.payload << std::endl;
            
            // Use real LLM
            std::string response = ChatWithLLM(event.payload);
            
            std::cout << "[AIEngine] LLM response: " << response << std::endl;
            
            // Escape single quotes in response for Lua
            std::string escapedResponse = response;
            size_t pos = 0;
            while ((pos = escapedResponse.find("'", pos)) != std::string::npos) {
                escapedResponse.replace(pos, 1, "\\'");
                pos += 2;
            }
            
            // Generate Lua script
            std::string luaScript = "pet.say('" + escapedResponse + "')";
            
            std::cout << "[AIEngine] Sending EXEC_LUA event with script: " << luaScript << std::endl;
            std::cout << "[AIEngine] outputQueue_ address: " << outputQueue_ << std::endl;
            
            // Send events
            outputQueue_->push(AppEvent(EventType::EXEC_LUA, luaScript));
            std::cout << "[AIEngine] EXEC_LUA event pushed, queue size: " << outputQueue_->size() << std::endl;
            outputQueue_->push(AppEvent(EventType::UI_UPDATE, "happy"));
            std::cout << "[AIEngine] UI_UPDATE event pushed, queue size: " << outputQueue_->size() << std::endl;
        }
    }
    
    std::cout << "[AIEngine] Thread loop ended" << std::endl;
}

std::string AIEngine::ChatWithLLM(const std::string& userInput) {
    if (!llama_model_ || !llama_context_ || !context_manager_) {
        return "[Error: LLM not initialized]";
    }
    
    // Get formatted prompt with sliding window history
    std::string fullPrompt = context_manager_->GetPromptString(userInput);
    
    // Tokenize
    const struct llama_vocab* vocab = llama_model_get_vocab(llama_model_);
    int n_prompt_tokens = -llama_tokenize(vocab, fullPrompt.c_str(), fullPrompt.length(), nullptr, 0, true, false);
    std::vector<llama_token> tokens(n_prompt_tokens);
    llama_tokenize(vocab, fullPrompt.c_str(), fullPrompt.length(), tokens.data(), tokens.size(), true, false);
    
    std::cout << "[AIEngine] Prompt tokens: " << n_prompt_tokens 
              << ", History size: " << context_manager_->GetHistorySize() << " messages" << std::endl;
    
    // Clear KV cache for fresh context (important for sliding window)
    llama_memory_t mem = llama_get_memory(llama_context_);
    llama_memory_clear(mem, true);
    
    // Decode prompt
    llama_batch batch = llama_batch_get_one(tokens.data(), tokens.size());
    if (llama_decode(llama_context_, batch) != 0) {
        return "[Error: Decode failed]";
    }
    
    // Generate response with GPU acceleration
    std::string response;
    const int max_tokens = 256;
    int n_generated = 0;
    
    // Create optimized sampler chain
    llama_sampler_chain_params chain_params = llama_sampler_chain_default_params();
    llama_sampler* sampler_chain = llama_sampler_chain_init(chain_params);
    llama_sampler_chain_add(sampler_chain, llama_sampler_init_penalties(64, 1.1f, 0.0f, 0.0f));
    llama_sampler_chain_add(sampler_chain, llama_sampler_init_top_p(0.95f, 1));
    llama_sampler_chain_add(sampler_chain, llama_sampler_init_temp(0.8f));
    llama_sampler_chain_add(sampler_chain, llama_sampler_init_dist(static_cast<uint32_t>(std::time(nullptr))));
    
    // Token-by-token generation
    while (n_generated < max_tokens) {
        llama_token new_token = llama_sampler_sample(sampler_chain, llama_context_, -1);
        llama_sampler_accept(sampler_chain, new_token);
        
        // Check for end-of-generation token
        if (llama_vocab_is_eog(vocab, new_token)) {
            break;
        }
        
        // Convert token to text
        char buf[256];
        int n = llama_token_to_piece(vocab, new_token, buf, sizeof(buf), 0, false);
        if (n > 0) {
            std::string token_text(buf, n);
            response.append(token_text);
            std::cout << token_text << std::flush;
            
            // Check for Qwen end marker
            if (response.find("<|im_end|>") != std::string::npos) {
                break;
            }
        }
        
        // Decode next token
        llama_batch next_batch = llama_batch_get_one(&new_token, 1);
        if (llama_decode(llama_context_, next_batch) != 0) {
            break;
        }
        
        n_generated++;
    }
    
    llama_sampler_free(sampler_chain);
    std::cout << std::endl;
    
    // Clean up response
    size_t pos = response.find("<|im_end|>");
    if (pos != std::string::npos) {
        response = response.substr(0, pos);
    }
    
    // Add to sliding window history (auto-truncates old messages)
    context_manager_->AddMessage("user", userInput);
    context_manager_->AddMessage("assistant", response);
    
    return response;
}

void AIEngine::CleanupLLM() {
    if (lora_adapter_) {
        llama_adapter_lora_free(lora_adapter_);
        lora_adapter_ = nullptr;
    }
    if (llama_context_) {
        llama_free(llama_context_);
        llama_context_ = nullptr;
    }
    if (llama_model_) {
        llama_free_model(llama_model_);
        llama_model_ = nullptr;
    }
    llama_backend_free();
}

// ============================================================================
// ScriptRunner Implementation
// ============================================================================

bool ScriptRunner::Init(ThreadSafeQueue<AppEvent>* eventQueue) {
    eventQueue_ = eventQueue;
    
    try {
        lua_.open_libraries(sol::lib::base, sol::lib::string, sol::lib::math, 
                           sol::lib::table, sol::lib::os);
        
        BindFunctions();
        
        initialized_ = true;
        std::cout << "[ScriptRunner] Initialized" << std::endl;
        return true;
    } catch (const sol::error& e) {
        std::cerr << "[ScriptRunner] Init error: " << e.what() << std::endl;
        return false;
    }
}

void ScriptRunner::BindFunctions() {
    // Create pet namespace
    auto pet = lua_["pet"].get_or_create<sol::table>();
    
    // pet.say: send SHOW_BUBBLE event if eventQueue is available
    pet["say"] = [this](const std::string& message) {
        std::cout << "[Lua] pet.say: " << message << std::endl;
        if (eventQueue_) {
            eventQueue_->push(AppEvent(EventType::SHOW_BUBBLE, message));
        }
    };
    
    pet["moveTo"] = [](int x, int y) {
        std::cout << "[Lua] pet.moveTo: (" << x << ", " << y << ")" << std::endl;
    };
    
    pet["setExpression"] = [](const std::string& expr) {
        std::cout << "[Lua] pet.setExpression: " << expr << std::endl;
    };
    
    // Create sys namespace
    auto sys = lua_["sys"].get_or_create<sol::table>();
    
    sys["lock"] = []() {
        std::cout << "[Lua] sys.lock: Locking workstation..." << std::endl;
#ifdef _WIN32
        LockWorkStation();
#endif
    };
    
    sys["shutdown"] = []() {
        std::cout << "[Lua] sys.shutdown: Shutting down..." << std::endl;
    };
    
    sys["getTime"] = []() -> std::string {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        char buffer[100];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localtime(&time_t));
        return buffer;
    };
}

bool ScriptRunner::RunScript(const std::string& code) {
    if (!initialized_) {
        std::cerr << "[ScriptRunner] Not initialized" << std::endl;
        return false;
    }
    
    try {
        lua_.script(code);
        return true;
    } catch (const sol::error& e) {
        std::cerr << "[ScriptRunner] Execution error: " << e.what() << std::endl;
        return false;
    }
}

bool ScriptRunner::LoadFile(const std::string& path) {
    if (!initialized_) {
        std::cerr << "[ScriptRunner] Not initialized" << std::endl;
        return false;
    }
    
    try {
        lua_.script_file(path);
        std::cout << "[ScriptRunner] Loaded file: " << path << std::endl;
        return true;
    } catch (const sol::error& e) {
        std::cerr << "[ScriptRunner] File load error: " << e.what() << std::endl;
        return false;
    }
}

// ============================================================================
// AudioManager Implementation
// ============================================================================

// Global audio buffer for callback
static std::vector<float> g_audio_buffer_global;
static std::mutex g_audio_mutex_global;
static std::atomic<bool> g_recording_global(false);

void AudioManager::AudioCallback(ma_device* pDevice, void* pOutput, 
                                 const void* pInput, unsigned int frameCount) {
    if (!g_recording_global) return;
    
    const float* pInputF = (const float*)pInput;
    std::lock_guard<std::mutex> lock(g_audio_mutex_global);
    
    for (unsigned int i = 0; i < frameCount; ++i) {
        g_audio_buffer_global.push_back(pInputF[i]);
    }
    (void)pOutput;
}

AudioManager::~AudioManager() {
    Stop();
    CleanupRecognizer();
    
    if (audio_device_) {
        ma_device_uninit(audio_device_);
        delete audio_device_;
        audio_device_ = nullptr;
    }
}

bool AudioManager::InitializeRecognizer(const std::string& modelDir) {
    std::cout << "[AudioManager] Initializing ASR..." << std::endl;
    std::cout << "[AudioManager]   Model dir: " << modelDir << std::endl;
    
    SherpaOnnxOfflineRecognizerConfig config;
    memset(&config, 0, sizeof(config));
    
    std::string modelPath = modelDir + "/model.onnx";
    std::string tokensPath = modelDir + "/tokens.txt";
    
    config.model_config.sense_voice.model = modelPath.c_str();
    config.model_config.sense_voice.language = "auto";
    config.model_config.sense_voice.use_itn = 1;
    config.model_config.tokens = tokensPath.c_str();
    config.model_config.num_threads = 2;
    config.model_config.provider = "cpu";
    config.model_config.debug = 0;
    
    config.decoding_method = "greedy_search";
    config.max_active_paths = 4;
    
    recognizer_ = SherpaOnnxCreateOfflineRecognizer(&config);
    if (!recognizer_) {
        std::cerr << "[AudioManager] ASR model load failed" << std::endl;
        return false;
    }
    
    std::cout << "[AudioManager] ASR loaded successfully" << std::endl;
    
    // Initialize audio device
    audio_device_ = new ma_device();
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_capture);
    deviceConfig.capture.format = ma_format_f32;
    deviceConfig.capture.channels = CHANNELS;
    deviceConfig.sampleRate = SAMPLE_RATE;
    deviceConfig.dataCallback = AudioCallback;
    deviceConfig.pUserData = nullptr;
    
    if (ma_device_init(NULL, &deviceConfig, audio_device_) != MA_SUCCESS) {
        std::cerr << "[AudioManager] Audio device init failed" << std::endl;
        delete audio_device_;
        audio_device_ = nullptr;
        return false;
    }
    
    std::cout << "[AudioManager] Audio device initialized" << std::endl;
    return true;
}

void AudioManager::Start(ThreadSafeQueue<AppEvent>* outputQueue) {
    if (running_) {
        std::cout << "[AudioManager] Already running" << std::endl;
        return;
    }
    
    outputQueue_ = outputQueue;
    running_ = true;
    
    thread_ = std::thread(&AudioManager::ThreadLoop, this);
    std::cout << "[AudioManager] Started" << std::endl;
}

void AudioManager::Stop() {
    if (!running_) return;
    
    running_ = false;
    recording_ = false;
    g_recording_global = false;
    
    if (thread_.joinable()) {
        thread_.join();
    }
    
    std::cout << "[AudioManager] Stopped" << std::endl;
}

void AudioManager::ThreadLoop() {
    std::cout << "[AudioManager] Thread loop started" << std::endl;
    
    while (running_) {
        // Wait for recording trigger
        if (trigger_recording_) {
            trigger_recording_ = false;
            std::string text = RecordAndTranscribe();
            
            if (!text.empty()) {
                std::cout << "[AudioManager] Transcribed: " << text << std::endl;
                outputQueue_->push(AppEvent(EventType::AUDIO_INPUT, text));
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));  // Faster response
    }
    
    std::cout << "[AudioManager] Thread loop ended" << std::endl;
}

void AudioManager::TriggerRecording() {
    trigger_recording_ = true;
}

void AudioManager::StopRecording() {
    recording_ = false;
}

std::string AudioManager::RecordAndTranscribe() {
    if (!audio_device_) {
        std::cerr << "[AudioManager] Audio device not initialized" << std::endl;
        return "";
    }
    
    // Clear buffer
    {
        std::lock_guard<std::mutex> lock(g_audio_mutex_global);
        g_audio_buffer_global.clear();
    }
    
    std::cout << "[AudioManager] Recording... (press SPACE again or wait " 
              << recording_seconds_ << "s)" << std::endl;
    
    g_recording_global = true;
    recording_ = true;
    
    if (ma_device_start(audio_device_) != MA_SUCCESS) {
        std::cerr << "[AudioManager] Failed to start audio device" << std::endl;
        return "";
    }
    
    // Record for N seconds or until manually stopped
    auto start_time = std::chrono::steady_clock::now();
    while (recording_ && running_) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_time).count();
        if (elapsed >= recording_seconds_) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));  // Faster check
    }
    
    g_recording_global = false;
    recording_ = false;
    ma_device_stop(audio_device_);
    
    std::cout << "[AudioManager] Recording complete" << std::endl;
    
    // Get audio data
    std::vector<float> audioData;
    {
        std::lock_guard<std::mutex> lock(g_audio_mutex_global);
        audioData = g_audio_buffer_global;
    }
    
    if (audioData.empty()) {
        std::cout << "[AudioManager] No audio data recorded" << std::endl;
        return "";
    }
    
    float duration = (float)audioData.size() / SAMPLE_RATE;
    std::cout << "[AudioManager] Audio duration: " << duration << "s" << std::endl;
    
    return TranscribeAudio(audioData);
}

std::string AudioManager::TranscribeAudio(const std::vector<float>& audioData) {
    if (!recognizer_ || audioData.empty()) {
        return "";
    }
    
    const SherpaOnnxOfflineStream* stream = SherpaOnnxCreateOfflineStream(recognizer_);
    if (!stream) {
        std::cerr << "[AudioManager] Failed to create stream" << std::endl;
        return "";
    }
    
    std::cout << "[AudioManager] Transcribing..." << std::endl;
    
    SherpaOnnxAcceptWaveformOffline(stream, SAMPLE_RATE, audioData.data(), audioData.size());
    SherpaOnnxDecodeOfflineStream(recognizer_, stream);
    
    const SherpaOnnxOfflineRecognizerResult* result = SherpaOnnxGetOfflineStreamResult(stream);
    std::string text;
    if (result && result->text) {
        text = result->text;
    }
    
    SherpaOnnxDestroyOfflineRecognizerResult(result);
    SherpaOnnxDestroyOfflineStream(stream);
    
    return text;
}

void AudioManager::CleanupRecognizer() {
    if (recognizer_) {
        SherpaOnnxDestroyOfflineRecognizer(recognizer_);
        recognizer_ = nullptr;
    }
}

void AudioManager::Speak(const std::string& text) {
    std::cout << "[AudioManager] TTS: " << text << std::endl;
    // TODO: Implement actual TTS
}

} // namespace DesktopPet
