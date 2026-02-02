#pragma once
#include "xpackage.h"
#include "xlang.h"
#include "ActionRunner.h"

#include "singleton.h"

namespace Galaxy {
    class CasLangCode: public Singleton<CasLangCode> {
        ActionRunner m_runner;
    public:
        BEGIN_PACKAGE(CasLangCode)
            APISET().AddFunc<1>("run", &CasLangCode::Run);
            APISET().AddFunc<1>("runs", &CasLangCode::Runs);
        END_PACKAGE

        CasLangCode();
        X::Value Run(X::Value fileName);
        X::Value Runs(X::Value code);
    };
}
