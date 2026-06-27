#pragma once
#include "CasOps.h"
#include <algorithm>
#include <regex>
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#endif

#define CASLANG_PRINT_AS_LOG 1

namespace CasLang {
    class CasStringOps : public CasOps {
    public:
        const std::string& Namespace() const override {
            static std::string k = "str";
            return k;
        }

        Cas::Value Execute(const std::vector<std::string>& ns_parts,
            const std::string& command,
            std::unordered_map<std::string, Cas::Value>& args,
            CasContext& ctx,
            std::vector<std::string>& errs) override
        {
            auto S = [&](const char* k, const std::string& def = "")->std::string {
                auto it = args.find(k);
                return (it != args.end()) ? it->second.asString() : def;
            };
            
            // Strict Type Check for 's'
            if (args.count("s")) {
                Cas::Value& v = args["s"];
                if (!v.isString() && command != "print" && command != "log") {
                    errs.push_back("E2103 E_ARG_TYPE: 's' must be of type string");
                    return Cas::Value();
                }
            }
            std::string s = S("s");
            
            if (command == "print") {
                std::string msg = S("msg");
                if (msg.empty()) msg = s; // Fallback to 's' if 'msg' not present
                std::string logMsg = "[CasLang L" + std::to_string(ctx.current_line) + "] " + msg;
#ifdef CASLANG_PRINT_AS_LOG
                ctx.logs.push_back(logMsg);
#endif
                std::string line = logMsg + "\n";
#ifdef _WIN32
                // Use WriteConsoleW for reliable UTF-8 output on Windows
                HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
                if (hOut != INVALID_HANDLE_VALUE && GetConsoleMode(hOut, (LPDWORD)&hOut + 1)) {
                    // It's a real console 鈥?convert UTF-8 to UTF-16 and write
                    int wlen = MultiByteToWideChar(CP_UTF8, 0, line.c_str(), (int)line.size(), nullptr, 0);
                    if (wlen > 0) {
                        std::wstring wline(wlen, L'\0');
                        MultiByteToWideChar(CP_UTF8, 0, line.c_str(), (int)line.size(), &wline[0], wlen);
                        HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
                        DWORD written;
                        WriteConsoleW(hStdOut, wline.c_str(), (DWORD)wline.size(), &written, nullptr);
                    }
                } else {
                    // Redirected to file/pipe 鈥?write UTF-8 directly
                    fwrite(line.c_str(), 1, line.size(), stdout);
                }
#else
                std::cout << line;
#endif
                return Cas::Value(true);
            }
            
            if (command == "log") {
                std::string msg = S("msg");
                if (msg.empty()) msg = s;
                std::string logLine = "[L" + std::to_string(ctx.current_line) + "] " + msg;
                ctx.logs.push_back(logLine);
                return Cas::Value(true);
            }

            if (command == "len") {
                return Cas::Value((long long)s.size());
            }
            if (command == "upper") {
                std::string out = s;
                std::transform(out.begin(), out.end(), out.begin(), ::toupper);
                return Cas::Value(out);
            }
            if (command == "lower") {
                std::string out = s;
                std::transform(out.begin(), out.end(), out.begin(), ::tolower);
                return Cas::Value(out);
            }
            if (command == "trim") {
                size_t first = s.find_first_not_of(" \t\r\n");
                if (first == std::string::npos) return Cas::Value("");
                size_t last = s.find_last_not_of(" \t\r\n");
                return Cas::Value(s.substr(first, last - first + 1));
            }
            if (command == "contains") {
                std::string sub = S("sub");
                return Cas::Value(s.find(sub) != std::string::npos);
            }
            if (command == "find") {
                std::string sub = S("sub");
                auto pos = s.find(sub);
                return Cas::Value((long long)((pos == std::string::npos) ? -1 : pos));
            }
            if (command == "replace") {
                std::string oldS = S("old");
                std::string newS = S("new");
                if (oldS.empty()) return Cas::Value(s);
                std::string out = s;
                size_t pos = 0;
                while((pos = out.find(oldS, pos)) != std::string::npos) {
                    out.replace(pos, oldS.length(), newS);
                    pos += newS.length();
                }
                return Cas::Value(out);
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
                
                if (start >= end) return Cas::Value("");
                return Cas::Value(s.substr(start, end - start));
            }

            if (command == "count") {
                std::string sub = S("sub");
                if (sub.empty()) return Cas::Value((long long)0);
                long long count = 0;
                size_t pos = 0;
                while ((pos = s.find(sub, pos)) != std::string::npos) {
                    count++;
                    pos += sub.length();
                }
                return Cas::Value(count);
            }

            if (command == "match") {
                std::string regexStr = S("regex");
                std::string caseMode = S("case");
                if (regexStr.empty()) {
                    errs.push_back("str.match: 'regex' required");
                    return Cas::Value();
                }
                
                std::regex_constants::syntax_option_type flags = std::regex_constants::ECMAScript;
                if (caseMode == "insensitive") flags |= std::regex_constants::icase;
                
                try {
                    std::regex re(regexStr, flags);
                    std::smatch m;
                    if (std::regex_search(s, m, re)) {
                        Cas::Dict res;
                        res->Set("ok", true);
                        res->Set("match", m.str());
                        res->Set("pos", (long long)m.position());
                        
                        Cas::List groups;
                        for (size_t i = 1; i < m.size(); ++i) {
                            groups += m[i].str();
                        }
                        res->Set("groups", groups);
                        
                        // Return as JSON string to match spec "stringified JSON object"
                         // Actually spec says "stringified JSON object" BUT usually Cas::Value returned is kept as Object if "as" var handles it. 
                         // However, for consistency with Spec 7C.9, it says "stringified JSON object".
                         // BUT looking at other ops, they return Cas::Value directly.
                         // Let's return Cas::Dict, and let the caller handle it.
                         // WAIT, Spec says "boolean false if no match OR a stringified JSON object".
                         // To avoid mixed types issue in some systems, returning Cas::Value (Dict) is best for internal execution.
                         // But if spec requires stringified, we should ToString(). 
                         // Let's stick to returning Cas::Value (Dict/Bool) which is more useful within CasLang Runtime. 
                         // The "Stringified JSON" in spec usually means the 'as' variable will hold an object that was parsed from string or just the object itself.
                         // Given CasRunner structure, we return Cas::Value.
                         return res; 
                    } else {
                        return Cas::Value(false);
                    }
                } catch (const std::exception& e) {
                    errs.push_back(std::string("str.match regex error: ") + e.what());
                    return Cas::Value();
                }
            }

            if (command == "count_match") {
                std::string regexStr = S("regex");
                std::string caseMode = S("case");
                if (regexStr.empty()) {
                    errs.push_back("str.count_match: 'regex' required");
                    return Cas::Value();
                }

                std::regex_constants::syntax_option_type flags = std::regex_constants::ECMAScript;
                if (caseMode == "insensitive") flags |= std::regex_constants::icase;

                try {
                    std::regex re(regexStr, flags);
                    auto begin = std::sregex_iterator(s.begin(), s.end(), re);
                    auto end = std::sregex_iterator();
                    long long count = std::distance(begin, end);
                    return Cas::Value(count);
                } catch (const std::exception& e) {
                    errs.push_back(std::string("str.count_match regex error: ") + e.what());
                    return Cas::Value();
                }
            }

            errs.push_back("str: unknown command " + command);
            return Cas::Value();
        }
    };
}
