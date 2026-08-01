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

#include <CasLang.h>

#include "../core/CasLangModule.h"
#include "../core/cas_value.h"

namespace CasLang {

std::string Runtime::Run(const std::string& code) const {
    CasLangModule runtime;
    return runtime.Runs(Cas::Value(code)).asString();
}

std::string Runtime::RunFile(const std::string& fileName) const {
    CasLangModule runtime;
    return runtime.Run(Cas::Value(fileName)).asString();
}

}  // namespace CasLang
