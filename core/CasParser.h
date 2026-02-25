#pragma once
#include <string>
#include <vector>
#include <optional>
#include <cctype>
#include <stdexcept>
#include <unordered_map>
#include <sstream>

#include <nlohmann/json.hpp>
#include "xlang.h"

using json = nlohmann::json;

namespace CasLang {

struct ParsedAction {
    std::vector<std::string> ns; // e.g. {"fs"}
    std::string command;         // e.g. "open"
    std::unordered_map<std::string, X::Value> args;
    size_t start = 0, end = 0;
    std::string error;
};

class CasParser {
public:
    // JSONL parser: splits input into lines, parses each as {"op":"ns.cmd",...}
    std::vector<ParsedAction> Extract(const std::string& text) {
        std::vector<ParsedAction> out;
        std::istringstream iss(text);
        std::string line;
        size_t lineStart = 0;
        
        while (std::getline(iss, line)) {
            // Trim whitespace
            size_t first = line.find_first_not_of(" \t\r\n");
            size_t last  = line.find_last_not_of(" \t\r\n");
            if (first == std::string::npos) {
                lineStart += line.size() + 1;
                continue;
            }
            std::string trimmed = line.substr(first, last - first + 1);
            
            // Skip empty lines and non-JSON lines
            if (trimmed.empty() || trimmed[0] != '{') {
                lineStart += line.size() + 1;
                continue;
            }

            ParsedAction pa;
            pa.start = lineStart;
            pa.end = lineStart + line.size();
            
            try {
                json j = json::parse(trimmed);
                if (!j.is_object()) {
                    pa.error = "Line is not a JSON object";
                    out.emplace_back(std::move(pa));
                    lineStart += line.size() + 1;
                    continue;
                }
                
                if (!j.contains("op") || !j["op"].is_string()) {
                    pa.error = "Missing or invalid 'op' field";
                    out.emplace_back(std::move(pa));
                    lineStart += line.size() + 1;
                    continue;
                }

                std::string opStr = j["op"].get<std::string>();
                
                // Split "ns.cmd" on first dot
                auto dotPos = opStr.find('.');
                if (dotPos == std::string::npos) {
                    pa.error = "E1003: op must be namespace.command: " + opStr;
                    out.emplace_back(std::move(pa));
                    lineStart += line.size() + 1;
                    continue;
                }
                
                pa.ns = { opStr.substr(0, dotPos) };
                pa.command = opStr.substr(dotPos + 1);
                
                // All non-"op" keys become args
                for (auto it = j.begin(); it != j.end(); ++it) {
                    if (it.key() == "op") continue;
                    pa.args.emplace(it.key(), toXScalar(it.value()));
                }
            }
            catch (const std::exception& e) {
                pa.error = std::string("JSON parse error: ") + e.what();
            }
            
            out.emplace_back(std::move(pa));
            lineStart += line.size() + 1;
        }
        return out;
    }

    // Convert ONLY scalars; arrays/objects are stringified JSON to keep scalar contract.
    static X::Value toXScalar(const json& v) {
        if (v.is_null())  return X::Value();
        if (v.is_boolean()) return X::Value(v.get<bool>());
        if (v.is_number_integer()) return X::Value((int64_t)v.get<long long>());
        if (v.is_number_unsigned()) return X::Value((int64_t)v.get<unsigned long long>());
        if (v.is_number_float()) return X::Value(v.get<double>());
        if (v.is_string()) return X::Value(v.get<std::string>());
        // composite -> stringify
        return X::Value(v.dump());
    }
};
}
