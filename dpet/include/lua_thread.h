#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>
#include <string>

namespace Threading {

// Lua command types
enum class LuaCommandType {
    CALL_FUNCTION,
    EXECUTE_CODE,
    SHUTDOWN
};

// Lua command structure
struct LuaCommand {
    LuaCommandType type;
    std::string functionName;
    std::string code;
    std::vector<std::string> args;
};

// Thread-safe command queue for Lua execution
class LuaCommandQueue {
public:
    void push(const LuaCommand& cmd);
    bool pop(LuaCommand& cmd);
    void clear();
    bool empty() const;
    
private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<LuaCommand> queue_;
};

} // namespace Threading
