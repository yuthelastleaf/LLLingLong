#pragma once

#include <string>
#include <SDL.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ChatBubble {

class Bubble {
public:
    Bubble();
    ~Bubble();
    
    // Show bubble with message
    void show(const std::string& message, int parentX, int parentY, int parentW, int parentH);
    
    // Hide bubble
    void hide();
    
    // Check if visible
    bool isVisible() const { return visible_; }
    
    // Update (for auto-hide timer)
    void update(float deltaTime);
    
    // Update bubble position when parent moves
    void updatePosition(int parentX, int parentY, int parentW, int parentH);
    
    // Render bubble (if using SDL rendering)
    void render();
    
private:
    bool visible_;
    float displayTime_;
    float maxDisplayTime_;
    std::string currentMessage_;
    
    // Track parent window position for following
    int lastParentX_;
    int lastParentY_;
    int lastParentW_;
    int lastParentH_;
    
#ifdef _WIN32
    HWND bubbleWindow_;
    
    static LRESULT CALLBACK BubbleWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void createBubbleWindow();
    void updateBubbleText(const std::string& text);
#endif
};

} // namespace ChatBubble
