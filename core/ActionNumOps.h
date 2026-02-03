#pragma once
#include "ActionOps.h"
#include <algorithm>
#include <thread>
#include <chrono>

namespace Galaxy {

    class ActionNumOps : public ActionOps {
    public:
         const std::string& Namespace() const override {
            static std::string k = "num";
            return k;
        }

        X::Value Execute(const std::vector<std::string>& ns_parts,
            const std::string& command,
            std::unordered_map<std::string, X::Value>& args,
            ActionContext& ctx,
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

            if (command == "add") return X::Value(a + b);
            if (command == "sub") return X::Value(a - b);
            if (command == "mul") return X::Value(a * b);
            if (command == "div") {
                if (b == 0.0) { errs.push_back("num.div: division by zero"); return X::Value(); }
                return X::Value(a / b);
            }
            if (command == "min") return X::Value((std::min)(a, b));
            if (command == "max") return X::Value((std::max)(a, b));

            errs.push_back("num: unknown command " + command);
            return X::Value();
        }
    };
}
