#include "CasLang.h"
#include "CasStringOps.h"
#include "CasFSOps.h"
#include "CasNumOps.h"
#include "CasTimeOps.h"
#include "CasDictOps.h"
#include "CasListOps.h"
#include "xlang.h"
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
             X::Dict out(X::g_pXHost->CreateDict());
             out->Set("success", false);
             X::Dict err(X::g_pXHost->CreateDict());
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

        CasRunner::Result res = runner.Run(code);
        
        // DEBUG PRINT
        std::cout << "[CasLang DEBUG] Run finished. Success: " << res.success << std::endl;
        if (!res.success) std::cout << "[CasLang DEBUG] Error: " << res.error << std::endl;

        X::Dict out(X::g_pXHost->CreateDict());
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
            X::Dict err(X::g_pXHost->CreateDict());
            err->Set("message", res.error);
            err->Set("line", res.errorLine);
            out->Set("error", err);
        }
        // Use internal JSON package to serialize to string
        X::Runtime rt;
        X::Package json(rt, "json", "");
        X::Value jsonStr = json["dumps"](out);
        return jsonStr; 
    }
}
