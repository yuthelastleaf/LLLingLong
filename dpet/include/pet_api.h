#pragma once

#include <string>
#include <functional>
#include <vector>
#include "chat_bubble.h"

// Forward declaration
struct SDL_Window;

// Desktop Pet API - C++ interfaces for Lua scripting
namespace PetAPI {

// Message callback type
using MessageCallback = std::function<void(const std::string&)>;

// Pet state
struct PetState {
    int x = 0;
    int y = 0;
    int windowWidth = 500;
    int windowHeight = 500;
    std::string currentAnimation = "idle";
    bool isDragging = false;
};

// API class - singleton for easy access from Lua
class API {
public:
    static API& getInstance();
    
    // System operations
    bool openProgram(const std::string& path);
    void shutdown();
    
    // Pet control
    void setPetPosition(int x, int y);
    std::pair<int, int> getPetPosition() const;
    void playAnimation(const std::string& animName);
    std::string getCurrentAnimation() const;
    
    // Communication
    void showMessage(const std::string& message);
    void setMessageCallback(MessageCallback callback);
    
    // Utility
    std::string getTime() const;
    void log(const std::string& message);
    
    // Internal state access (for main.cpp)
    PetState& getState() { return state_; }
    const PetState& getState() const { return state_; }
    
    // Bubble and window management
    void setSDLWindow(SDL_Window* window) { window_ = window; }
    void updateBubble(float deltaTime);
    void updateBubblePosition();
    
private:
    API() = default;
    PetState state_;
    MessageCallback messageCallback_;
    std::vector<std::string> logMessages_;
    SDL_Window* window_ = nullptr;
    ChatBubble::Bubble bubble_;
};

} // namespace PetAPI
