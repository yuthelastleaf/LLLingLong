#include "../include/chat_bubble.h"
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <string>

// Static member for window procedure
static ChatBubble::Bubble* g_bubbleInstance = nullptr;
#endif

namespace ChatBubble {

Bubble::Bubble() 
    : visible_(false)
    , displayTime_(0.0f)
    , maxDisplayTime_(3.0f)
    , lastParentX_(0)
    , lastParentY_(0)
    , lastParentW_(0)
    , lastParentH_(0)
#ifdef _WIN32
    , bubbleWindow_(nullptr)
#endif
{
#ifdef _WIN32
    g_bubbleInstance = this;
    createBubbleWindow();
#endif
}

Bubble::~Bubble() {
#ifdef _WIN32
    if (bubbleWindow_) {
        DestroyWindow(bubbleWindow_);
        bubbleWindow_ = nullptr;
    }
#endif
}

void Bubble::show(const std::string& message, int parentX, int parentY, int parentW, int parentH) {
    std::cout << "[ChatBubble] show() called with message: \"" << message << "\"" << std::endl;
    std::cout << "[ChatBubble] Parent position: (" << parentX << ", " << parentY << "), size: (" << parentW << ", " << parentH << ")" << std::endl;
    
    currentMessage_ = message;
    visible_ = true;
    displayTime_ = 0.0f;
    
    // Save parent position for following
    lastParentX_ = parentX;
    lastParentY_ = parentY;
    lastParentW_ = parentW;
    lastParentH_ = parentH;
    
#ifdef _WIN32
    if (!bubbleWindow_) {
        std::cout << "[ChatBubble] ERROR: bubbleWindow_ is NULL!" << std::endl;
        return;
    }
    
    std::cout << "[ChatBubble] bubbleWindow_ = " << bubbleWindow_ << std::endl;
    
    // Convert UTF-8 to UTF-16
    int wideSize = MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, nullptr, 0);
    std::wstring wideMessage(wideSize, 0);
    MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, &wideMessage[0], wideSize);
    
    // Calculate bubble size based on text
    HDC hdc = GetDC(bubbleWindow_);
    HFONT hFont = CreateFontW(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
    HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
    
    RECT textRect = {0, 0, 0, 0};
    DrawTextW(hdc, wideMessage.c_str(), -1, &textRect, DT_CALCRECT | DT_WORDBREAK);
    
    SelectObject(hdc, oldFont);
    DeleteObject(hFont);
    ReleaseDC(bubbleWindow_, hdc);
    
    // Add padding
    int bubbleWidth = textRect.right + 40;
    int bubbleHeight = textRect.bottom + 30;
    
    // Limit size
    if (bubbleWidth < 100) bubbleWidth = 100;
    if (bubbleWidth > 400) bubbleWidth = 400;
    if (bubbleHeight < 60) bubbleHeight = 60;
    
    // Position above parent window
    int bubbleX = parentX + (parentW - bubbleWidth) / 2;
    int bubbleY = parentY - bubbleHeight - 10;
    
    // Keep on screen
    RECT screenRect;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &screenRect, 0);
    if (bubbleX < screenRect.left) bubbleX = screenRect.left;
    if (bubbleY < screenRect.top) bubbleY = screenRect.top;
    if (bubbleX + bubbleWidth > screenRect.right) 
        bubbleX = screenRect.right - bubbleWidth;
    
    // Update window
    std::cout << "[ChatBubble] Setting window position: (" << bubbleX << ", " << bubbleY << "), size: (" << bubbleWidth << ", " << bubbleHeight << ")" << std::endl;
    
    BOOL result = SetWindowPos(bubbleWindow_, HWND_TOPMOST, bubbleX, bubbleY, bubbleWidth, bubbleHeight, 
        SWP_SHOWWINDOW);
    
    if (!result) {
        std::cout << "[ChatBubble] SetWindowPos failed, error: " << GetLastError() << std::endl;
    } else {
        std::cout << "[ChatBubble] SetWindowPos succeeded" << std::endl;
    }
    
    // Force window to be visible
    ShowWindow(bubbleWindow_, SW_SHOW);
    UpdateWindow(bubbleWindow_);
    SetForegroundWindow(bubbleWindow_);
    std::cout << "[ChatBubble] ShowWindow/UpdateWindow/SetForegroundWindow called" << std::endl;
    
    updateBubbleText(message);
    InvalidateRect(bubbleWindow_, nullptr, TRUE);
    std::cout << "[ChatBubble] show() completed" << std::endl;
#endif
}

void Bubble::hide() {
    visible_ = false;
    
#ifdef _WIN32
    if (bubbleWindow_) {
        ShowWindow(bubbleWindow_, SW_HIDE);
    }
#endif
}

