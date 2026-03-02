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
            else if (command == "query") {
                // json.query: deep query with dot-path syntax
                // Usage: {"op":"json.query","obj":"${doc}","path":"a.b[0].c","as":"val"}
                //        {"op":"json.query","obj":"${doc}","path":"items[*].name","as":"names"}
                if (!args.count("obj") || !args.count("path")) {
                    errs.push_back("json.query requires 'obj' and 'path'");
                    return X::Value();
                }
                X::Value obj = args["obj"];
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
                                return X::Value();
                            }
                            long long idx = std::stoll(path.substr(numStart, p - numStart));
                            segs.push_back({Seg::INDEX, "", idx});
                        }
                        if (p < path.size() && path[p] == ']') p++; // skip ']'
                    } else if (path[p] == '.') {
                        p++; // skip dot separator
                    } else {
                        // key name — read until '.', '[', or end
                        size_t keyStart = p;
                        while (p < path.size() && path[p] != '.' && path[p] != '[') p++;
                        segs.push_back({Seg::KEY, path.substr(keyStart, p - keyStart), 0});
                    }
                }
                
                if (segs.empty()) {
                    return obj; // empty path returns the object itself
                }
                
                // Recursive traversal function
                std::function<X::Value(X::Value, size_t)> traverse;
                traverse = [&](X::Value cur, size_t si) -> X::Value {
                    if (si >= segs.size()) return cur;
                    
                    const Seg& seg = segs[si];
                    
                    if (seg.type == Seg::KEY) {
                        if (!cur.IsDict()) {
                            errs.push_back("json.query: expected dict at '" + seg.key + "' but got non-dict");
                            return X::Value();
                        }
                        X::Dict d(cur);
                        if (!d->Has(seg.key.c_str())) {
                            errs.push_back("json.query: key '" + seg.key + "' not found");
                            return X::Value();
                        }
                        X::Value next = d[seg.key.c_str()];
                        return traverse(next, si + 1);
                    }
                    else if (seg.type == Seg::INDEX) {
                        if (!cur.IsList()) {
                            errs.push_back("json.query: expected list at index [" + std::to_string(seg.idx) + "] but got non-list");
                            return X::Value();
                        }
                        X::List l(cur);
                        if (seg.idx < 0 || seg.idx >= l.Size()) {
                            errs.push_back("json.query: index [" + std::to_string(seg.idx) + "] out of range (size=" + std::to_string(l.Size()) + ")");
                            return X::Value();
                        }
                        X::Value next = l[seg.idx];
                        return traverse(next, si + 1);
                    }
                    else { // WILDCARD [*]
                        if (!cur.IsList()) {
                            errs.push_back("json.query: [*] requires a list");
                            return X::Value();
                        }
                        X::List src(cur);
                        X::List result;
                        long long sz = src.Size();
                        for (long long i = 0; i < sz; i++) {
                            X::Value elem = src[i];
                            X::Value val = traverse(elem, si + 1);
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
            return X::Value();
        }
    };
}
