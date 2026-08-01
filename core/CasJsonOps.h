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
#include "cas_value.h"
#include <nlohmann/json.hpp>

// Convert JSON to Cas::Value
inline Cas::Value JsonToCasValue(const nlohmann::json& j) {
    if (j.is_null()) return Cas::Value();
    if (j.is_boolean()) return Cas::Value(j.get<bool>());
    if (j.is_number_integer()) return Cas::Value((int64_t)j.get<int64_t>());
    if (j.is_number_unsigned()) return Cas::Value((int64_t)j.get<uint64_t>());
    if (j.is_number_float()) return Cas::Value(j.get<double>());
    if (j.is_string()) return Cas::Value(j.get<std::string>());
    if (j.is_array()) {
        Cas::List l;
        for (auto& e : j) l->AddItem(JsonToCasValue(e));
        return l;
    }
    if (j.is_object()) {
        Cas::Dict d;
        for (auto& el : j.items()) d->Set(el.key(), JsonToCasValue(el.value()));
        return d;
    }
    return Cas::Value();
}

// Convert Cas::Value to JSON
inline nlohmann::json CasValueToJson(const Cas::Value& val) {
    if (val.IsNone() || !val.IsValid()) return nullptr;
    if (val.isBool()) return val.asBool();
    if (val.isNumber()) {
        if (std::to_string((long long)val) == val.asString()) return (long long)val;
        return (double)val;
    }
    if (val.isString()) return val.asString();
    if (val.IsList()) {
        Cas::List l(val);
        nlohmann::json j = nlohmann::json::array();
        long long sz = l->Size();
        for (long long i = 0; i < sz; i++) j.push_back(CasValueToJson(l->Get(i)));
        return j;
    }
    if (val.IsDict()) {
        Cas::Dict d(val);
        nlohmann::json j = nlohmann::json::object();
        d->Enum([&](Cas::Value& k, Cas::Value& v) {
            j[k.asString()] = CasValueToJson(v);
        });
        return j;
    }
    return val.asString(); // fallback
}

namespace CasLang {
    class CasJsonOps : public CasOps {
        std::string m_ns = "json";
    public:
        using CasOps::CasOps;
        
        const std::string& Namespace() const override { return m_ns; }

