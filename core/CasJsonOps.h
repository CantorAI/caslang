#pragma once
#include "CasOps.h"
#include <string>
#include "xlang.h"

namespace CasLang {
    class CasJsonOps : public CasOps {
        std::string m_ns = "json";
    public:
        using CasOps::CasOps;
        
        const std::string& Namespace() const override { return m_ns; }

        X::Value Execute(const std::vector<std::string>& ns_parts, 
                         const std::string& command, 
                         std::unordered_map<std::string, X::Value>& args, 
                         CasContext& ctx, 
                         std::vector<std::string>& errs) override {
            
            if (command == "parse") {
                // json.parse: parse a JSON string into a dict or list
                // Usage: {"op":"json.parse","s":"${raw_json}","as":"obj"}
                if (!args.count("s")) {
                    errs.push_back("json.parse requires 's' (JSON string)");
                    return X::Value();
                }
                std::string s = args["s"].asString();
                if (s.empty()) {
                    errs.push_back("json.parse: empty string");
                    return X::Value();
                }
                
                X::Runtime rt;
                X::Package json(rt, "json", "");
                try {
                    X::Value parsed = json["loads"](s);
                    return parsed;
                } catch (const std::exception& e) {
                    std::string preview = s.substr(0, 200);
                    if (s.size() > 200) preview += "...";
                    errs.push_back(std::string("json.parse error: ") + e.what() 
                        + " | input preview: " + preview);
                    return X::Value();
                } catch (...) {
                    std::string preview = s.substr(0, 200);
                    if (s.size() > 200) preview += "...";
                    errs.push_back("json.parse: invalid JSON | input preview: " + preview);
                    return X::Value();
                }
            }
            else if (command == "save") {
                // json.save: serialize a dict or list to a JSON string
                // Usage: {"op":"json.save","obj":"${mydict}","as":"json_str"}
                if (!args.count("obj")) {
                    errs.push_back("json.save requires 'obj' (dict or list)");
                    return X::Value();
                }
                X::Value obj = args["obj"];
                
                X::Runtime rt;
                X::Package json(rt, "json", "");
                try {
                    X::Value result = json["dumps"](obj);
                    return result;
                } catch (const std::exception& e) {
                    errs.push_back(std::string("json.save error: ") + e.what());
                    return X::Value();
                } catch (...) {
                    errs.push_back("json.save: serialization failed");
                    return X::Value();
                }
            }
            
            errs.push_back("json: unknown command " + command);
            return X::Value();
        }
    };
}
