#include "CasRunner.h"
#include <sstream>
#include <iostream>
#include <regex>
#include <thread>
#include <algorithm> // Added for transform
#include <functional> // [NEW] Added for std::function
#include "xlang.h"
#include "CasExpression.h" // [NEW]

namespace CasLang {
    
    CasRunner::CasRunner() {
    }

    // Condition Evaluator (Simple)
    bool EvaluateCond(const std::string& cond) {
        // ... (rest of function using EvaluateExpr)
        std::string t = cond;
        // Trim
        t.erase(0, t.find_first_not_of(" \t"));
        t.erase(t.find_last_not_of(" \t") + 1);

        if (t.empty()) return false;
        if (t[0] == '!') return !EvaluateCond(t.substr(1));

        std::regex re(R"(\s*(.*?)\s*(==|!=|>=|<=|>|<)\s*(.*)\s*)");
        std::smatch m;
        if (std::regex_match(t, m, re)) {
             std::string lhs = m[1];
             std::string op = m[2];
             std::string rhs = m[3];
             
             X::Value vL = EvaluateExpr(lhs);
             X::Value vR = EvaluateExpr(rhs);
             
             if (vL.isNumber() && vR.isNumber()) {
                 double l = (double)vL;
                 double r = (double)vR;
                 if (op == "==") return l == r;
                 if (op == "!=") return l != r;
                 if (op == ">") return l > r;
                 if (op == "<") return l < r;
                 if (op == ">=") return l >= r;
                 if (op == "<=") return l <= r;
             }
             else {
                 std::string sL = vL.ToString();
                 std::string sR = vR.ToString();
                 if (op == "==") return sL == sR;
                 if (op == "!=") return sL != sR;
             }
        }
        
        if (t == "true") return true;
        
        X::Value v = EvaluateExpr(t);
        return v.IsTrue();
    }

    CasRunner::~CasRunner() {
    }

    void CasRunner::Register(std::unique_ptr<CasOps> op) {
        if (op) {
            m_ops[op->Namespace()] = std::move(op);
        }
    }

    void CasRunner::LogError(const std::string& msg) {
        std::cerr << "[CasLang Error] " << msg << std::endl;
    }

    // Basic parser for #ns.cmd{json}
    bool CasRunner::ParseLine(const std::string& line, std::string& ns, std::string& cmd, std::unordered_map<std::string, X::Value>& args, std::string& outErr) {
        if (line.empty()) return false;
        size_t hashPos = line.find('#');
        if (hashPos == std::string::npos) {
             outErr = "Missing '#' at start of command. Line: [" + line + "]";
             return false;
        }

        size_t bracePos = line.find('{', hashPos);
        if (bracePos == std::string::npos) {
             outErr = "Missing '{' for JSON arguments. Line: [" + line + "]";
             return false;
        }

        std::string fullCmd = line.substr(hashPos + 1, bracePos - (hashPos + 1));
        
        size_t dotPos = fullCmd.find('.');
        if (dotPos == std::string::npos) {
             outErr = "Command format must be #namespace.command";
             return false;
        }
        ns = fullCmd.substr(0, dotPos);
        cmd = fullCmd.substr(dotPos + 1);

        size_t closeBrace = line.rfind('}');
        if (closeBrace == std::string::npos || closeBrace < bracePos) {
             outErr = "Missing closing '}' for JSON arguments";
             return false;
        }

        std::string jsonStr = line.substr(bracePos, closeBrace - bracePos + 1);
        
        X::Runtime rt;
        X::Package json(rt, "json", "");
        X::Value jsonVal = json["loads"](jsonStr);

        if (!jsonVal.IsObject()) {
            if (jsonStr.find("\\n#") != std::string::npos) {
                 outErr = "E1002 E_SCRIPT_FORMAT: Malformed script. Found escaped newline '\\n' between commands. Use real line breaks (ASCII 10).";
                 return false;
            }
            outErr = "Args must be JSON object: " + jsonStr;
            //LogError(outErr);
            return false;
        }

        X::Dict dict(jsonVal);
        dict->Enum([&](X::Value& k, X::Value& v){
            args[k.asString()] = v;
        });

        return true;
    }

