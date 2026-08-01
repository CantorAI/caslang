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
#include <unordered_map>

#include "cas_value.h"

namespace CasLang {
    using ExternalHandler = std::function<Cas::Value(const std::string& ns, const std::string& cmd, std::unordered_map<std::string, Cas::Value>& args, const std::string& metaData)>;

    struct CasContext {
        std::unordered_map<std::string, Cas::Value> vars;
        Cas::Value _last;
        std::string metaData;
        
        // Flow control flags
        bool break_flag = false;
        bool continue_flag = false;
        bool return_flag = false;
        Cas::Value return_value;
        std::string return_to;  // "llm" (default) or "final"

        // Recursion/Loop limits could go here
        
        // Execution Logs (for LLM debug)
        std::vector<std::string> logs;
        int current_line = 0;

        // Callback for external tools (e.g. tool.call, sandbox.exec)
        ExternalHandler externalHandler;
    };

    class CasOps {
    public:
        virtual ~CasOps() {}
        virtual bool Init(CasContext& ctx) { return true; }
        // Namespace prefix for this Op set (e.g. "fs", "str")
        virtual const std::string& Namespace() const = 0;

        // Describe commands for LLM prompt generation
        struct CommandInfo {
            std::string name;
            std::string desc;
            struct Arg { std::string name; std::string type; bool required; std::string desc; std::string default_val; };
            std::vector<Arg> args;
            std::string returns;
            std::vector<std::string> examples;
        };
        virtual std::vector<CommandInfo> DescribeCommands() const { return {}; }

        // Execute a command
        virtual Cas::Value Execute(const std::vector<std::string>& ns_parts, 
                                 const std::string& command, 
                                 std::unordered_map<std::string, Cas::Value>& args, 
                                 CasContext& ctx, 
                                 std::vector<std::string>& errs) = 0;
    };
}
