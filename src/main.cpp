#include <iostream>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include <chrono>
#include <thread>
#include <sstream>
#include <ctime>
#include <regex>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#endif

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "sherpa-onnx/c-api/c-api.h"
#include "llama.h"

constexpr int SAMPLE_RATE = 16000;
constexpr int CHANNELS = 1;
constexpr int RECORDING_SECONDS = 20;

// 全局变量
std::atomic<bool> g_recording(false);
std::vector<float> g_audioBuffer;
std::mutex g_bufferMutex;
const SherpaOnnxOfflineRecognizer* g_recognizer = nullptr;
bool g_debugMode = false;

// LLM 全局变量
llama_model* g_llama_model = nullptr;
llama_context* g_llama_context = nullptr;
llama_adapter_lora* g_lora_adapter = nullptr;

// 对话历史管理
struct DialogTurn {
    std::string user_message;
    std::string assistant_message;
};
std::vector<DialogTurn> g_dialog_history;
constexpr int MAX_CONTEXT_TOKENS = 1800;  // 留一些余量，实际上下文是2048

void audio_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    if (!g_recording) {
        return;
    }
    const float* pInputF = (const float*)pInput;
    std::lock_guard<std::mutex> lock(g_bufferMutex);
    for (ma_uint32 i = 0; i < frameCount; ++i) {
        g_audioBuffer.push_back(pInputF[i]);
    }
    (void)pOutput;
}

void SaveAudioToWav(const std::vector<float>& audioData, const std::string& filename) {
    ma_encoder_config config = ma_encoder_config_init(ma_encoding_format_wav, ma_format_f32, CHANNELS, SAMPLE_RATE);
    ma_encoder encoder;
    if (ma_encoder_init_file(filename.c_str(), &config, &encoder) == MA_SUCCESS) {
        ma_uint64 framesWritten;
        ma_encoder_write_pcm_frames(&encoder, audioData.data(), audioData.size() / CHANNELS, &framesWritten);
        ma_encoder_uninit(&encoder);
        if (g_debugMode) {
            std::cout << "音频已保存到: " << filename << std::endl;
        }
    }
}

