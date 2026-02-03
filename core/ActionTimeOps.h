#pragma once
#include "ActionOps.h"
#include <thread>
#include <chrono>

namespace Galaxy {
    class ActionTimeOps : public ActionOps {
    public:
        const std::string& Namespace() const override {
            static std::string k = "time";
            return k;
        }

        X::Value Execute(const std::vector<std::string>& ns_parts,
            const std::string& command,
            std::unordered_map<std::string, X::Value>& args,
            ActionContext& ctx,
            std::vector<std::string>& errs) override
        {
            if (command == "now") {
                auto now = std::chrono::system_clock::now();
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
                return X::Value((long long)ms);
            }
            if (command == "sleep") {
                auto it = args.find("ms");
                long long ms = 0;
                if (it != args.end()) {
                    if (it->second.isNumber()) ms = (long long)it->second;
                    else if (it->second.isString()) try { ms = std::stoll(it->second.asString()); } catch(...) {}
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(ms));
                return X::Value(true);
            }

            errs.push_back("time: unknown command " + command);
            return X::Value();
        }
    };
}
