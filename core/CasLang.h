#pragma once
#include "xpackage.h"
#include "xlang.h"
#include "CasRunner.h"

#include "singleton.h"

namespace CasLang {
    class CasLangModule: public Singleton<CasLangModule> {

    public:
        BEGIN_PACKAGE(CasLangModule)
            APISET().AddFunc<1>("run", &CasLangModule::Run);
            APISET().AddFunc<1>("runs", &CasLangModule::Runs);
        END_PACKAGE

        CasLangModule();
        X::Value Run(X::Value fileName);
        X::Value Runs(X::Value code);
    };
}
