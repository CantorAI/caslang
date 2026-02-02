#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include "xlang.h"

// Persistent context shared across commands
struct ActionContext {
    std::unordered_map<std::string, X::Value> vars;

    void set(const std::string& k, const X::Value& v) { vars[k] = v; }
    bool has(const std::string& k) const { return vars.find(k) != vars.end(); }
    X::Value get(const std::string& k) const {
        auto it = vars.find(k);
        return it == vars.end() ? X::Value() : it->second;
    }
};

class ActionOps {
public:
    struct Arg {
        std::string name;        // "path"
        std::string type;        // "string" | "number" | "bool"
        bool        required;    // required?
        std::string desc;        // 1-line
        std::string default_val; // textual default (e.g. "\"*\"" or "false")
    };
    struct CommandInfo {
        std::string name;                   // "read_file"
        std::string brief;                  // one-liner
        std::vector<Arg> args;              // arguments
        std::string returns;                // return description
        std::vector<std::string> examples;  // sample commands
    };

    virtual ~ActionOps() = default;

    // Root namespace, e.g. "fs"
    virtual const std::string& Namespace() const = 0;

    // Execute command; ns_parts[0] == Namespace()
    virtual X::Value Execute(const std::vector<std::string>& ns_parts,
        const std::string& command,
        std::unordered_map<std::string, X::Value>& args,
        ActionContext& ctx,
        std::vector<std::string>& errs) = 0;

    // Describe all commands this Ops provides (for LLM/tooling/docs)
    virtual std::vector<CommandInfo> DescribeCommands() const = 0;

    // Build an LLM usage prompt from the metadata
    virtual std::string BuildLLMUsagePrompt() const {
        std::ostringstream os;
        os << "Emit commands as: #" << Namespace()
            << ".<command>{\"key\":value,...}\n"
            << "Rules: JSON only (double quotes, no trailing commas). "
            << "Use ${var_name} inside string values for interpolation. "
            << "Optional keys: \"as\":\"var\" to capture output, \"return\":true to choose final output.\n\n";

        for (const auto& c : DescribeCommands()) {
            os << "? #" << Namespace() << "." << c.name << "    " << c.brief << "\n";
            if (!c.args.empty()) {
                os << "  Args:\n";
                for (const auto& a : c.args) {
                    os << "    - " << a.name << " : " << a.type
                        << (a.required ? " (required)" : " (optional)")
                        << (a.default_val.empty() ? "" : ", default=" + a.default_val)
                        << "    " << a.desc << "\n";
                }
            }
            if (!c.returns.empty()) os << "  Returns: " << c.returns << "\n";
            if (!c.examples.empty()) {
                os << "  Example" << (c.examples.size() > 1 ? "s" : "") << ":\n";
                for (const auto& ex : c.examples) os << "    " << ex << "\n";
            }
            os << "\n";
        }
        return os.str();
    }
};
