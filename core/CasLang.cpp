#include "CasLang.h"
#include "CasStringOps.h"
#include "CasFSOps.h"
#include "CasNumOps.h"
#include "CasTimeOps.h"
#include "CasDictOps.h"
#include "CasListOps.h"
#include "CasToolOps.h"
#include "CasSandboxOps.h"
#include "CasJsonOps.h"
#include "value.h"
#include <fstream>
#include <sstream>
#include <iostream>

namespace CasLang {
    CasLangModule::CasLangModule() {
    }

    X::Value CasLangModule::Run(X::Value valFileName) {
        std::string fileName = valFileName.asString();
        std::cout << "[CasLang] Run file: " << fileName << std::endl;
        std::ifstream t(fileName);
        if (!t.is_open()) {
             std::cerr << "[CasLang] Failed to open file: " << fileName << std::endl;
             X::Dict out;
             out->Set("success", false);
             X::Dict err;
             err->Set("message", "Failed to open file: " + fileName);
             err->Set("line", 0);
             out->Set("error", err);
             return out;
        }
        std::stringstream buffer;
        buffer << t.rdbuf();
        return Runs(X::Value(buffer.str()));
    }

    X::Value CasLangModule::Runs(X::Value valCode) {
        std::string code = valCode.asString();
        std::cout << "[CasLang] Executing code:\n" << code << std::endl;
        
        CasRunner runner;
        runner.Register(std::make_unique<CasStringOps>());
        runner.Register(std::make_unique<CasFSOps>());
        //runner.Register(std::make_unique<CasNumOps>());
        runner.Register(std::make_unique<CasTimeOps>());
        runner.Register(std::make_unique<CasDictOps>());
        runner.Register(std::make_unique<CasListOps>());
        runner.Register(std::make_unique<CasToolOps>());
        runner.Register(std::make_unique<CasSandboxOps>());
        runner.Register(std::make_unique<CasJsonOps>());

        CasRunner::Result res = runner.Run(code);
        
        // DEBUG PRINT
        std::cout << "[CasLang DEBUG] Run finished. Success: " << res.success << std::endl;
        if (!res.success) std::cout << "[CasLang DEBUG] Error: " << res.error << std::endl;

        X::Dict out;
        out->Set("success", res.success);

        // Copy logs (COMMENTED OUT TO ISOLATE BRIDGE ISSUE)
        /*
        X::List logs;
        for(const auto& l : runner.GetContext().logs) {
            X::Value v(l);
            logs->AddItem(v);
        }
        out->Set("logs", logs);
        */
        if (res.success) {
            out->Set("data", res.output);
        } else {
            X::Dict err;
            err->Set("message", res.error);
            err->Set("line", res.errorLine);
            out->Set("error", err);
        }
        
        nlohmann::json jOut = XValueToJson(X::Value(out));
        return X::Value(jOut.dump());
    }

    void CasLangModule::RunScript(const std::string& fileName) {
        std::ifstream t(fileName);
        if (!t.is_open()) {
             std::cerr << "[CasLang] Failed to open file: " << fileName << std::endl;
             return;
        }
        std::stringstream buffer;
        buffer << t.rdbuf();
        
        CasRunner runner;
        runner.Register(std::make_unique<CasStringOps>());
        runner.Register(std::make_unique<CasFSOps>());
        runner.Register(std::make_unique<CasTimeOps>());
        runner.Register(std::make_unique<CasDictOps>());
        runner.Register(std::make_unique<CasListOps>());
        runner.Register(std::make_unique<CasToolOps>());
        runner.Register(std::make_unique<CasSandboxOps>());
        runner.Register(std::make_unique<CasJsonOps>());
        
        CasRunner::Result res = runner.Run(buffer.str());
        
        std::cout << "\n--- CasLang Execution Result ---\n";
        std::cout << "Success: " << (res.success ? "true" : "false") << std::endl;
        if (!res.success) {
            std::cout << "Error: " << res.error << " at line " << res.errorLine << std::endl;
        }
        
        std::cout << "Output Data:" << std::endl;
        if (res.output.IsDict() || res.output.IsList()) {
            std::cout << XValueToJson(res.output).dump(4) << std::endl;
        } else {
            std::cout << res.output.asString() << std::endl;
        }
        
        std::cout << "Logs:" << std::endl;
        for(const auto& l : runner.GetContext().logs) {
            std::cout << l << std::endl;
        }
        std::cout << "--------------------------------\n";
    }
}
