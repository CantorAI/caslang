/*
Copyright (C) 2026 CantorAI Inc.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

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
#include "cas_value.h"
#include <fstream>
#include <sstream>
#include <iostream>

namespace CasLang {
    CasLangModule::CasLangModule() {
    }

    Cas::Value CasLangModule::Run(Cas::Value valFileName) {
        std::string fileName = valFileName.asString();
        std::cout << "[CasLang] Run file: " << fileName << std::endl;
        std::ifstream t(fileName);
        if (!t.is_open()) {
             std::cerr << "[CasLang] Failed to open file: " << fileName << std::endl;
             Cas::Dict out;
             out->Set("success", false);
             Cas::Dict err;
             err->Set("message", "Failed to open file: " + fileName);
             err->Set("line", 0);
             out->Set("error", err);
             return Cas::Value(CasValueToJson(Cas::Value(out)).dump());
        }
        std::stringstream buffer;
        buffer << t.rdbuf();
        return Runs(Cas::Value(buffer.str()));
    }

    Cas::Value CasLangModule::Runs(Cas::Value valCode) {
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

        Cas::Dict out;
        out->Set("success", res.success);

        // Copy logs (COMMENTED OUT TO ISOLATE BRIDGE ISSUE)
        /*
        Cas::List logs;
        for(const auto& l : runner.GetContext().logs) {
            Cas::Value v(l);
            logs->AddItem(v);
        }
        out->Set("logs", logs);
        */
        if (res.success) {
            out->Set("data", res.output);
        } else {
            Cas::Dict err;
            err->Set("message", res.error);
            err->Set("line", res.errorLine);
            out->Set("error", err);
        }
        
        nlohmann::json jOut = CasValueToJson(Cas::Value(out));
        return Cas::Value(jOut.dump());
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
            std::cout << CasValueToJson(res.output).dump(4) << std::endl;
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
