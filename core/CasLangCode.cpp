#include "CasLangCode.h"
#include "ActionStringOps.h"
#include "ActionFSOps.h"
#include "ActionNumOps.h"
#include "ActionTimeOps.h"
#include <fstream>
#include <sstream>
#include <iostream>

namespace Galaxy {
    CasLangCode::CasLangCode() {
        m_runner.Register(std::make_unique<StringOps>());
        m_runner.Register(std::make_unique<ActionFSOps>());
        m_runner.Register(std::make_unique<ActionNumOps>());
        m_runner.Register(std::make_unique<ActionTimeOps>());
    }

    X::Value CasLangCode::Run(X::Value valFileName) {
        std::string fileName = valFileName.asString();
        std::cout << "[CasLangCode] Run file: " << fileName << std::endl;
        std::ifstream t(fileName);
        if (!t.is_open()) {
             std::cerr << "[CasLangCode] Failed to open file: " << fileName << std::endl;
             return X::Value(false);
        }
        std::stringstream buffer;
        buffer << t.rdbuf();
        return Runs(X::Value(buffer.str()));
    }

    X::Value CasLangCode::Runs(X::Value valCode) {
        std::string code = valCode.asString();
        std::cout << "[CasLangCode] Executing code:\n" << code << std::endl;
        auto result = m_runner.Run(code);
        return result.output;
    }
}