    CasRunner::Result CasRunner::ValidateScript(const std::vector<std::string>& lines) {
        std::vector<std::string> scopeStack; // "if", "loop", "retry"

        for (size_t i = 0; i < lines.size(); ++i) {
            std::string line = lines[i];
            if (line.empty() || line.find_first_not_of(" \t\r\n") == std::string::npos) continue;

            // Strict check: Must start with #
            size_t firstChar = line.find_first_not_of(" \t\r\n");
            if (line[firstChar] != '#') {
                return { false, "Line " + std::to_string(i + 1) + ": Invalid syntax (must start with #)", (int)i + 1, X::Value() };
            }

            // Parse Line
            std::string ns, cmd, err;
            std::unordered_map<std::string, X::Value> args;
            if (!ParseLine(line.substr(firstChar), ns, cmd, args, err)) {
                return { false, "Line " + std::to_string(i + 1) + ": " + err, (int)i + 1, X::Value() };
            }

            // Valid Command - Check Flow
            if (ns == "flow") {
                if (cmd == "loop_start") scopeStack.push_back("loop");
                else if (cmd == "if") scopeStack.push_back("if");
                else if (cmd == "retry_start") scopeStack.push_back("retry");
                else if (cmd == "loop_end") {
                    if (scopeStack.empty() || scopeStack.back() != "loop") 
                        return { false, "Line " + std::to_string(i + 1) + ": Unexpected #flow.loop_end", (int)i + 1, X::Value() };
                    scopeStack.pop_back();
                }
                else if (cmd == "retry_end") {
                    if (scopeStack.empty() || scopeStack.back() != "retry") 
                        return { false, "Line " + std::to_string(i + 1) + ": Unexpected #flow.retry_end", (int)i + 1, X::Value() };
                    scopeStack.pop_back();
                }
                else if (cmd == "endif") {
                    if (scopeStack.empty() || scopeStack.back() != "if") 
                        return { false, "Line " + std::to_string(i + 1) + ": Unexpected #flow.endif", (int)i + 1, X::Value() };
                    scopeStack.pop_back();
                }
                else if (cmd == "else") {
                     if (scopeStack.empty() || scopeStack.back() != "if")
                        return { false, "Line " + std::to_string(i + 1) + ": Unexpected #flow.else (not in if)", (int)i + 1, X::Value() };
                }
            }
        }

        if (!scopeStack.empty()) {
            return { false, "Unclosed scope: " + scopeStack.back(), (int)lines.size(), X::Value() };
        }

        return { true, "", -1, X::Value() };
    }

    // Helper: Find matching end block (supports nesting)
    // blockType: "if" or "loop" or "retry"
    size_t CasRunner::FindBlockEnd(const std::vector<std::string>& lines, size_t startLine, const std::string& blockType) {
        int depth = 1;
        std::string startCmd = "#flow." + blockType;
        if (blockType == "loop") startCmd = "#flow.loop_start";
        else if (blockType == "retry") startCmd = "#flow.retry_start";
        else startCmd = "#flow.if";

        std::string endCmd = "#flow." + blockType + "_end"; 
        if (blockType == "if") endCmd = "#flow.endif";
        if (blockType == "loop") endCmd = "#flow.loop_end";
        if (blockType == "retry") endCmd = "#flow.retry_end";

        for (size_t i = startLine + 1; i < lines.size(); ++i) {
            std::string line = lines[i];
            // Normalize line checks?
            if (line.find(startCmd) != std::string::npos) {
                depth++;
            } else if (line.find(endCmd) != std::string::npos) {
                depth--;
                if (depth == 0) return i;
            }
        }
        return lines.size();
    }

