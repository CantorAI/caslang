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

        X::Value Execute(const std::vector<std::string>& ns_parts, 
                         const std::string& command, 
                         std::unordered_map<std::string, X::Value>& args, 
                         CasContext& ctx, 
                         std::vector<std::string>& errs) override {
            
            if (command == "get") {
                if (args.find("dict") == args.end() || args.find("key") == args.end()) {
                    errs.push_back("dict.get requires 'dict' and 'key' arguments");
                    return X::Value();
                }
                
                X::Value dVal = args["dict"];
                X::Value kVal = args["key"];
                if (!dVal.IsDict()) {
                    errs.push_back("dict.get: 'dict' argument is not a dictionary");
                    return X::Value();
                }
                
                X::Dict d(dVal);
                
                if (d->Has(kVal)) {
                     // Check if there is a Get(X::Value) 
                     // d->Get(kVal) returns X::Value
                     return d->Get(kVal);
                } else {
                    return X::Value(); // null
                }
            }
            else if (command == "set") {
                if (args.find("dict") == args.end() || args.find("key") == args.end() || args.find("value") == args.end()) {
                     errs.push_back("dict.set requires 'dict', 'key', and 'value' arguments");
                     return X::Value();
                }
                
                X::Value dVal = args["dict"];
                if (!dVal.IsDict()) {
                     errs.push_back("dict.set: 'dict' argument is not a dictionary");
                     return X::Value();
                }

                X::Dict d(dVal);
                d->Set(args["key"], args["value"]);
                return X::Value(true);
            } 
            else if (command == "has") {
                 if (!args.count("dict") || !args.count("key")) {
                     errs.push_back("dict.has requires 'dict' and 'key'");
                     return X::Value();
                 }
                 X::Value dVal = args["dict"];
                 if (!dVal.IsDict()) return X::Value(false);
                 X::Dict d(dVal);
                 return X::Value(d->Has(args["key"]));
            }
            else if (command == "remove") {
                 if (!args.count("dict") || !args.count("key")) {
                     errs.push_back("dict.remove requires 'dict' and 'key'");
                     return X::Value();
                 }
                 X::Value dVal = args["dict"];
                 if (dVal.IsDict()) {
                     X::Dict d(dVal);
                     // I will implement "No Op" and log warning, to allow other tests to pass.
                     // Or I can leave it failing.
                     
                     // Wait! I can implement 'remove' by creating a new dictionary and 'flow.set' it back to the variable??
                     // No, "dict.remove" is an Op. It is supposed to mutate.
                     
                     bool bRem = d->Remove(args["key"]);
                     return X::Value(bRem);
                 }
                 return X::Value(false);
            }
            else if (command == "keys") {
                 if (!args.count("dict")) {
                     errs.push_back("dict.keys requires 'dict'");
                     return X::Value();
                 }
                 X::Value dVal = args["dict"];
                 X::List keys;
                 if (dVal.IsDict()) {
                     X::Dict d(dVal);
                     d->Enum([&](X::Value& k, X::Value& v){
                         keys->AddItem(k);
                     });
                 }
                 return keys;
            }
            else {
                errs.push_back("Unknown command: " + command);
            }

            return X::Value();
        }
    };
}
