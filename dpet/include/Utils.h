#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <string>
#include <optional>

namespace DesktopPet {

// Thread-safe queue template for cross-thread communication
template<typename T>
class ThreadSafeQueue {
public:
    ThreadSafeQueue() = default;
    ~ThreadSafeQueue() = default;
    
    // Disable copy
    ThreadSafeQueue(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;
    
    /**
     * @brief Push an item to the queue
     */
    void push(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(item);
        cv_.notify_one();
    }
    
    void push(T&& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(item));
        cv_.notify_one();
    }
    
    /**
     * @brief Pop an item from the queue (blocking)
     * @return The item, or std::nullopt if queue is being destroyed
     */
    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !queue_.empty() || shutdown_; });
        
        if (shutdown_ && queue_.empty()) {
            return std::nullopt;
        }
        
        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }
    
    /**
     * @brief Try to pop an item without blocking
     */
    std::optional<T> tryPop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }
        
        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }
    
    /**
     * @brief Check if queue is empty
     */
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }
    
    /**
     * @brief Get queue size
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
    /**
     * @brief Clear the queue
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::queue<T> empty;
        std::swap(queue_, empty);
    }
    
    /**
     * @brief Signal shutdown to unblock waiting threads
     */
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
        cv_.notify_all();
    }
    
private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool shutdown_ = false;
};

// Event types for cross-thread communication
enum class EventType {
    AUDIO_INPUT,    // Audio input received from microphone
    AI_THINK,       // Trigger AI to think/respond
    EXEC_LUA,       // Execute Lua script
    UI_UPDATE,      // Update UI (e.g., change expression)
    SHOW_BUBBLE,    // Show chat bubble with message
    SHUTDOWN        // Shutdown signal
};

// Application event structure
struct AppEvent {
    EventType type;
    std::string payload;
    
    AppEvent() : type(EventType::UI_UPDATE), payload("") {}
    AppEvent(EventType t, const std::string& p) : type(t), payload(p) {}
    AppEvent(EventType t, std::string&& p) : type(t), payload(std::move(p)) {}
};

} // namespace DesktopPet
