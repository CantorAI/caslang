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
#include <thread>
#include <chrono>

namespace CasLang {
    class CasTimeOps : public CasOps {
    public:
        const std::string& Namespace() const override {
            static std::string k = "time";
            return k;
        }

        Cas::Value Execute(const std::vector<std::string>& ns_parts,
            const std::string& command,
            std::unordered_map<std::string, Cas::Value>& args,
            CasContext& ctx,
            std::vector<std::string>& errs) override
        {
            if (command == "now") {
                auto now = std::chrono::system_clock::now();
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
                return Cas::Value((long long)ms);
            }
            if (command == "sleep") {
                auto it = args.find("ms");
                long long ms = 0;
                if (it != args.end()) {
                    if (it->second.isNumber()) ms = (long long)it->second;
                    else if (it->second.isString()) try { ms = std::stoll(it->second.asString()); } catch(...) {}
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(ms));
                return Cas::Value(true);
            }

            errs.push_back("time: unknown command " + command);
            return Cas::Value();
        }
    };
}
