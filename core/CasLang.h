#pragma once

#include "value.h"
#include "CasRunner.h"

namespace CasLang {
    class CasLangModule {

    public:

        CasLangModule();
        X::Value Run(X::Value fileName);
        X::Value Runs(X::Value code);
        
        static void RunScript(const std::string& fileName);
    };
}