    // Special for IF: find next at same level: else, or endif
    size_t CasRunner::FindElseOrEndif(const std::vector<std::string>& lines, size_t startLine) {
         int depth = 1;
         for (size_t i = startLine + 1; i < lines.size(); ++i) {
            std::string line = lines[i];
            if (line.find("#flow.if") != std::string::npos) {
                depth++;
            } else if (line.find("#flow.endif") != std::string::npos) {
                depth--;
                if (depth == 0) return i;
            } else if (line.find("#flow.else") != std::string::npos) {
                if (depth == 1) return i;
            }
        }
        return lines.size();
    }





    // Retry State
    struct RetryState {
        size_t startPc; // Index of retry_start line
        int count;
        int delay;
    };

    CasRunner::Result CasRunner::Run(const std::string& script) {
        m_ctx.break_flag = false;
        m_ctx.continue_flag = false;
        m_ctx.return_flag = false;
        m_ctx.return_flag = false;
        m_ctx._last = X::Value();
        m_ctx.logs.clear();

        // 1. Split into lines
        std::vector<std::string> lines;
        std::istringstream iss(script);
        std::string l;
        while (std::getline(iss, l)) {
             size_t first = l.find_first_not_of(" \t\r\n");
             if (first == std::string::npos) lines.push_back(""); 
             else {
                 size_t last = l.find_last_not_of(" \t\r\n");
                 lines.push_back(l.substr(first, last - first + 1));
             }
        }

        // 2. Validate Phase
        Result validation = ValidateScript(lines);
        if (!validation.success) {
            return validation;
        }

        std::vector<std::pair<size_t, size_t>> loopStack;
        std::vector<RetryState> retryStack;

        // 3. PC Loop
        size_t pc = 0;
        while (pc < lines.size()) {
            m_ctx.current_line = (int)pc + 1;
            std::string line = lines[pc];
            if (line.empty() || line[0] != '#') { pc++; continue; }

            std::string ns, cmd, err;
            std::unordered_map<std::string, X::Value> args;
            if (!ParseLine(line, ns, cmd, args, err)) {
                 // Should be caught by validation, but just in case
                 return { false, "Runtime Parsing Error: " + err, (int)pc + 1, X::Value() };
            }

            // Substitute variables in args
             for (auto& kv : args) {
                if (kv.second.isString()) {
                    std::string s = kv.second.asString();
                    
                    // Optimization: Check for exact match "${var}" to preserve type
                    if (s.size() > 3 && s.front() == '$' && s[1] == '{' && s.back() == '}') {
                        std::string content = s.substr(2, s.size() - 3);
                        std::string varName = content;
                        std::string keyName;
                        long long listIdx = -1;
                        int accessType = 0; // 0=None, 1=Dict, 2=List
                        
                        size_t bracketOpen = content.find('[');
                        size_t bracketClose = content.rfind(']');
                        if (bracketOpen != std::string::npos && bracketClose != std::string::npos && bracketClose > bracketOpen) {
                             varName = content.substr(0, bracketOpen);
                             std::string inner = content.substr(bracketOpen + 1, bracketClose - (bracketOpen + 1));
                             if (inner.size() >= 2 && inner.front() == '\'' && inner.back() == '\'') {
                                 keyName = inner.substr(1, inner.size() - 2);
                                 accessType = 1;
                             } else {
                                 try {
                                     listIdx = std::stoll(inner);
                                     accessType = 2;
                                 } catch(...) {}
                             }
                        }

                        if (accessType > 0) {
                            if (!m_ctx.vars.count(varName)) {
                                return { false, "E2201 E_VAR_UNDEFINED: " + varName, (int)pc + 1, X::Value() };
                            }
                            X::Value base = m_ctx.vars[varName];
                            
                            if (accessType == 1) { // Dict
                                if (!base.IsDict()) return { false, "E2201 E_VAR_TYPE_ERROR: " + varName + " is not a dict", (int)pc + 1, X::Value() };
                                X::Dict d(base);
                                if (d->Has(keyName.c_str())) { kv.second = d[keyName.c_str()]; continue; }
                                else return { false, "E2206 E_INDEX_KEY_NOT_FOUND: Key '" + keyName + "' not found in " + varName, (int)pc + 1, X::Value() };
                            }
                            else if (accessType == 2) { // List
                                if (!base.IsList()) return { false, "E2201 E_VAR_TYPE_ERROR: " + varName + " is not a list", (int)pc + 1, X::Value() };
                                X::List l(base);
                                if (listIdx >= 0 && listIdx < l.Size()) { kv.second = l[(int)listIdx]; continue; }
                                else return { false, "E2207 E_INDEX_OUT_OF_RANGE: Index " + std::to_string(listIdx) + " out of bounds for " + varName, (int)pc + 1, X::Value() };
                            }
                        }
                        else {
                            if (varName == "_last") {
                                if (m_ctx._last.IsValid()) {
                                    kv.second = m_ctx._last;
                                    continue;
                                }
                            }
                            else if (m_ctx.vars.count(varName)) {
                                kv.second = m_ctx.vars[varName];
                                continue;
                            }
                        }
                    }

                    size_t pos = 0;
                    while ((pos = s.find("${", pos)) != std::string::npos) {
                        size_t end = s.find('}', pos);
                        if (end != std::string::npos) {
                            std::string content = s.substr(pos + 2, end - (pos + 2));
                            std::string varName = content;
                            std::string keyName;
                            long long listIdx = -1;
                            int accessType = 0;
                             
                            size_t bracketOpen = content.find('[');
                            size_t bracketClose = content.rfind(']');
                            if (bracketOpen != std::string::npos && bracketClose != std::string::npos && bracketClose > bracketOpen) {
                                  varName = content.substr(0, bracketOpen);
                                  std::string inner = content.substr(bracketOpen + 1, bracketClose - (bracketOpen + 1));
                                  if (inner.size() >= 2 && inner.front() == '\'' && inner.back() == '\'') {
                                      keyName = inner.substr(1, inner.size() - 2);
                                      accessType = 1;
                                  } else {
                                      try {
                                          listIdx = std::stoll(inner);
                                          accessType = 2;
                                      } catch(...) {}
                                  }
                            }

                            std::string valStr = "null";
                            if (accessType > 0) {
                                 if (m_ctx.vars.count(varName)) {
                                     X::Value base = m_ctx.vars[varName];
                                     if (accessType == 1 && base.IsDict()) {
                                         X::Dict d(base);
                                         if (d->Has(keyName.c_str())) valStr = d[keyName.c_str()].ToString();
                                         else return { false, "E2206 E_INDEX_KEY_NOT_FOUND: Key '" + keyName + "' not found", (int)pc + 1, X::Value() };
                                     }
                                     else if (accessType == 2 && base.IsList()) {
                                         X::List l(base);
                                         if (listIdx >= 0 && listIdx < l.Size()) valStr = l[(int)listIdx].ToString();
                                         else return { false, "E2207 E_INDEX_OUT_OF_RANGE: Index " + std::to_string(listIdx) + " out of bounds", (int)pc + 1, X::Value() };
                                     }
                                 }
                            }
                            else if (varName == "_last") valStr = m_ctx._last.IsValid() ? m_ctx._last.ToString() : "null";
                            else if (m_ctx.vars.count(varName)) valStr = m_ctx.vars[varName].ToString();
                            
                            s.replace(pos, end - pos + 1, valStr);
                        }
                        pos++; 
                    }
                    kv.second = X::Value(s);
                }
            }

            // --- Flow Control ---
            bool isErr = false;
            std::string errMsg;

            if (ns == "flow") {
                if (cmd == "set") {
                    if (args.count("name") && args.count("value")) {
                        std::string name = args["name"].asString();
                        X::Value val = args["value"];
                        
                        // Check for Expression
                        if (val.isString()) {
                            std::string sVal = val.asString();
                            if (!sVal.empty() && sVal[0] == '=') {
                                val = EvaluateExpr(sVal.substr(1));
                            }
                        }
                        
                        
                        // Deep Copy Logic using Native API
                        if (val.IsList() || val.IsDict()) {
                            bool handled = false;
                            if (val.IsList()) {
                                X::List l(val);
                                if (l.Size() == 0) {
                                     m_ctx.vars[name] = X::Value(X::g_pXHost->CreateList());
                                     handled = true;
                                }
                            }
                            else if (val.IsDict()) {
                                X::Dict d(val);
                                if (d.Size() == 0) {
                                     m_ctx.vars[name] = X::Value(X::g_pXHost->CreateDict());
                                     handled = true;
                                }
                            }
                            
                            if (!handled) {
                                X::Value vCopy = val;
                                vCopy.Clone(); 
                                m_ctx.vars[name] = vCopy;
                            }
                        } else {
                            m_ctx.vars[name] = val;
                        }
                    }
                }
                else if (cmd == "get") {
                     if (args.count("name")) {
                         std::string name = args["name"].asString();
                         if (m_ctx.vars.count(name)) m_ctx._last = m_ctx.vars[name];
                     }
                }
                else if (cmd == "if") {
                    std::string cond = args["cond"].asString();
                    if (!EvaluateCond(cond)) {
                        size_t nextPt = FindElseOrEndif(lines, pc);
                        pc = nextPt; 
                        if (pc < lines.size() && lines[pc].find("#flow.else") != std::string::npos) {
                             pc++; continue; 
                        }
                    }
                }
                else if (cmd == "else") {
                    size_t i = pc + 1;
                    int depth = 1; 
                    while(i < lines.size()) {
                        if (lines[i].find("#flow.if") != std::string::npos) depth++;
                        else if (lines[i].find("#flow.endif") != std::string::npos) {
                            depth--;
                            if (depth == 0) { pc = i; break; }
                        }
                        i++;
                    }
                }
                else if (cmd == "endif") {
                }
                else if (cmd == "loop_start") {
                     std::string varName = args["var"].asString();
                     // Check if 'in' is already a list (from variable substitution)
                     X::Value listVal;
                     if (args["in"].IsList()) {
                         listVal = args["in"];
                     }
                     else {
                         std::string listJson = args["in"].asString();
                         // Parse list
                         if (listJson.size()>=2 && listJson.front()=='[' && listJson.back()==']') {
                             X::Runtime rt;
                             X::Package json(rt, "json", "");
                             listVal = json["loads"](listJson);
                         } else {
                             // Maybe variable reference that wasn't substituted? Or simple string?
                             // For now, assume it MUST be a JSON list
                             errMsg = "loop_start: 'in' must be a JSON list found:" + listJson;
                             isErr = true;
                         }
                     }

                     if (!isErr && listVal.IsList()) {
                         X::List list(listVal);
                         long long size = list.Size();
                         
                         // Check if we are re-entering (iterating)
                         if (!loopStack.empty() && loopStack.back().first == pc) {
                             // Increment
                             loopStack.back().second++;
                         } else {
                             // New Loop
                             loopStack.push_back({pc, 0});
                         }

                         long long idx = (long long)loopStack.back().second;
                         if (idx < size) {
                             m_ctx.vars[varName] = list[idx];
                             // Proceed to next line (body)
                         } else {
                             // Used up
                             loopStack.pop_back();
                             // Skip to end
                             size_t end = FindBlockEnd(lines, pc, "loop");
                             pc = end; 
                             continue; // Skip the pc++ at end of loop
                         }
                     } else {
                         // Not a list or error
                         if(!isErr) {
                             errMsg = "loop_start: 'in' is not a list";
                             isErr = true;
                         }
                     }
                }
                else if (cmd == "loop_end") {
                     if (!loopStack.empty()) {
                         size_t start = loopStack.back().first;
                         pc = start; // Jump back to loop_start
                         continue;
                     }
                }
                else if (cmd == "break") {
                    if (!loopStack.empty()) {
                        size_t start = loopStack.back().first;
                        size_t end = FindBlockEnd(lines, start, "loop");
                        pc = end; 
                        loopStack.pop_back();
                    }
                }
                else if (cmd == "retry_start") {
                    int count = 3;
                    int delay = 1000;
                    if (args.count("count")) count = args["count"].isNumber() ? (int)args["count"] : std::stoi(args["count"].asString());
                    if (args.count("delay")) delay = args["delay"].isNumber() ? (int)args["delay"] : std::stoi(args["delay"].asString());
                    
                    retryStack.push_back({pc, count, delay});
                }
                else if (cmd == "retry_end") {
                    if (!retryStack.empty()) {
                        retryStack.pop_back();
                    }
                }
                else if (cmd == "return") {
                    if (args.count("value")) m_ctx.return_value = args["value"];
                    else m_ctx.return_value = m_ctx._last;
                    m_ctx.return_flag = true;
                    return { true, "", -1, m_ctx.return_value };
                }
            }
            else {
                // Execute Op
                std::vector<std::string> errs;
                if (m_ops.count(ns)) {
                    try {
                        m_ctx._last = m_ops[ns]->Execute({ns}, cmd, args, m_ctx, errs);
                        if (!errs.empty()) {
                            isErr = true;
                            errMsg = errs[0];
                        }
                        if (args.count("as")) {
                            m_ctx.vars[args["as"].asString()] = m_ctx._last;
                        }
                        if (args.count("return") && args["return"].IsTrue()) {
                             m_ctx.return_value = m_ctx._last;
                             m_ctx.return_flag = true;
                             return { true, "", -1, m_ctx._last };
                        }
                    }
                    catch (const std::exception& e) {
                        isErr = true;
                        errMsg = "E3001 E_RUNTIME_EXCEPTION: " + std::string(e.what());
                    }
                    catch (...) {
                        isErr = true;
                        errMsg = "E3001 E_RUNTIME_EXCEPTION: Unknown Error";
                    }
                } 
                else if (m_externalHandler)
                {
                    try {
                        m_ctx._last = m_externalHandler(ns, cmd, args);
                        if (args.count("as")) {
                            m_ctx.vars[args["as"].asString()] = m_ctx._last;
                        }
                    } 
                    catch (const std::exception& e) {
                        isErr = true;
                        errMsg = std::string("External Handler Error: ") + e.what();
                    }
                    catch (...) {
                        isErr = true;
                        errMsg = "External Handler Unknown Error";
                    }
                }
                else {
                    isErr = true;
                    errMsg = "Unknown namespace: " + ns;
                }
            }

            // Error Handling & Retry Logic
            if (isErr) {
                if (!retryStack.empty()) {
                    RetryState& rs = retryStack.back();
                    if (rs.count > 0) {
                        rs.count--;
                        LogError("Error caught (" + errMsg + "), retrying... " + std::to_string(rs.count) + " left");
                        std::this_thread::sleep_for(std::chrono::milliseconds(rs.delay));
                        pc = rs.startPc + 1; // Jump to instruction AFTER retry_start
                        continue;
                    }
                    else {
                        LogError(errMsg + " (Retry exhausted)");
                        return { false, errMsg, (int)pc + 1, X::Value() };
                    }
                }
                else {
                    LogError(errMsg);
                    return { false, errMsg, (int)pc + 1, X::Value() };
                }
            }

            pc++;
        }
        
        return { true, "", -1, m_ctx._last };
    }
}
