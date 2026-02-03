#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "ActionOps.h"

namespace Galaxy {
    class ActionRunner {
        std::unordered_map<std::string, std::unique_ptr<ActionOps>> m_ops;
        ActionContext m_ctx;

        // Helpers
        bool ParseLine(const std::string& line, std::string& ns, std::string& cmd, std::unordered_map<std::string, X::Value>& args);
        void LogError(const std::string& msg);
        
        // Scan for matching end block
        size_t FindBlockEnd(const std::vector<std::string>& lines, size_t startLine, const std::string& blockType);
        size_t FindElseOrEndif(const std::vector<std::string>& lines, size_t startLine);

    public:
        ActionRunner();
        ~ActionRunner();

        void Register(std::unique_ptr<ActionOps> op);

        struct Result {
            bool success;
            std::string error; // Last error
            X::Value output;   // Final output
        };

        Result Run(const std::string& script);
    };
}
