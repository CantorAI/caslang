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

        X::Value Execute(const std::vector<std::string>& ns_parts,
            const std::string& command,
            std::unordered_map<std::string, X::Value>& args,
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

            if (command == "add") return X::Value(a + b);
            if (command == "sub") return X::Value(a - b);
            if (command == "mul") return X::Value(a * b);
            if (command == "div") {
                if (b == 0.0) { errs.push_back("num.div: division by zero"); return X::Value(); }
                return X::Value(a / b);
            }
            if (command == "min") return X::Value((std::min)(a, b));
            if (command == "max") return X::Value((std::max)(a, b));
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
                return X::Value(res);
            }

            errs.push_back("num: unknown command " + command);
            return X::Value();
        }
    };
}
