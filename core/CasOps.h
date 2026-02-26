#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "xpackage.h"
#include "xlang.h"

namespace CasLang {
    using ExternalHandler = std::function<X::Value(const std::string& ns, const std::string& cmd, std::unordered_map<std::string, X::Value>& args, const std::string& metaData)>;

    struct CasContext {
        std::unordered_map<std::string, X::Value> vars;
        X::Value _last;
        std::string metaData;
        
        // Flow control flags
        bool break_flag = false;
        bool continue_flag = false;
        bool return_flag = false;
        X::Value return_value;

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
        virtual X::Value Execute(const std::vector<std::string>& ns_parts, 
                                 const std::string& command, 
                                 std::unordered_map<std::string, X::Value>& args, 
                                 CasContext& ctx, 
                                 std::vector<std::string>& errs) = 0;
    };
}
