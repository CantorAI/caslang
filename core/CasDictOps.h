#pragma once
#include "CasOps.h"

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
                if (!dVal.IsDict()) {
                    errs.push_back("dict.get: 'dict' argument is not a dictionary");
                    return X::Value();
                }
                
                std::string key = args["key"].asString();
                X::Dict d(dVal);
                
                if (d->Has(key.c_str())) {
                    return d[key.c_str()];
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
                std::string key = args["key"].asString();
                d->Set(key.c_str(), args["value"]);
                return X::Value(true);
            } else {
                errs.push_back("Unknown command: " + command);
            }

            return X::Value();
        }
    };
}
