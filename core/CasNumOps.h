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
#include "CasOps.h"
#include <algorithm>
#include <thread>
#include <chrono>

namespace CasLang {

    class CasNumOps : public CasOps {
    public:
         const std::string& Namespace() const override {
            static std::string k = "num";
            return k;
        }

        Cas::Value Execute(const std::vector<std::string>& ns_parts,
            const std::string& command,
            std::unordered_map<std::string, Cas::Value>& args,
            CasContext& ctx,
            std::vector<std::string>& errs) override
        {
            auto D = [&](const char* k, double def = 0.0)->double {
                auto it = args.find(k);
                if (it == args.end()) return def;
                if (it->second.isNumber()) return (double)it->second;
                if (it->second.isString()) {
                    try { return std::stod(it->second.asString()); } catch(...) { return def; }
                }
                return def;
            };

            double a = D("a");
            double b = D("b");

            if (command == "add") return Cas::Value(a + b);
            if (command == "sub") return Cas::Value(a - b);
            if (command == "mul") return Cas::Value(a * b);
            if (command == "div") {
                if (b == 0.0) { errs.push_back("num.div: division by zero"); return Cas::Value(); }
                return Cas::Value(a / b);
            }
            if (command == "min") return Cas::Value((std::min)(a, b));
            if (command == "max") return Cas::Value((std::max)(a, b));
            if (command == "range") {
                int start = (int)D("start", 0);
                int end = (int)D("end", 10);
                int step = (int)D("step", 1);
                std::string res = "[";
                for (int i = start; i < end; i += step) {
                     if (i > start) res += ",";
                     res += std::to_string(i);
                }
                res += "]";
                return Cas::Value(res);
            }

            errs.push_back("num: unknown command " + command);
            return Cas::Value();
        }
    };
}
