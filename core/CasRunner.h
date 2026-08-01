/*
Copyright (C) 2026 CantorAI Inc.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

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
        bool ParseLine(const std::string& line, std::string& ns, std::string& cmd, std::unordered_map<std::string, Cas::Value>& args, std::string& outErr);
        void LogError(const std::string& msg);
        
        // Scan for matching end block
        size_t FindBlockEnd(const std::vector<std::string>& lines, size_t startLine, const std::string& blockType);
        size_t FindElseOrEndif(const std::vector<std::string>& lines, size_t startLine);

    public:
        CasRunner();
        ~CasRunner();

        void Register(std::unique_ptr<CasOps> op);
        
        // using ExternalHandler = std::function<Cas::Value(const std::string& ns, const std::string& cmd, std::unordered_map<std::string, Cas::Value>& args)>;
        void SetExternalHandler(ExternalHandler handler) { m_externalHandler = handler; }
        void SetMetaData(const std::string& md) { m_ctx.metaData = md; }

        struct Result {
            bool success;
            std::string error; // Last error
            int errorLine = -1;
            Cas::Value output;   // Final output
            std::string return_to;  // "llm" (default) or "final"
        };

        Result Run(const std::string& script);
        
        const CasContext& GetContext() const { return m_ctx; }

    private:
        ExternalHandler m_externalHandler;
        Result ValidateScript(const std::vector<std::string>& lines);
    };
}
