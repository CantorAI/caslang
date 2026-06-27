#pragma once

#include "xlang_value.h"
#include "CasRunner.h"

namespace CasLang {
    class CasLangModule {

    public:

        CasLangModule();
        Cas::Value Run(Cas::Value fileName);
        Cas::Value Runs(Cas::Value code);
        
        static void RunScript(const std::string& fileName);
    };
}
