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

#include "CasJsonOps.h"
#include "CasLang.h"

#include <iostream>
#include <string>

namespace {

void PrintUsage() {
    std::cerr << "Usage: caslang <script.cas>\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        PrintUsage();
        return 64;
    }

    CasLang::CasLangModule runtime;
    const Cas::Value encoded = runtime.Run(Cas::Value(std::string(argv[1])));
    const std::string result = encoded.asString();
    std::cout << result << '\n';

    try {
        const auto document = nlohmann::json::parse(result);
        return document.value("success", false) ? 0 : 1;
    } catch (...) {
        return 1;
    }
}
