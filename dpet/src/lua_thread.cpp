#include "../include/lua_thread.h"

namespace Threading {

void LuaCommandQueue::push(const LuaCommand& cmd) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(cmd);
    }
    cv_.notify_one();
}

bool LuaCommandQueue::pop(LuaCommand& cmd) {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return !queue_.empty(); });
    
    if (!queue_.empty()) {
        cmd = queue_.front();
        queue_.pop();
        return true;
    }
    return false;
}

void LuaCommandQueue::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!queue_.empty()) {
        queue_.pop();
    }
}

bool LuaCommandQueue::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}

} // namespace Threading
