#pragma once

#include <string>
#include <deque>

namespace DesktopPet {

/**
 * @struct ChatMessage
 * @brief Represents a single message in the conversation
 */
struct ChatMessage {
    std::string role;     // "user" or "assistant"
    std::string content;  // Message content
    
    ChatMessage(const std::string& r, const std::string& c) 
        : role(r), content(c) {}
};

/**
 * @class ContextManager
 * @brief Manages conversation history with sliding window mechanism
 * 
 * Features:
 * - Maintains a fixed-size sliding window of conversation history
 * - System prompt is always preserved at the beginning
 * - Automatically truncates old messages to keep context size stable
 * - Generates properly formatted prompts for LLM inference
 */
class ContextManager {
public:
    /**
     * @brief Constructor
     * @param system_prompt The system prompt that defines AI behavior
     * @param max_turns Maximum number of conversation turns to keep (default: 10)
     */
    explicit ContextManager(const std::string& system_prompt, int max_turns = 10);
    
    /**
     * @brief Add a new message to the conversation history
     * @param role Either "user" or "assistant"
     * @param content The message content
     * 
     * Automatically removes oldest messages if history exceeds max_turns
     */
    void AddMessage(const std::string& role, const std::string& content);
    
    /**
     * @brief Generate the complete prompt string for LLM
     * @param current_user_input The current user input (not yet added to history)
     * @return Formatted prompt string ready for tokenization
     * 
     * Format: <|im_start|>system...user...assistant...<|im_end|>
     */
    std::string GetPromptString(const std::string& current_user_input) const;
    
    /**
     * @brief Clear all conversation history (keeps system prompt)
     */
    void Clear();
    
    /**
     * @brief Get current number of messages in history
     */
    size_t GetHistorySize() const { return history_.size(); }
    
    /**
     * @brief Get the system prompt
     */
    const std::string& GetSystemPrompt() const { return system_prompt_; }
    
    /**
     * @brief Update the system prompt
     */
    void SetSystemPrompt(const std::string& prompt) { system_prompt_ = prompt; }
    
    /**
     * @brief Get maximum turns allowed
     */
    int GetMaxTurns() const { return max_turns_; }

private:
    std::string system_prompt_;          // Never removed
    std::deque<ChatMessage> history_;    // Sliding window of messages
    int max_turns_;                       // Maximum conversation turns to keep
    
    /**
     * @brief Internal method to truncate old messages if needed
     */
    void TruncateIfNeeded();
};

} // namespace DesktopPet
