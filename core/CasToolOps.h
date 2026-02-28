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

        X::Value Execute(const std::vector<std::string>& ns_parts,
            const std::string& command,
            std::unordered_map<std::string, X::Value>& args,
            CasContext& ctx,
            std::vector<std::string>& errs) override
        {
            if (command == "call") {
                if (ctx.externalHandler) {
                    // Forward to external handler (e.g. host)
                    return ctx.externalHandler("tool", "call", args, ctx.metaData);
                } else {
                    errs.push_back("E5001 E_NO_HANDLER: No external handler for tool.call");
                    return X::Value();
                }
            }
            errs.push_back("tool: unknown command " + command);
            return X::Value();
        }
    };
}