        Cas::Value Execute(const std::vector<std::string>& ns_parts, 
                         const std::string& command, 
                         std::unordered_map<std::string, Cas::Value>& args, 
                         CasContext& ctx, 
                         std::vector<std::string>& errs) override {
            
            if (command == "parse") {
                // json.parse: parse a JSON string into a dict or list
                // Usage: {"op":"json.parse","s":"${raw_json}","as":"obj"}
                if (!args.count("s")) {
                    errs.push_back("json.parse requires 's' (JSON string)");
                    return Cas::Value();
                }
                std::string s = args["s"].asString();
                if (s.empty()) {
                    errs.push_back("json.parse: empty string");
                    return Cas::Value();
                }
                
                try {
                    nlohmann::json parsed = nlohmann::json::parse(s);
                    return JsonToCasValue(parsed);
                } catch (const std::exception& e) {
                    std::string preview = s.substr(0, 200);
                    if (s.size() > 200) preview += "...";
                    errs.push_back(std::string("json.parse error: ") + e.what() 
                        + " | input preview: " + preview);
                    return Cas::Value();
                } catch (...) {
                    std::string preview = s.substr(0, 200);
                    if (s.size() > 200) preview += "...";
                    errs.push_back("json.parse: invalid JSON | input preview: " + preview);
                    return Cas::Value();
                }
            }
            else if (command == "save") {
                // json.save: serialize a dict or list to a JSON string
                // Usage: {"op":"json.save","obj":"${mydict}","as":"json_str"}
                if (!args.count("obj")) {
                    errs.push_back("json.save requires 'obj' (dict or list)");
                    return Cas::Value();
                }
                Cas::Value obj = args["obj"];
                
                try {
                    nlohmann::json j = CasValueToJson(obj);
                    return Cas::Value(j.dump());
                } catch (const std::exception& e) {
                    errs.push_back(std::string("json.save error: ") + e.what());
                    return Cas::Value();
                } catch (...) {
                    errs.push_back("json.save: serialization failed");
                    return Cas::Value();
                }
            }
            else if (command == "query") {
                // json.query: deep query with dot-path syntax
                // Usage: {"op":"json.query","obj":"${doc}","path":"a.b[0].c","as":"val"}
                //        {"op":"json.query","obj":"${doc}","path":"items[*].name","as":"names"}
                if (!args.count("obj") || !args.count("path")) {
                    errs.push_back("json.query requires 'obj' and 'path'");
                    return Cas::Value();
                }
                Cas::Value obj = args["obj"];
                std::string path = args["path"].asString();
                
                // Parse path into segments
                // Segment types: KEY("name"), INDEX(0), WILDCARD
                struct Seg { enum Type { KEY, INDEX, WILDCARD } type; std::string key; long long idx = 0; };
                std::vector<Seg> segs;
                
                size_t p = 0;
                while (p < path.size()) {
                    if (path[p] == '[') {
                        p++; // skip '['
                        if (p < path.size() && path[p] == '*') {
                            segs.push_back({Seg::WILDCARD, "", 0});
                            p++; // skip '*'
                        } else {
                            // numeric index
                            size_t numStart = p;
                            while (p < path.size() && (path[p] >= '0' && path[p] <= '9')) p++;
                            if (p == numStart) {
                                errs.push_back("json.query: invalid index in path at position " + std::to_string(p));
                                return Cas::Value();
                            }
                            long long idx = std::stoll(path.substr(numStart, p - numStart));
                            segs.push_back({Seg::INDEX, "", idx});
                        }
                        if (p < path.size() && path[p] == ']') p++; // skip ']'
                    } else if (path[p] == '.') {
                        p++; // skip dot separator
                    } else {
                        // key name 鈥?read until '.', '[', or end
                        size_t keyStart = p;
                        while (p < path.size() && path[p] != '.' && path[p] != '[') p++;
                        segs.push_back({Seg::KEY, path.substr(keyStart, p - keyStart), 0});
                    }
                }
                
                if (segs.empty()) {
                    return obj; // empty path returns the object itself
                }
                
                // Recursive traversal function
                std::function<Cas::Value(Cas::Value, size_t)> traverse;
                traverse = [&](Cas::Value cur, size_t si) -> Cas::Value {
                    if (si >= segs.size()) return cur;
                    
                    const Seg& seg = segs[si];
                    
                    if (seg.type == Seg::KEY) {
                        if (!cur.IsDict()) {
                            errs.push_back("json.query: expected dict at '" + seg.key + "' but got non-dict");
                            return Cas::Value();
                        }
                        Cas::Dict d(cur);
                        if (!d->Has(seg.key.c_str())) {
                            errs.push_back("json.query: key '" + seg.key + "' not found");
                            return Cas::Value();
                        }
                        Cas::Value next = d[seg.key.c_str()];
                        return traverse(next, si + 1);
                    }
                    else if (seg.type == Seg::INDEX) {
                        if (!cur.IsList()) {
                            errs.push_back("json.query: expected list at index [" + std::to_string(seg.idx) + "] but got non-list");
                            return Cas::Value();
                        }
                        Cas::List l(cur);
                        if (seg.idx < 0 || seg.idx >= (int)l.Size()) {
                            errs.push_back("json.query: index [" + std::to_string(seg.idx) + "] out of range (size=" + std::to_string(l.Size()) + ")");
                            return Cas::Value();
                        }
                        Cas::Value next = l[seg.idx];
                        return traverse(next, si + 1);
                    }
                    else { // WILDCARD [*]
                        if (!cur.IsList()) {
                            errs.push_back("json.query: [*] requires a list");
                            return Cas::Value();
                        }
                        Cas::List src(cur);
                        Cas::List result;
                        long long sz = src.Size();
                        for (long long i = 0; i < sz; i++) {
                            Cas::Value elem = src[i];
                            Cas::Value val = traverse(elem, si + 1);
                            if (val.IsValid() && !val.IsNone()) {
                                result->AddItem(val);
                            }
                        }
                        return result;
                    }
                };
                
                return traverse(obj, 0);
            }
            
            errs.push_back("json: unknown command " + command);
            return Cas::Value();
        }
    };
}
