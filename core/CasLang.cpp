#include "CasLang.h"
#include "CasStringOps.h"
#include "CasFSOps.h"
#include "CasNumOps.h"
#include "CasTimeOps.h"
#include <fstream>
#include <sstream>
#include <iostream>

namespace CasLang {
    CasLangModule::CasLangModule() {
        m_runner.Register(std::make_unique<CasStringOps>());
        m_runner.Register(std::make_unique<CasFSOps>());
        m_runner.Register(std::make_unique<CasNumOps>());
        m_runner.Register(std::make_unique<CasTimeOps>());
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
        auto result = m_runner.Run(code);
        
        X::Dict out;
        out->Set("success", result.success);

        // Populate logs
        X::List logList;
        for (const auto& l : m_runner.GetContext().logs) {
            X::Value v(l);
            logList->AddItem(v);
        }
        out->Set("logs", logList);

        if (result.success) {
            out->Set("data", result.output);
        } else {
            X::Dict err;
            err->Set("message", result.error);
            err->Set("line", result.errorLine);
            out->Set("error", err);
        }
        return out;
    }
}
