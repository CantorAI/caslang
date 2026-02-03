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
             return X::Value(false);
        }
        std::stringstream buffer;
        buffer << t.rdbuf();
        return Runs(X::Value(buffer.str()));
    }

    X::Value CasLangModule::Runs(X::Value valCode) {
        std::string code = valCode.asString();
        std::cout << "[CasLang] Executing code:\n" << code << std::endl;
        auto result = m_runner.Run(code);
        return result.output;
    }
}
