#pragma once
#include "CasOps.h"
#include <iostream>

namespace CasLang {
    class CasSandboxOps : public CasOps {
    public:
        const std::string& Namespace() const override {
            static std::string k = "sandbox";
            return k;
        }

        X::Value Execute(const std::vector<std::string>& ns_parts,
            const std::string& command,
            std::unordered_map<std::string, X::Value>& args,
            CasContext& ctx,
            std::vector<std::string>& errs) override
        {
            if (command == "exec") {
                std::string cmdLine = "";
                if(args.count("cmd")) cmdLine = args["cmd"].asString();
                if(cmdLine.empty()) {
                    errs.push_back("sandbox.exec: missing 'cmd'");
                    return X::Value();
                }

                std::string result;
                // Use _popen on Windows, popen on Linux
#if defined(_WIN32) || defined(WIN32)
                FILE* pipe = _popen(cmdLine.c_str(), "r");
#else
                FILE* pipe = popen(cmdLine.c_str(), "r");
#endif
                if (!pipe) {
                    errs.push_back("sandbox.exec: failed to start command");
                    return X::Value();
                }

                char buffer[128];
                while (fgets(buffer, 128, pipe) != NULL) {
                    result += buffer;
                }

#if defined(_WIN32) || defined(WIN32)
                _pclose(pipe);
#else
                pclose(pipe);
#endif
                return X::Value(result);
            }
            errs.push_back("sandbox: unknown command " + command);
            return X::Value();
        }
    };
}