// 初始化识别器
bool InitializeRecognizer(const std::string& modelDir) {
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
    config.model_config.debug = g_debugMode ? 1 : 0;
    
    config.decoding_method = "greedy_search";
    config.max_active_paths = 4;
    
    if (g_debugMode) {
        std::cout << "正在加载模型..." << std::endl;
        std::cout << "  模型: " << modelPath << std::endl;
        std::cout << "  词表: " << tokensPath << std::endl;
    }
    
    try {
        g_recognizer = SherpaOnnxCreateOfflineRecognizer(&config);
        if (g_recognizer) {
            std::cout << "✓ 模型加载成功" << std::endl;
            return true;
        } else {
            std::cerr << "✗ 模型加载失败 (返回 nullptr)" << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "✗ 模型加载异常: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "✗ 模型加载发生未知异常" << std::endl;
        return false;
    }
}

// 清理识别器
void CleanupRecognizer() {
    if (g_recognizer) {
        SherpaOnnxDestroyOfflineRecognizer(g_recognizer);
        g_recognizer = nullptr;
        if (g_debugMode) {
            std::cout << "识别器已清理" << std::endl;
        }
    }
}

// 初始化 LLM
bool InitializeLLM(const std::string& modelPath) {
    if (g_debugMode) {
        std::cout << "正在初始化 LLM..." << std::endl;
        std::cout << "  模型路径: " << modelPath << std::endl;
    }
    
    // 初始化 llama backend
    llama_backend_init();
    llama_numa_init(GGML_NUMA_STRATEGY_DISABLED);
    
    // 加载所有 backends（包括CUDA）
    ggml_backend_load_all();
    
    // 配置模型参数
    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = 99;  // GPU 模式：尽可能多地使用GPU层（99表示所有层）
    
    std::cout << "✓ GPU加速已启用 (n_gpu_layers=99)" << std::endl;
    
    // 加载模型
    g_llama_model = llama_load_model_from_file(modelPath.c_str(), model_params);
    if (!g_llama_model) {
        std::cerr << "✗ LLM 模型加载失败" << std::endl;
        llama_backend_free();
        return false;
    }
    
    // 配置上下文参数
    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = 2048;  // 上下文长度
    ctx_params.n_threads = 4;  // 线程数
    ctx_params.n_batch = 2048;  // 批处理大小（必须 >= 最大输入token数）
    
    // 创建上下文
    g_llama_context = llama_new_context_with_model(g_llama_model, ctx_params);
    if (!g_llama_context) {
        std::cerr << "✗ LLM 上下文创建失败" << std::endl;
        llama_free_model(g_llama_model);
        g_llama_model = nullptr;
        llama_backend_free();
        return false;
    }
    
    std::cout << "✓ LLM 加载成功" << std::endl;
    
    // 检查并加载 LoRA 模型（如果存在）
    std::string loraPath = "F:/ollama/model/qwen2.5_7b_q4k/Shen_Lingshuang_Lora-F16-LoRA.gguf";
    
    // 检查文件是否存在
    FILE* loraFile = fopen(loraPath.c_str(), "rb");
    if (loraFile) {
        fclose(loraFile);
        std::cout << "检测到 LoRA 模型，正在加载..." << std::endl;
        
        g_lora_adapter = llama_adapter_lora_init(g_llama_model, loraPath.c_str());
        if (g_lora_adapter) {
            // 应用 LoRA 到 context，scale=1.0 表示完全应用
            int result = llama_set_adapter_lora(g_llama_context, g_lora_adapter, 1.0f);
            if (result == 0) {
                std::cout << "✓ LoRA 模型加载成功 (Shen_Lingshuang)" << std::endl;
            } else {
                std::cerr << "✗ LoRA 应用失败" << std::endl;
                llama_adapter_lora_free(g_lora_adapter);
                g_lora_adapter = nullptr;
            }
        } else {
            std::cerr << "✗ LoRA 模型加载失败" << std::endl;
        }
    } else {
        std::cout << "未检测到 LoRA 模型，使用基础模型" << std::endl;
    }
    
    return true;
}

// 清理 LLM
void CleanupLLM() {
    // 先释放 LoRA 适配器
    if (g_lora_adapter) {
        llama_adapter_lora_free(g_lora_adapter);
        g_lora_adapter = nullptr;
    }
    
    if (g_llama_context) {
        llama_free(g_llama_context);
        g_llama_context = nullptr;
    }
    if (g_llama_model) {
        llama_free_model(g_llama_model);
        g_llama_model = nullptr;
    }
    llama_backend_free();
    
    if (g_debugMode) {
        std::cout << "LLM 已清理" << std::endl;
    }
}

// LLM 对话（带对话历史管理）
std::string ChatWithLLM(const std::string& userInput) {
    if (!g_llama_model || !g_llama_context) {
        return "[错误: LLM 未初始化]";
    }
    
    // 构建包含历史的完整对话（沈凌霜角色设定）
    std::string fullPrompt = "<|im_start|>system\n你是沈凌霜，凌云门大弟子。你身受重伤被玩家所救。请务必使用 JSON 格式回答，包含 text, action, expression, affection 字段。只返回一个JSON对象，然后立即用<|im_end|>结束。<|im_end|>\n";
    
    // 添加历史对话
    for (const auto& turn : g_dialog_history) {
        fullPrompt += "<|im_start|>user\n" + turn.user_message + "<|im_end|>\n";
        fullPrompt += "<|im_start|>assistant\n" + turn.assistant_message + "<|im_end|>\n";
    }
    
    // 添加当前用户输入
    fullPrompt += "<|im_start|>user\n" + userInput + "<|im_end|>\n";
    fullPrompt += "<|im_start|>assistant\n";
    
    // 获取 model 的 vocab
    const struct llama_vocab * vocab = llama_model_get_vocab(g_llama_model);
    
    // Tokenize 输入 - 先获取需要的 token 数量
    int n_prompt_tokens = -llama_tokenize(vocab, fullPrompt.c_str(), fullPrompt.length(), nullptr, 0, true, false);
    std::vector<llama_token> tokens(n_prompt_tokens);
    llama_tokenize(vocab, fullPrompt.c_str(), fullPrompt.length(), tokens.data(), tokens.size(), true, false);
    
    // 如果超过token限制，删除最早的对话轮次
    while (n_prompt_tokens > MAX_CONTEXT_TOKENS && !g_dialog_history.empty()) {
        std::cout << "[系统] 上下文过长（" << n_prompt_tokens << " tokens），删除最早的对话..." << std::endl;
        g_dialog_history.erase(g_dialog_history.begin());
        
        // 重新构建prompt（沈凌霜角色设定）
        fullPrompt = "<|im_start|>system\n你是沈凌霜，凌云门大弟子。你身受重伤被玩家所救。请务必使用 JSON 格式回答，包含 text, action, expression, affection 字段。只返回一个JSON对象，然后立即用<|im_end|>结束。<|im_end|>\n";
        for (const auto& turn : g_dialog_history) {
            fullPrompt += "<|im_start|>user\n" + turn.user_message + "<|im_end|>\n";
            fullPrompt += "<|im_start|>assistant\n" + turn.assistant_message + "<|im_end|>\n";
        }
        fullPrompt += "<|im_start|>user\n" + userInput + "<|im_end|>\n";
        fullPrompt += "<|im_start|>assistant\n";
        
        // 重新分词
        n_prompt_tokens = -llama_tokenize(vocab, fullPrompt.c_str(), fullPrompt.length(), nullptr, 0, true, false);
        tokens.resize(n_prompt_tokens);
        llama_tokenize(vocab, fullPrompt.c_str(), fullPrompt.length(), tokens.data(), tokens.size(), true, false);
    }
    
    if (g_debugMode) {
        std::cout << "[系统] 使用 " << n_prompt_tokens << " 个tokens（历史对话: " 
                  << g_dialog_history.size() << " 轮）" << std::endl;
        std::cout << "[LLM输入] " << userInput << std::endl;
    }
    
    // **关键：清空KV缓存**
    // llama_kv_cache_clear(g_llama_context);
    // ✅ 新写法：先获取 memory 对象，再执行清理
    llama_memory_t mem = llama_get_memory(g_llama_context);
    llama_memory_clear(mem, true); // true 表示同时清理数据缓存
    
    // 创建 batch 并填充 tokens
    llama_batch batch = llama_batch_get_one(tokens.data(), tokens.size());
    
    // Decode
    if (llama_decode(g_llama_context, batch) != 0) {
        return "[错误: Decode 失败]";
    }
    
    // 生成回复（流式输出，使用 sampler 链）
    std::string response;
    const int max_tokens = 512;
    int n_generated = 0;
    
    // 创建 sampler 链（添加温度、top_p、重复惩罚）
    llama_sampler_chain_params chain_params = llama_sampler_chain_default_params();
    llama_sampler* sampler_chain = llama_sampler_chain_init(chain_params);
    
    // 添加重复惩罚（penalty_repeat=1.1, penalty_last_n=64）
    llama_sampler_chain_add(sampler_chain, 
        llama_sampler_init_penalties(64, 1.1f, 0.0f, 0.0f));
    
    // 添加 top_p 采样（nucleus sampling, p=0.95）
    llama_sampler_chain_add(sampler_chain, 
        llama_sampler_init_top_p(0.95f, 1));
    
    // 添加温度参数（temperature=0.8，增加随机性）
    llama_sampler_chain_add(sampler_chain, 
        llama_sampler_init_temp(0.8f));
    
    // 添加分布采样（使用随机种子）
    llama_sampler_chain_add(sampler_chain, 
        llama_sampler_init_dist(static_cast<uint32_t>(std::time(nullptr))));
    
    while (n_generated < max_tokens) {
        // 使用 sampler 链进行采样
        llama_token new_token = llama_sampler_sample(sampler_chain, g_llama_context, -1);
        
        // 接受采样的token（用于重复惩罚的历史追踪）
        llama_sampler_accept(sampler_chain, new_token);
        
        // 检查是否是结束 token
        if (llama_vocab_is_eog(vocab, new_token)) {
            break;
        }
        
        // 转换 token 为文本
        char buf[256];
        int n = llama_token_to_piece(vocab, new_token, buf, sizeof(buf), 0, false);
        if (n > 0) {
            std::string token_text(buf, n);
            response.append(token_text);
            
            // 流式输出：立即显示生成的token
            std::cout << token_text << std::flush;
            
            // 检查是否生成了结束标记 <|im_end|>
            if (response.find("<|im_end|>") != std::string::npos) {
                break;
            }
            
            // 检测完整JSON对象后的异常模式（多个JSON连续生成）
            // 如果已经有一个完整的JSON（以}结尾），且又开始生成新的{，说明模型在重复生成
            size_t last_brace = response.rfind('}');
            if (last_brace != std::string::npos && last_brace < response.length() - 1) {
                // 检查}后面是否有新的JSON开始标志
                std::string after_brace = response.substr(last_brace + 1);
                if (after_brace.find('{') != std::string::npos || 
                    after_brace.find("\"text\"") != std::string::npos) {
                    // 检测到重复生成，截断到第一个完整JSON
                    response = response.substr(0, last_brace + 1);
                    break;
                }
            }
        }
        
        // 继续生成下一个 token
        llama_batch next_batch = llama_batch_get_one(&new_token, 1);
        if (llama_decode(g_llama_context, next_batch) != 0) {
            break;
        }
        
        n_generated++;
    }
    
    // 释放 sampler 链
    llama_sampler_free(sampler_chain);
    
    std::cout << std::endl;  // 输出换行
    
    // 移除尾部的 <|im_end|> 标记
    size_t pos = response.find("<|im_end|>");
    if (pos != std::string::npos) {
        response = response.substr(0, pos);
    }
    
    // 保存到对话历史
    g_dialog_history.push_back({userInput, response});
    
    return response;
}

// 转录音频
std::string TranscribeAudio(const std::vector<float>& audioData) {
    if (!g_recognizer || audioData.empty()) {
        return "";
    }
    
    const SherpaOnnxOfflineStream* stream = SherpaOnnxCreateOfflineStream(g_recognizer);
    if (!stream) {
        std::cerr << "✗ 创建流失败" << std::endl;
        return "";
    }
    
    if (g_debugMode) {
        std::cout << "正在转录音频..." << std::endl;
    }
    
    SherpaOnnxAcceptWaveformOffline(stream, SAMPLE_RATE, audioData.data(), audioData.size());
    SherpaOnnxDecodeOfflineStream(g_recognizer, stream);
    
    const SherpaOnnxOfflineRecognizerResult* result = SherpaOnnxGetOfflineStreamResult(stream);
    std::string text;
    if (result && result->text) {
        text = result->text;
    }
    
    SherpaOnnxDestroyOfflineRecognizerResult(result);
    SherpaOnnxDestroyOfflineStream(stream);
    
    return text;
}

// 录音并转录
std::string RecordAndTranscribe(ma_device& device) {
    // 清空缓冲区
    {
        std::lock_guard<std::mutex> lock(g_bufferMutex);
        g_audioBuffer.clear();
    }
    
    std::cout << "\n[录音中] 请说话，最长 " << RECORDING_SECONDS << " 秒（按回车提前结束）..." << std::endl;
    std::cout << "========================================" << std::endl;
    
    g_recording = true;
    if (ma_device_start(&device) != MA_SUCCESS) {
        std::cerr << "✗ 启动音频设备失败" << std::endl;
        return "";
    }
    
    // 录音 N 秒，或者用户按回车提前结束
    auto start_time = std::chrono::steady_clock::now();
    bool manual_stop = false;
    
    // 创建线程检测用户输入
    std::thread input_thread([&manual_stop]() {
        std::cin.get();  // 等待用户按回车
        manual_stop = true;
    });
    input_thread.detach();
    
    while (g_recording) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_time).count();
        if (elapsed >= RECORDING_SECONDS || manual_stop) {
            break;
        }
    }
    
    g_recording = false;
    ma_device_stop(&device);
    
    if (manual_stop) {
        std::cout << "[手动停止录音]" << std::endl;
    } else {
        std::cout << "[录音完成]" << std::endl;
    }
    
    // 获取录音数据
    std::vector<float> audioData;
    {
        std::lock_guard<std::mutex> lock(g_bufferMutex);
        audioData = g_audioBuffer;
    }
    
    if (audioData.empty()) {
        std::cout << "✗ 未录制到音频数据" << std::endl;
        return "";
    }
    
    float duration = (float)audioData.size() / SAMPLE_RATE;
    std::cout << "音频时长: " << duration << " 秒" << std::endl;
    
    // 转录
    std::string transcription = TranscribeAudio(audioData);
    
    return transcription;
}

