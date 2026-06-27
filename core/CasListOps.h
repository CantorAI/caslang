#pragma once
#include "CasOps.h"
#include <vector>
#include <string>
#include <unordered_map>

namespace CasLang {
    class CasListOps : public CasOps {
        std::string m_ns = "list";
    public:
        using CasOps::CasOps;
        
        const std::string& Namespace() const override { return m_ns; }

        Cas::Value Execute(const std::vector<std::string>& ns_parts, 
                         const std::string& command, 
                         std::unordered_map<std::string, Cas::Value>& args, 
                         CasContext& ctx, 
                         std::vector<std::string>& errs) override {
            
            if (command == "append") {
                if (!args.count("list") || !args.count("value")) {
                    errs.push_back("list.append requires 'list' and 'value'");
                    return Cas::Value();
                }
                Cas::Value lVal = args["list"];
                if (!lVal.IsList()) {
                    errs.push_back("list.append: 'list' is not a list");
                    return Cas::Value();
                }
                Cas::List list(lVal);
                Cas::Value val = args["value"]; // non-const for API
                list->AddItem(val);
                return Cas::Value(true);
            }
            else if (command == "remove") {
                if (!args.count("list") || !args.count("index")) {
                    errs.push_back("list.remove requires 'list' and 'index'");
                    return Cas::Value();
                }
                Cas::Value lVal = args["list"];
                if (!lVal.IsList()) {
                    errs.push_back("list.remove: 'list' is not a list");
                    return Cas::Value();
                }
                Cas::List list(lVal);
                long long idx = args["index"].isNumber() ? (long long)args["index"] : std::stoll(args["index"].asString());
                long long size = list->Size();
                if (idx < 0 || idx >= size) return Cas::Value(false);

                // Rebuild list without item (Native RemoveAt invalid/unstable)
                Cas::List newList;
                for (long long i = 0; i < size; i++) {
                    if (i == idx) continue;
                    Cas::Value v = list->Get(i);
                    newList->AddItem(v);
                }
                list->RemoveAll();
                long long newSize = newList->Size();
                for (long long i = 0; i < newSize; i++) {
                    Cas::Value v = newList->Get(i);
                    list->AddItem(v);
                }
                return Cas::Value(true);
            }
            else if (command == "len") {
                 if (!args.count("list")) {
                    errs.push_back("list.len requires 'list'");
                    return Cas::Value();
                }
                Cas::Value lVal = args["list"];
                if (!lVal.IsList()) {
                    errs.push_back("list.len: 'list' is not a list");
                    return Cas::Value();
                }
                Cas::List list(lVal);
                return Cas::Value((long long)list.Size());
            }
            else if (command == "range") {
                long long from = args.count("from") ? (long long)args["from"] : 0;
                long long to = args.count("to") ? (long long)args["to"] : 0;
                long long step = args.count("step") ? (long long)args["step"] : 1;
                
                if (step == 0) step = 1;
                
                Cas::List list;
                if (step > 0) {
                     for (long long i = from; i < to; i += step) {
                         Cas::Value v(i);
                         list->AddItem(v);
                     }
                } else {
                     for (long long i = from; i > to; i += step) {
                         Cas::Value v(i);
                         list->AddItem(v);
                     }
                }
                return list;
            }
            else {
                errs.push_back("Unknown command: " + command);
            }

            return Cas::Value();
        }
    };
}
