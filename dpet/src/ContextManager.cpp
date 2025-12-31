#include "../include/ContextManager.h"
#include <iostream>

namespace DesktopPet {

ContextManager::ContextManager(const std::string& system_prompt, int max_turns)
    : system_prompt_(system_prompt)
    , max_turns_(max_turns) {
    std::cout << "[ContextManager] Initialized with max_turns=" << max_turns << std::endl;
}

void ContextManager::AddMessage(const std::string& role, const std::string& content) {
    history_.emplace_back(role, content);
    TruncateIfNeeded();
}

void ContextManager::TruncateIfNeeded() {
    // Count conversation turns (pairs of user+assistant messages)
    // Each turn = 2 messages (user + assistant)
    int current_turns = static_cast<int>(history_.size()) / 2;
    
    if (current_turns > max_turns_) {
        // Remove oldest user+assistant pair (2 messages)
        int messages_to_remove = (current_turns - max_turns_) * 2;
        
        for (int i = 0; i < messages_to_remove && !history_.empty(); ++i) {
            history_.pop_front();
        }
        
        std::cout << "[ContextManager] Truncated " << messages_to_remove 
                  << " old messages, now " << history_.size() << " messages" << std::endl;
    }
}

std::string ContextManager::GetPromptString(const std::string& current_user_input) const {
    // Start with system prompt
    std::string prompt = "<|im_start|>system\n" + system_prompt_ + "<|im_end|>\n";
    
    // Add conversation history
    for (const auto& msg : history_) {
        prompt += "<|im_start|>" + msg.role + "\n" + msg.content + "<|im_end|>\n";
    }
    
    // Add current user input
    prompt += "<|im_start|>user\n" + current_user_input + "<|im_end|>\n";
    
    // Prepare for assistant response
    prompt += "<|im_start|>assistant\n";
    
    return prompt;
}

void ContextManager::Clear() {
    history_.clear();
    std::cout << "[ContextManager] History cleared" << std::endl;
}

} // namespace DesktopPet