int main(int argc, char* argv[]) {
    // 设置控制台为 UTF-8 编码，解决中文乱码问题
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif
    
    // 解析命令行参数
    std::string modelDir = "F:/ollama/model/SenseVoidSmall-onnx-official";
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--debug" || arg == "-d") {
            g_debugMode = true;
        } else {
            // 第一个非选项参数作为模型路径
            modelDir = arg;
        }
    }
    
    std::cout << "=== Windows 音频采集与转录 Demo ===" << std::endl;
    std::cout << "基于 miniaudio + sherpa-onnx + llama.cpp" << std::endl;
    std::cout << "模型路径: " << modelDir << std::endl;
    std::cout << "调试模式: " << (g_debugMode ? "开启" : "关闭") << std::endl;
    std::cout << std::endl;
    
    // 启动时预加载ASR模型
    std::cout << "正在加载ASR模型..." << std::endl;
    if (!InitializeRecognizer(modelDir)) {
        std::cerr << "ASR模型加载失败，程序退出！" << std::endl;
        return 1;
    }
    std::cout << "ASR模型加载成功！" << std::endl;
    std::cout << std::endl;
    
    // 初始化 LLM
    std::string llmModelPath = "F:/ollama/model/qwen2.5_7b_q4k/qwen2.5-7b-instruct-q4_k_m-00001-of-00002.gguf";
    std::cout << "正在加载LLM模型..." << std::endl;
    if (!InitializeLLM(llmModelPath)) {
        std::cerr << "LLM模型加载失败，程序退出！" << std::endl;
        CleanupRecognizer();
        return 1;
    }
    std::cout << std::endl;
    
    // 初始化音频设备
    ma_device_config deviceConfig;
    ma_device device;
    
    deviceConfig = ma_device_config_init(ma_device_type_capture);
    deviceConfig.capture.format = ma_format_f32;
    deviceConfig.capture.channels = CHANNELS;
    deviceConfig.sampleRate = SAMPLE_RATE;
    deviceConfig.dataCallback = audio_callback;
    deviceConfig.pUserData = nullptr;
    
    if (g_debugMode) {
        std::cout << "正在初始化音频设备..." << std::endl;
    }
    if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
        std::cerr << "音频设备初始化失败！" << std::endl;
        CleanupRecognizer();
        return 1;
    }
    
    if (g_debugMode) {
        std::cout << "音频设备初始化成功！" << std::endl;
        std::cout << "设备名称: " << device.capture.name << std::endl;
        std::cout << "采样率: " << SAMPLE_RATE << " Hz" << std::endl;
        std::cout << "声道数: " << CHANNELS << std::endl;
        std::cout << std::endl;
    }
    
    // 显示帮助信息
    std::cout << "命令列表:" << std::endl;
    std::cout << "  t - 开始录音并转录 (录音 " << RECORDING_SECONDS << " 秒)" << std::endl;
    std::cout << "  h - 显示帮助信息" << std::endl;
    std::cout << "  q - 退出程序" << std::endl;
    std::cout << std::endl;
    
    // 命令循环
    bool running = true;
    while (running) {
        std::cout << "请输入命令 (t/h/q): ";
        std::string command;
        std::getline(std::cin, command);
        
        // 去除首尾空格
        command.erase(0, command.find_first_not_of(" \t\n\r"));
        command.erase(command.find_last_not_of(" \t\n\r") + 1);
        
        if (command.empty()) {
            continue;
        }
        
        char cmd = std::tolower(command[0]);
        
        switch (cmd) {
            case 't': {
                // 执行录音和转录
                std::string result = RecordAndTranscribe(device);
                if (!result.empty()) {
                    std::cout << "\n转录结果:" << std::endl;
                    std::cout << "========================================" << std::endl;
                    std::cout << result << std::endl;
                    std::cout << "========================================" << std::endl;
                    
                    // 将转录结果发送给 LLM 进行对话（流式输出）
                    std::cout << "\nLLM 回复:" << std::endl;
                    std::cout << "========================================" << std::endl;
                    ChatWithLLM(result);  // 内部已经流式输出，不需要接收返回值
                    std::cout << "========================================" << std::endl;
                } else {
                    std::cout << "\n(无转录结果或录音失败)" << std::endl;
                }
                std::cout << std::endl;
                break;
            }
            
            case 'h': {
                // 显示帮助
                std::cout << "\n命令列表:" << std::endl;
                std::cout << "  t - 开始录音并转录 (录音 " << RECORDING_SECONDS << " 秒)" << std::endl;
                std::cout << "  h - 显示帮助信息" << std::endl;
                std::cout << "  q - 退出程序" << std::endl;
                std::cout << std::endl;
                break;
            }
            
            case 'q': {
                // 退出
                std::cout << "正在退出程序..." << std::endl;
                running = false;
                break;
            }
            
            default: {
                std::cout << "未知命令: " << command << std::endl;
                std::cout << "输入 'h' 查看帮助" << std::endl;
                std::cout << std::endl;
                break;
            }
        }
    }
    
    // 清理资源
    ma_device_uninit(&device);
    CleanupRecognizer();
    CleanupLLM();
    
    std::cout << "程序已退出。" << std::endl;
    return 0;
}
