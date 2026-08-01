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
#include <optional>
#include <cctype>
#include <stdexcept>
#include <unordered_map>
#include <sstream>

#include <nlohmann/json.hpp>
#include "cas_value.h"

using json = nlohmann::json;

namespace CasLang {

struct ParsedAction {
    std::vector<std::string> ns; // e.g. {"fs"}
    std::string command;         // e.g. "open"
    std::unordered_map<std::string, Cas::Value> args;
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
                    // Allow "caslang" header op without dot
                    if (opStr == "caslang") {
                        pa.ns = { "caslang" };
                        pa.command = "";
                    } else {
                        pa.error = "E1003: op must be namespace.command: " + opStr;
                        out.emplace_back(std::move(pa));
                        lineStart += line.size() + 1;
                        continue;
                    }
                } else {
                    pa.ns = { opStr.substr(0, dotPos) };
                    pa.command = opStr.substr(dotPos + 1);
                }
                
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
    static Cas::Value toXScalar(const json& v) {
        if (v.is_null())  return Cas::Value();
        if (v.is_boolean()) return Cas::Value(v.get<bool>());
        if (v.is_number_integer()) return Cas::Value((int64_t)v.get<long long>());
        if (v.is_number_unsigned()) return Cas::Value((int64_t)v.get<unsigned long long>());
        if (v.is_number_float()) return Cas::Value(v.get<double>());
        if (v.is_string()) return Cas::Value(v.get<std::string>());
        // composite -> stringify
        return Cas::Value(v.dump());
    }
};
}
