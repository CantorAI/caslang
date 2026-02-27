#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include "CasOps.h"

namespace CasLang {
    class CasRunner {
        std::unordered_map<std::string, std::unique_ptr<CasOps>> m_ops;
        CasContext m_ctx;

        // Helpers
        bool ParseLine(const std::string& line, std::string& ns, std::string& cmd, std::unordered_map<std::string, X::Value>& args, std::string& outErr);
        void LogError(const std::string& msg);
        
        // Scan for matching end block
        size_t FindBlockEnd(const std::vector<std::string>& lines, size_t startLine, const std::string& blockType);
        size_t FindElseOrEndif(const std::vector<std::string>& lines, size_t startLine);

    public:
        CasRunner();
        ~CasRunner();

        void Register(std::unique_ptr<CasOps> op);
        
        // using ExternalHandler = std::function<X::Value(const std::string& ns, const std::string& cmd, std::unordered_map<std::string, X::Value>& args)>;
        void SetExternalHandler(ExternalHandler handler) { m_externalHandler = handler; }
        void SetMetaData(const std::string& md) { m_ctx.metaData = md; }

        struct Result {
            bool success;
            std::string error; // Last error
            int errorLine = -1;
            X::Value output;   // Final output
            std::string return_to;  // "llm" (default) or "final"
        };

        Result Run(const std::string& script);
        
        const CasContext& GetContext() const { return m_ctx; }

    private:
        ExternalHandler m_externalHandler;
        Result ValidateScript(const std::vector<std::string>& lines);
    };
}
