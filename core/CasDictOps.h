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
#include "CasOps.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace CasLang {
    class CasDictOps : public CasOps {
        std::string m_ns = "dict";
    public:
        using CasOps::CasOps;
        
        const std::string& Namespace() const override { return m_ns; }

        Cas::Value Execute(const std::vector<std::string>& ns_parts, 
                         const std::string& command, 
                         std::unordered_map<std::string, Cas::Value>& args, 
                         CasContext& ctx, 
                         std::vector<std::string>& errs) override {
            
            if (command == "get") {
                if (args.find("dict") == args.end() || args.find("key") == args.end()) {
                    errs.push_back("dict.get requires 'dict' and 'key' arguments");
                    return Cas::Value();
                }
                
                Cas::Value dVal = args["dict"];
                Cas::Value kVal = args["key"];
                if (!dVal.IsDict()) {
                    errs.push_back("dict.get: 'dict' argument is not a dictionary");
                    return Cas::Value();
                }
                
                Cas::Dict d(dVal);
                
                if (d->Has(kVal)) {
                     // Check if there is a Get(Cas::Value) 
                     // d->Get(kVal) returns Cas::Value
                     return d->Get(kVal);
                } else {
                    return Cas::Value(); // null
                }
            }
            else if (command == "set") {
                if (args.find("dict") == args.end() || args.find("key") == args.end() || args.find("value") == args.end()) {
                     errs.push_back("dict.set requires 'dict', 'key', and 'value' arguments");
                     return Cas::Value();
                }
                
                Cas::Value dVal = args["dict"];
                if (!dVal.IsDict()) {
                     errs.push_back("dict.set: 'dict' argument is not a dictionary");
                     return Cas::Value();
                }

                Cas::Dict d(dVal);
                d->Set(args["key"], args["value"]);
                return Cas::Value(true);
            } 
            else if (command == "has") {
                 if (!args.count("dict") || !args.count("key")) {
                     errs.push_back("dict.has requires 'dict' and 'key'");
                     return Cas::Value();
                 }
                 Cas::Value dVal = args["dict"];
                 if (!dVal.IsDict()) return Cas::Value(false);
                 Cas::Dict d(dVal);
                 return Cas::Value(d->Has(args["key"]));
            }
            else if (command == "remove") {
                 if (!args.count("dict") || !args.count("key")) {
                     errs.push_back("dict.remove requires 'dict' and 'key'");
                     return Cas::Value();
                 }
                 Cas::Value dVal = args["dict"];
                 if (dVal.IsDict()) {
                     Cas::Dict d(dVal);
                     // I will implement "No Op" and log warning, to allow other tests to pass.
                     // Or I can leave it failing.
                     
                     // Wait! I can implement 'remove' by creating a new dictionary and 'flow.set' it back to the variable??
                     // No, "dict.remove" is an Op. It is supposed to mutate.
                     
                     bool bRem = d->Remove(args["key"]);
                     return Cas::Value(bRem);
                 }
                 return Cas::Value(false);
            }
            else if (command == "keys") {
                 if (!args.count("dict")) {
                     errs.push_back("dict.keys requires 'dict'");
                     return Cas::Value();
                 }
                 Cas::Value dVal = args["dict"];
                 Cas::List keys;
                 if (dVal.IsDict()) {
                     Cas::Dict d(dVal);
                     d->Enum([&](Cas::Value& k, Cas::Value& v){
                         keys->AddItem(k);
                     });
                 }
                 return keys;
            }
            else {
                errs.push_back("Unknown command: " + command);
            }

            return Cas::Value();
        }
    };
}
