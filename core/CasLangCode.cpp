#include "CasLangCode.h"
#include "ActionStringOps.h"
#include "ActionFSOps.h"
#include <fstream>
#include <sstream>
#include <iostream>

namespace Galaxy {
    CasLangCode::CasLangCode() {
        m_runner.Register(std::make_unique<StringOps>());
        m_runner.Register(std::make_unique<ActionFSOps>());
    }

    X::Value CasLangCode::Run(X::Value valFileName) {
        std::string fileName = valFileName.ToString();
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
        std::string code = valCode.ToString();
        std::cout << "[CasLangCode] Executing code:\n" << code << std::endl;
        auto result = m_runner.Run(code);
        return result.output;
    }
}
