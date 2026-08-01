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
#include <iostream>

namespace CasLang {
    class CasToolOps : public CasOps {
    public:
        const std::string& Namespace() const override {
            static std::string k = "tool";
            return k;
        }

        Cas::Value Execute(const std::vector<std::string>& ns_parts,
            const std::string& command,
            std::unordered_map<std::string, Cas::Value>& args,
            CasContext& ctx,
            std::vector<std::string>& errs) override
        {
            if (command == "call") {
                if (ctx.externalHandler) {
                    // Forward to external handler (e.g. host)
                    return ctx.externalHandler("tool", "call", args, ctx.metaData);
                } else {
                    errs.push_back("E5001 E_NO_HANDLER: No external handler for tool.call");
                    return Cas::Value();
                }
            }
            errs.push_back("tool: unknown command " + command);
            return Cas::Value();
        }
    };
}
