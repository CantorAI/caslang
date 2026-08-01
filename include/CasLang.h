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

#pragma once

#include <functional>
#include <string>

namespace CasLang {

struct HostRequest {
    std::string name_space;
    std::string operation;
    std::string arguments_json;
    std::string metadata_json;
};

struct HostResponse {
    bool success = true;
    std::string value_json = "null";
    std::string error;
};

using HostCallback = std::function<HostResponse(const HostRequest&)>;

struct RunOptions {
    HostCallback host_callback;
    std::string metadata_json;
};

// Stable public facade for embedding the CasLang runtime. Results are encoded
// as JSON so consumers do not depend on CasLang's internal value types.
class Runtime final {
public:
    // Execute a UTF-8 CasLang JSONL program and return its JSON result.
    std::string Run(const std::string& code) const;
    std::string Run(const std::string& code, const RunOptions& options) const;

    // Execute a UTF-8 .cas file and return its JSON result.
    std::string RunFile(const std::string& fileName) const;
    std::string RunFile(const std::string& fileName, const RunOptions& options) const;
};

}  // namespace CasLang