void Bubble::update(float deltaTime) {
    if (!visible_) return;
    
    displayTime_ += deltaTime;
    if (displayTime_ >= maxDisplayTime_) {
        hide();
    }
}

#ifdef _WIN32
void Bubble::updatePosition(int parentX, int parentY, int parentW, int parentH) {
    if (!bubbleWindow_ || !visible_) return;
    
    // Save current parent position
    lastParentX_ = parentX;
    lastParentY_ = parentY;
    lastParentW_ = parentW;
    lastParentH_ = parentH;
    
    // Get current bubble size
    RECT bubbleRect;
    GetWindowRect(bubbleWindow_, &bubbleRect);
    int bubbleWidth = bubbleRect.right - bubbleRect.left;
    int bubbleHeight = bubbleRect.bottom - bubbleRect.top;
    
    // Calculate new position above parent
    int bubbleX = parentX + (parentW - bubbleWidth) / 2;
    int bubbleY = parentY - bubbleHeight - 10;
    
    // Keep on screen
    RECT screenRect;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &screenRect, 0);
    if (bubbleX < screenRect.left) bubbleX = screenRect.left;
    if (bubbleY < screenRect.top) bubbleY = screenRect.top;
    if (bubbleX + bubbleWidth > screenRect.right) 
        bubbleX = screenRect.right - bubbleWidth;
    
    // Update position
    SetWindowPos(bubbleWindow_, HWND_TOPMOST, bubbleX, bubbleY, 0, 0, 
        SWP_NOSIZE | SWP_NOACTIVATE);
}
#endif

void Bubble::render() {
    // Rendering is handled by Windows GDI in WndProc
}

#ifdef _WIN32
void Bubble::createBubbleWindow() {
    std::cout << "[ChatBubble] Creating bubble window..." << std::endl;
    
    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = BubbleWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = L"ChatBubbleClass";
    
    if (!RegisterClassExW(&wc)) {
        DWORD error = GetLastError();
        if (error != ERROR_CLASS_ALREADY_EXISTS) {
            std::cout << "[ChatBubble] Failed to register window class, error: " << error << std::endl;
            return;
        }
    }
    
    // Create window
    bubbleWindow_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        L"ChatBubbleClass",
        L"",
        WS_POPUP,
        0, 0, 200, 80,
        nullptr, nullptr, GetModuleHandle(nullptr), nullptr
    );
    
    if (bubbleWindow_) {
        std::cout << "[ChatBubble] Window created successfully: " << bubbleWindow_ << std::endl;
        // Make window 90% opaque
        SetLayeredWindowAttributes(bubbleWindow_, 0, 230, LWA_ALPHA);
    } else {
        std::cout << "[ChatBubble] Failed to create window, error: " << GetLastError() << std::endl;
    }
}

void Bubble::updateBubbleText(const std::string& text) {
    if (!bubbleWindow_) return;
    
    // Convert UTF-8 to UTF-16
    int wideSize = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    std::wstring wideText(wideSize, 0);
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &wideText[0], wideSize);
    
    // Store text as window property
    SetWindowTextW(bubbleWindow_, wideText.c_str());
}

LRESULT CALLBACK Bubble::BubbleWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // Get window size
            RECT rect;
            GetClientRect(hwnd, &rect);
            
            // Fill background with rounded rectangle
            HBRUSH bgBrush = CreateSolidBrush(RGB(255, 255, 230)); // Light yellow
            HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, bgBrush);
            HPEN pen = CreatePen(PS_SOLID, 2, RGB(100, 100, 100));
            HPEN oldPen = (HPEN)SelectObject(hdc, pen);
            
            RoundRect(hdc, 2, 2, rect.right - 2, rect.bottom - 2, 15, 15);
            
            SelectObject(hdc, oldPen);
            SelectObject(hdc, oldBrush);
            DeleteObject(pen);
            DeleteObject(bgBrush);
            
            // Draw text
            wchar_t text[512];
            GetWindowTextW(hwnd, text, 512);
            
            HFONT hFont = CreateFontW(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
            HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
            
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(0, 0, 0));
            
            RECT textRect = rect;
            textRect.left += 15;
            textRect.right -= 15;
            textRect.top += 10;
            textRect.bottom -= 10;
            
            DrawTextW(hdc, text, -1, &textRect, DT_WORDBREAK | DT_CENTER);
            
            SelectObject(hdc, oldFont);
            DeleteObject(hFont);
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_LBUTTONDOWN:
            // Click to dismiss
            if (g_bubbleInstance) {
                g_bubbleInstance->hide();
            }
            return 0;
            
        case WM_DESTROY:
            return 0;
    }
    
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
#endif

} // namespace ChatBubble
