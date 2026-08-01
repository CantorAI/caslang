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
#include "../core/CasJsonOps.h"
#include "../core/cas_value.h"

#include <stdexcept>
#include <utility>

namespace CasLang {

namespace {

ExternalHandler MakeExternalHandler(const RunOptions& options) {
    if (!options.host_callback) return ExternalHandler();

    const HostCallback callback = options.host_callback;
    return [callback](
               const std::string& nameSpace,
               const std::string& operation,
               std::unordered_map<std::string, Cas::Value>& arguments,
               const std::string& metadata) -> Cas::Value {
        nlohmann::json encodedArguments = nlohmann::json::object();
        for (const auto& [name, value] : arguments) {
            encodedArguments[name] = CasValueToJson(value);
        }

        const HostResponse response = callback(HostRequest{
            nameSpace,
            operation,
            encodedArguments.dump(),
            metadata,
        });
        if (!response.success) {
            throw std::runtime_error(
                response.error.empty() ? "CasLang host callback failed" : response.error);
        }
        if (response.value_json.empty()) return Cas::Value();

        try {
            return JsonToCasValue(nlohmann::json::parse(response.value_json));
        } catch (const nlohmann::json::exception& error) {
            throw std::runtime_error(
                std::string("CasLang host returned invalid value_json: ") + error.what());
        }
    };
}

}  // namespace

std::string Runtime::Run(const std::string& code) const {
    return Run(code, RunOptions());
}

std::string Runtime::Run(const std::string& code, const RunOptions& options) const {
    CasLangModule runtime;
    return runtime.Runs(
        Cas::Value(code), MakeExternalHandler(options), options.metadata_json).asString();
}

std::string Runtime::RunFile(const std::string& fileName) const {
    return RunFile(fileName, RunOptions());
}

std::string Runtime::RunFile(const std::string& fileName, const RunOptions& options) const {
    CasLangModule runtime;
    return runtime.Run(
        Cas::Value(fileName), MakeExternalHandler(options), options.metadata_json).asString();
}

}  // namespace CasLang
