#pragma once
#include "CasOps.h"
#include <algorithm>
#include <regex>

namespace CasLang {
    class CasStringOps : public CasOps {
    public:
        const std::string& Namespace() const override {
            static std::string k = "str";
            return k;
        }

        X::Value Execute(const std::vector<std::string>& ns_parts,
            const std::string& command,
            std::unordered_map<std::string, X::Value>& args,
            CasContext& ctx,
            std::vector<std::string>& errs) override
        {
            auto S = [&](const char* k, const std::string& def = "")->std::string {
                auto it = args.find(k);
                return (it != args.end()) ? it->second.asString() : def;
            };
            
            std::string s = S("s");
            
            if (command == "len") {
                return X::Value((long long)s.size());
            }
            if (command == "upper") {
                std::string out = s;
                std::transform(out.begin(), out.end(), out.begin(), ::toupper);
                return X::Value(out);
            }
            if (command == "lower") {
                std::string out = s;
                std::transform(out.begin(), out.end(), out.begin(), ::tolower);
                return X::Value(out);
            }
            if (command == "trim") {
                size_t first = s.find_first_not_of(" \t\r\n");
                if (first == std::string::npos) return X::Value("");
                size_t last = s.find_last_not_of(" \t\r\n");
                return X::Value(s.substr(first, last - first + 1));
            }
            if (command == "contains") {
                std::string sub = S("sub");
                return X::Value(s.find(sub) != std::string::npos);
            }
            if (command == "find") {
                std::string sub = S("sub");
                auto pos = s.find(sub);
                return X::Value((long long)((pos == std::string::npos) ? -1 : pos));
            }
            if (command == "replace") {
                std::string oldS = S("old");
                std::string newS = S("new");
                if (oldS.empty()) return X::Value(s);
                std::string out = s;
                size_t pos = 0;
                while((pos = out.find(oldS, pos)) != std::string::npos) {
                    out.replace(pos, oldS.length(), newS);
                    pos += newS.length();
                }
                return X::Value(out);
            }
            if (command == "slice") {
                long long start = 0;
                long long end = (long long)s.size();
                if (args.count("start")) start = args["start"].isNumber() ? (long long)args["start"] : std::stoll(args["start"].asString());
                if (args.count("end")) end = args["end"].isNumber() ? (long long)args["end"] : std::stoll(args["end"].asString());
                
                if (start < 0) start += s.size();
                if (end < 0) end += s.size();
                if (start < 0) start = 0;
                if (end > (long long)s.size()) end = s.size();
                
                if (start >= end) return X::Value("");
                return X::Value(s.substr(start, end - start));
            }

            errs.push_back("str: unknown command " + command);
            return X::Value();
        }
    };
}
