#pragma once
#include "CasOps.h"
#include <iostream>
#include <fstream>
#include <cstdio>

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

                // Detect multiline commands: if the command contains real
                // newlines, write to a temp file and execute that instead.
                // This is necessary because _popen/cmd.exe on Windows cannot
                // handle multiline arguments in python -c "..." properly.
                std::string actualCmd = cmdLine;
                std::string tempPath;
                bool useTempFile = false;

                if (cmdLine.find('\n') != std::string::npos) {
                    // Extract the script body from "python -c \"...\""
                    // or just treat the whole thing as a script
                    std::string scriptBody;
                    
                    // Check for pattern: python -c "..."
                    auto pyPos = cmdLine.find("python");
                    auto cPos = cmdLine.find("-c");
                    if (pyPos != std::string::npos && cPos != std::string::npos && cPos > pyPos) {
                        // Extract everything after -c, stripping surrounding quotes
                        size_t scriptStart = cPos + 2;
                        while (scriptStart < cmdLine.size() && (cmdLine[scriptStart] == ' ' || cmdLine[scriptStart] == '"'))
                            scriptStart++;
                        size_t scriptEnd = cmdLine.size();
                        while (scriptEnd > scriptStart && (cmdLine[scriptEnd - 1] == '"' || cmdLine[scriptEnd - 1] == ' '))
                            scriptEnd--;
                        scriptBody = cmdLine.substr(scriptStart, scriptEnd - scriptStart);
                    } else {
                        scriptBody = cmdLine;
                    }

                    // Write to temp file
#if defined(_WIN32) || defined(WIN32)
                    char tmpDir[MAX_PATH];
                    GetTempPathA(MAX_PATH, tmpDir);
                    tempPath = std::string(tmpDir) + "caslang_sandbox.py";
#else
                    tempPath = "/tmp/caslang_sandbox.py";
#endif
                    {
                        std::ofstream ofs(tempPath);
                        if (!ofs.is_open()) {
                            errs.push_back("sandbox.exec: failed to create temp file: " + tempPath);
                            return X::Value();
                        }
                        ofs << scriptBody;
                        ofs.close();
                    }
                    actualCmd = "python \"" + tempPath + "\"";
                    useTempFile = true;
                }

                std::string result;
                // Redirect stderr to stdout so Python errors are captured
                std::string execCmd = actualCmd + " 2>&1";
#if defined(_WIN32) || defined(WIN32)
                FILE* pipe = _popen(execCmd.c_str(), "r");
#else
                FILE* pipe = popen(execCmd.c_str(), "r");
#endif
                if (!pipe) {
                    errs.push_back("sandbox.exec: failed to start command");
                    if (useTempFile) std::remove(tempPath.c_str());
                    return X::Value();
                }

                char buffer[128];
                while (fgets(buffer, 128, pipe) != NULL) {
                    result += buffer;
                }

#if defined(_WIN32) || defined(WIN32)
                int exitCode = _pclose(pipe);
#else
                int exitCode = pclose(pipe);
#endif
                // Clean up temp file
                if (useTempFile) std::remove(tempPath.c_str());

                if (exitCode != 0) {
                    errs.push_back("sandbox.exec: command failed (exit " + std::to_string(exitCode) + "): " + result);
                    return X::Value(result);
                }

                return X::Value(result);
            }
            errs.push_back("sandbox: unknown command " + command);
            return X::Value();
        }
    };
}
