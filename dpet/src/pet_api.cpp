#include "../include/pet_api.h"
#include <windows.h>
#include <SDL.h>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace PetAPI {

API& API::getInstance() {
    static API instance;
    return instance;
}

bool API::openProgram(const std::string& path) {
    log("Opening program: " + path);
    
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    
    // Use ShellExecute for better compatibility
    HINSTANCE result = ShellExecuteA(
        nullptr,
        "open",
        path.c_str(),
        nullptr,
        nullptr,
        SW_SHOWNORMAL
    );
    
    bool success = (INT_PTR)result > 32;
    if (success) {
        log("Program opened successfully");
    } else {
        log("Failed to open program: error code " + std::to_string((INT_PTR)result));
    }
    
    return success;
}

void API::shutdown() {
    log("Shutdown requested");
    // Set a flag or post quit event - will be handled in main loop
}

void API::setPetPosition(int x, int y) {
    state_.x = x;
    state_.y = y;
    log("Set pet position to (" + std::to_string(x) + ", " + std::to_string(y) + ")");
}

std::pair<int, int> API::getPetPosition() const {
    return {state_.x, state_.y};
}

void API::playAnimation(const std::string& animName) {
    state_.currentAnimation = animName;
    log("Playing animation: " + animName);
}

std::string API::getCurrentAnimation() const {
    return state_.currentAnimation;
}

void API::showMessage(const std::string& message) {
    log("Message: " + message);
    
    if (messageCallback_) {
        messageCallback_(message);
    } else {
        // Use chat bubble if SDL window is available
        if (window_) {
            int x, y, w, h;
            SDL_GetWindowPosition(window_, &x, &y);
            SDL_GetWindowSize(window_, &w, &h);
            bubble_.show(message, x, y, w, h);
        } else {
            // Fallback: use Windows message box with UTF-8 support
#ifdef _WIN32
            // Convert UTF-8 to UTF-16 for MessageBoxW
            int wideSize = MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, nullptr, 0);
            if (wideSize > 0) {
                std::wstring wideMessage(wideSize, 0);
                MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, &wideMessage[0], wideSize);
                MessageBoxW(nullptr, wideMessage.c_str(), L"桌面宠物消息", MB_OK | MB_ICONINFORMATION);
            }
#else
            MessageBoxA(nullptr, message.c_str(), "Pet Message", MB_OK | MB_ICONINFORMATION);
#endif
        }
    }
}

void API::setMessageCallback(MessageCallback callback) {
    messageCallback_ = std::move(callback);
}

std::string API::getTime() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

void API::log(const std::string& message) {
    std::string logEntry = "[" + getTime() + "] " + message;
    logMessages_.push_back(logEntry);
    std::cout << logEntry << std::endl;
    
    // Keep only last 100 messages
    if (logMessages_.size() > 100) {
        logMessages_.erase(logMessages_.begin());
    }
}

void API::updateBubble(float deltaTime) {
    bubble_.update(deltaTime);
    
    // Update bubble position to follow window
    if (window_ && bubble_.isVisible()) {
        int x, y, w, h;
        SDL_GetWindowPosition(window_, &x, &y);
        SDL_GetWindowSize(window_, &w, &h);
        bubble_.updatePosition(x, y, w, h);
    }
}

} // namespace PetAPI
