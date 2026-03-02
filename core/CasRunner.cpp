#include "CasRunner.h"
#include <sstream>
#include <iostream>
#include <regex>
#include <thread>
#include <algorithm>
#include <functional>
#include <unordered_set>
#include "xlang.h"
#include "CasExpression.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace CasLang {
    
    CasRunner::CasRunner() {
    }

    // Condition Evaluator (Simple)
    bool EvaluateCond(const std::string& cond) {
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
        
        if (t == "true" || t == "True") return true;
        if (t == "false" || t == "False") return false;
        
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

    // Convert ONLY scalars; arrays/objects are stringified JSON to keep scalar contract.
    static X::Value toXScalar(const json& v) {
        if (v.is_null())  return X::Value();
        if (v.is_boolean()) return X::Value(v.get<bool>());
        if (v.is_number_integer()) return X::Value((int64_t)v.get<long long>());
        if (v.is_number_unsigned()) return X::Value((int64_t)v.get<unsigned long long>());
        if (v.is_number_float()) return X::Value(v.get<double>());
        if (v.is_string()) return X::Value(v.get<std::string>());
        // composite -> stringify
        return X::Value(v.dump());
    }

    // JSONL parser: {"op":"ns.cmd", ...args}
    bool CasRunner::ParseLine(const std::string& line, std::string& ns, std::string& cmd,
        std::unordered_map<std::string, X::Value>& args, std::string& outErr) {
        if (line.empty()) return false;

        // Parse as JSON object
        json j;
        try {
            j = json::parse(line);
        }
        catch (const std::exception& e) {
            std::string preview = line.substr(0, 200);
            if (line.size() > 200) preview += "...";
            outErr = std::string("E1004 E_JSON_INVALID: ") + e.what() + " | line: " + preview;
            return false;
        }

        if (!j.is_object()) {
            outErr = "E1002 E_LINE_NOT_JSON_OBJECT";
            return false;
        }

        if (!j.contains("op") || !j["op"].is_string()) {
            outErr = "E2101 E_ARG_MISSING: 'op' field required";
            return false;
        }

        std::string opStr = j["op"].get<std::string>();

        // Split "ns.cmd" on first dot
        auto dotPos = opStr.find('.');
        if (dotPos == std::string::npos) {
            // Allow "caslang" header op without dot
            if (opStr == "caslang") {
                ns = "caslang";
                cmd = "";
            } else {
                outErr = "E1003 E_COMMAND_SYNTAX: op must be namespace.command, got: " + opStr;
                return false;
            }
        } else {
            ns = opStr.substr(0, dotPos);
            cmd = opStr.substr(dotPos + 1);
        }

        // All non-"op" keys become args
        for (auto it = j.begin(); it != j.end(); ++it) {
            if (it.key() == "op") continue;
            args.emplace(it.key(), toXScalar(it.value()));
        }

        return true;
    }

    // Helper: Try to parse a line as JSON and extract the "op" field.
    // Returns true if line is valid JSON with an "op" field, setting outOp.
    static bool TryParseOp(const std::string& line, std::string& outOp) {
        if (line.empty() || line[0] != '{') return false;
        try {
            json j = json::parse(line);
            if (j.is_object() && j.contains("op") && j["op"].is_string()) {
                outOp = j["op"].get<std::string>();
                return true;
            }
        }
        catch (...) {}
        return false;
    }

    CasRunner::Result CasRunner::ValidateScript(const std::vector<std::string>& lines) {
        std::vector<std::string> scopeStack; // "if", "loop", "retry"
        bool inBlock = false;
        std::string blockName;
        std::string blockNonce;

        for (size_t i = 0; i < lines.size(); ++i) {
            std::string line = lines[i];
            if (line.empty()) continue;

            // Inside block-set mode: only check for end_set terminator
            if (inBlock) {
                std::string op;
                if (TryParseOp(line, op) && op == "flow.end_set") {
                    // Parse full JSON to check name/nonce match
                    try {
                        json j = json::parse(line);
                        std::string eName = j.value("name", "");
                        std::string eNonce = j.value("nonce", "");
                        if (eName == blockName && eNonce == blockNonce) {
                            inBlock = false;
                            continue;
                        }
                    }
                    catch (...) {}
                }
                // Raw text line inside block — skip validation
                continue;
            }

            // Non-empty, non-block line: must be valid JSON
            std::string op;
            if (!TryParseOp(line, op)) {
                return { false, "Line " + std::to_string(i + 1) +
                    ": E1004 E_JSON_INVALID (must be a JSON object with 'op')", (int)i + 1, X::Value(), "final" };
            }

            // Check for block-set start
            if (op == "flow.set") {
                try {
                    json j = json::parse(line);
                    if (j.contains("mode") && j["mode"].get<std::string>() == "block") {
                        if (!j.contains("nonce") || !j["nonce"].is_string()) {
                            return { false, "Line " + std::to_string(i + 1) +
                                ": E2101 E_ARG_MISSING: block mode requires 'nonce'", (int)i + 1, X::Value(), "final" };
                        }
                        blockName = j.value("name", "");
                        blockNonce = j["nonce"].get<std::string>();
                        inBlock = true;
                        continue;
                    }
                }
                catch (...) {}
            }

            // Scope tracking for flow control
            if (op == "flow.loop_start") scopeStack.push_back("loop");
            else if (op == "flow.if") scopeStack.push_back("if");
            else if (op == "flow.retry_start") scopeStack.push_back("retry");
            else if (op == "flow.loop_end") {
                if (scopeStack.empty() || scopeStack.back() != "loop")
                    return { false, "Line " + std::to_string(i + 1) + ": E2301 Unexpected flow.loop_end", (int)i + 1, X::Value(), "final" };
                scopeStack.pop_back();
            }
            else if (op == "flow.retry_end") {
                if (scopeStack.empty() || scopeStack.back() != "retry")
                    return { false, "Line " + std::to_string(i + 1) + ": E2310 Unexpected flow.retry_end", (int)i + 1, X::Value(), "final" };
                scopeStack.pop_back();
            }
            else if (op == "flow.endif") {
                if (scopeStack.empty() || scopeStack.back() != "if")
                    return { false, "Line " + std::to_string(i + 1) + ": E2301 Unexpected flow.endif", (int)i + 1, X::Value(), "final" };
                scopeStack.pop_back();
            }
            else if (op == "flow.else") {
                 if (scopeStack.empty() || scopeStack.back() != "if")
                    return { false, "Line " + std::to_string(i + 1) + ": E2301 Unexpected flow.else (not in if)", (int)i + 1, X::Value(), "final" };
            }
        }

        if (inBlock) {
            return { false, "E2301 Unclosed block-set for variable '" + blockName + "'", (int)lines.size(), X::Value(), "final" };
        }

        if (!scopeStack.empty()) {
            return { false, "Unclosed scope: " + scopeStack.back(), (int)lines.size(), X::Value(), "final" };
        }

        return { true, "", -1, X::Value(), "final" };
    }

    // Helper: Extract "op" from a line by JSON parse.
    // Returns empty string if not a valid JSON command line.
    static std::string GetOp(const std::string& line) {
        std::string op;
        TryParseOp(line, op);
        return op;
    }

    // Helper: Find matching end block (supports nesting)
    // blockType: "if" or "loop" or "retry"
    size_t CasRunner::FindBlockEnd(const std::vector<std::string>& lines, size_t startLine, const std::string& blockType) {
        int depth = 1;
        std::string startOp, endOp;
        if (blockType == "loop") { startOp = "flow.loop_start"; endOp = "flow.loop_end"; }
        else if (blockType == "retry") { startOp = "flow.retry_start"; endOp = "flow.retry_end"; }
        else { startOp = "flow.if"; endOp = "flow.endif"; }

        for (size_t i = startLine + 1; i < lines.size(); ++i) {
            std::string op = GetOp(lines[i]);
            if (op == startOp) {
                depth++;
            } else if (op == endOp) {
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
            std::string op = GetOp(lines[i]);
            if (op == "flow.if") {
                depth++;
            } else if (op == "flow.endif") {
                depth--;
                if (depth == 0) return i;
            } else if (op == "flow.else") {
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
        m_ctx.return_to = "final";
        m_ctx._last = X::Value();
        m_ctx.logs.clear();
        m_ctx.externalHandler = m_externalHandler;

        // 1. Split into lines (keep both raw and trimmed)
        std::vector<std::string> lines;     // trimmed (for commands)
        std::vector<std::string> rawLines;  // original (for block-set)
        std::istringstream iss(script);
        std::string l;
        while (std::getline(iss, l)) {
             // Strip only trailing \r\n for raw
             std::string raw = l;
             while (!raw.empty() && (raw.back() == '\r' || raw.back() == '\n'))
                 raw.pop_back();
             rawLines.push_back(raw);

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

        // Block-set state
        bool inBlock = false;
        std::string blockVarName;
        std::string blockNonce;
        std::string blockAccum;

        // 3. PC Loop
        size_t pc = 0;
        while (pc < lines.size()) {
            m_ctx.current_line = (int)pc + 1;
            std::string line = lines[pc];

            // Skip empty lines
            if (line.empty()) { pc++; continue; }

            // --- Block-set mode: collect raw text lines ---
            if (inBlock) {
                // Check if this line is the block terminator
                std::string termOp;
                if (TryParseOp(line, termOp) && termOp == "flow.end_set") {
                    try {
                        json j = json::parse(line);
                        std::string eName = j.value("name", "");
                        std::string eNonce = j.value("nonce", "");
                        if (eName == blockVarName && eNonce == blockNonce) {
                            // End of block — store accumulated text
                            // Remove trailing newline if present
                            if (!blockAccum.empty() && blockAccum.back() == '\n') {
                                blockAccum.pop_back();
                            }
                            // Unescape literal \n sequences to real newlines.
                            // Handles the case where the LLM produces literal
                            // "\n" (two chars) instead of real newlines.
                            {
                                std::string unescaped;
                                unescaped.reserve(blockAccum.size());
                                for (size_t ci = 0; ci < blockAccum.size(); ++ci) {
                                    if (blockAccum[ci] == '\\' && ci + 1 < blockAccum.size()) {
                                        char next = blockAccum[ci + 1];
                                        if (next == 'n')  { unescaped += '\n'; ++ci; continue; }
                                        if (next == '\\') { unescaped += '\\'; ++ci; continue; }
                                    }
                                    unescaped += blockAccum[ci];
                                }
                                blockAccum = std::move(unescaped);
                            }
                            m_ctx.vars[blockVarName] = X::Value(blockAccum);
                            inBlock = false;
                            blockAccum.clear();
                            pc++;
                            continue;
                        }
                    }
                    catch (...) {}
                }
                // Not a terminator — accumulate as raw text (preserve indentation)
                blockAccum += rawLines[pc] + "\n";
                pc++;
                continue;
            }

            // Non-JSON lines are skipped (blank lines already handled)
            if (line[0] != '{') { pc++; continue; }

            std::string ns, cmd, err;
            std::unordered_map<std::string, X::Value> args;
            if (!ParseLine(line, ns, cmd, args, err)) {
                 // Should be caught by validation, but just in case
                 return { false, "Runtime Parsing Error: " + err, (int)pc + 1, X::Value(), "final" };
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
                                 // Check for variable-based index: ${varName}
                                 if (inner.size() > 3 && inner.front() == '$' && inner[1] == '{' && inner.back() == '}') {
                                     std::string idxVarName = inner.substr(2, inner.size() - 3);
                                     if (m_ctx.vars.count(idxVarName)) {
                                         X::Value idxVal = m_ctx.vars[idxVarName];
                                         if (idxVal.isNumber()) {
                                             listIdx = (long long)idxVal;
                                             accessType = 2;
                                         } else if (idxVal.isString()) {
                                             keyName = idxVal.asString();
                                             accessType = 1;
                                         }
                                     }
                                 } else {
                                     try {
                                         listIdx = std::stoll(inner);
                                         accessType = 2;
                                     } catch(...) {}
                                 }
                             }
                        }

                        if (accessType > 0) {
                            if (!m_ctx.vars.count(varName)) {
                                return { false, "E2201 E_VAR_UNDEFINED: " + varName, (int)pc + 1, X::Value(), "final" };
                            }
                            X::Value base = m_ctx.vars[varName];
                            
                            if (accessType == 1) { // Dict
                                if (!base.IsDict()) return { false, "E2201 E_VAR_TYPE_ERROR: " + varName + " is not a dict", (int)pc + 1, X::Value(), "final" };
                                X::Dict d(base);
                                if (d->Has(keyName.c_str())) {
                                    X::Value v = d[keyName.c_str()];
                                    if (v.isString()) { s = v.asString(); }
                                    else { kv.second = v; continue; }
                                }
                                else return { false, "E2206 E_INDEX_KEY_NOT_FOUND: Key '" + keyName + "' not found in " + varName, (int)pc + 1, X::Value(), "final" };
                            }
                            else if (accessType == 2) { // List
                                if (!base.IsList()) return { false, "E2201 E_VAR_TYPE_ERROR: " + varName + " is not a list", (int)pc + 1, X::Value(), "final" };
                                X::List l(base);
                                if (listIdx >= 0 && listIdx < l.Size()) {
                                    X::Value v = l[(int)listIdx];
                                    if (v.isString()) { s = v.asString(); }
                                    else { kv.second = v; continue; }
                                }
                                else return { false, "E2207 E_INDEX_OUT_OF_RANGE: Index " + std::to_string(listIdx) + " out of bounds for " + varName, (int)pc + 1, X::Value(), "final" };
                            }
                        }
                        else {
                            if (varName == "_last") {
                                if (m_ctx._last.IsValid()) {
                                    if (m_ctx._last.isString()) { s = m_ctx._last.asString(); }
                                    else { kv.second = m_ctx._last; continue; }
                                }
                            }
                            else if (m_ctx.vars.count(varName)) {
                                X::Value v = m_ctx.vars[varName];
                                if (v.isString()) { s = v.asString(); }
                                else { kv.second = v; continue; }
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
                                      // Check for variable-based index: ${varName}
                                      if (inner.size() > 3 && inner.front() == '$' && inner[1] == '{' && inner.back() == '}') {
                                          std::string idxVarName = inner.substr(2, inner.size() - 3);
                                          if (m_ctx.vars.count(idxVarName)) {
                                              X::Value idxVal = m_ctx.vars[idxVarName];
                                              if (idxVal.isNumber()) {
                                                  listIdx = (long long)idxVal;
                                                  accessType = 2;
                                              } else if (idxVal.isString()) {
                                                  keyName = idxVal.asString();
                                                  accessType = 1;
                                              }
                                          }
                                      } else {
                                          try {
                                              listIdx = std::stoll(inner);
                                              accessType = 2;
                                          } catch(...) {}
                                      }
                                  }
                            }

                            std::string valStr = "null";
                            if (accessType > 0) {
                                 if (m_ctx.vars.count(varName)) {
                                     X::Value base = m_ctx.vars[varName];
                                     if (accessType == 1 && base.IsDict()) {
                                         X::Dict d(base);
                                         if (d->Has(keyName.c_str())) valStr = d[keyName.c_str()].ToString();
                                          else return { false, "E2206 E_INDEX_KEY_NOT_FOUND: Key '" + keyName + "' not found", (int)pc + 1, X::Value(), "final" };
                                     }
                                     else if (accessType == 2 && base.IsList()) {
                                         X::List l(base);
                                         if (listIdx >= 0 && listIdx < l.Size()) valStr = l[(int)listIdx].ToString();
                                          else return { false, "E2207 E_INDEX_OUT_OF_RANGE: Index " + std::to_string(listIdx) + " out of bounds", (int)pc + 1, X::Value(), "final" };
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

            // CasLang header line — no-op, just skip
            if (ns == "caslang") {
                pc++; continue;
            }
            else if (ns == "flow") {
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
                            // Parse JSON string for List/Dict
                            else if (sVal.size() >= 2 && (sVal.front() == '[' || sVal.front() == '{')) {
                                // Optimization: Direct creation for empty structures
                                if (sVal == "[]") {
                                    val = X::Value(X::g_pXHost->CreateList());
                                }
                                else if (sVal == "{}") {
                                    val = X::Value(X::g_pXHost->CreateDict());
                                }
                                else {
                                    // Try to parse as JSON
                                    X::Runtime rt;
                                    X::Package json(rt, "json", "");
                                    try {
                                        X::Value parsed = json["loads"](sVal);
                                        if (parsed.IsList() || parsed.IsDict()) {
                                            val = parsed;
                                        }
                                    } catch (...) {
                                        // Ignore parse errors, treat as string
                                    }
                                }
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
                    // Check for block-set mode
                    else if (args.count("name") && args.count("mode")) {
                        std::string mode = args["mode"].asString();
                        if (mode == "block") {
                            if (!args.count("nonce")) {
                                return { false, "E2101 E_ARG_MISSING: block mode requires 'nonce'", (int)pc + 1, X::Value(), "final" };
                            }
                            blockVarName = args["name"].asString();
                            blockNonce = args["nonce"].asString();
                            inBlock = true;
                            blockAccum.clear();
                        }
                    }
                }
                else if (cmd == "end_set") {
                    // This should only be reached if not in block mode (mismatched)
                    // Block termination is handled above in the block collection loop
                    return { false, "E2301 E_BLOCK_UNBALANCED: flow.end_set without matching block start", (int)pc + 1, X::Value(), "final" };
                }
                else if (cmd == "if") {
                    std::string cond = args["cond"].asString();
                    if (!EvaluateCond(cond)) {
                        size_t nextPt = FindElseOrEndif(lines, pc);
                        pc = nextPt; 
                        if (pc < lines.size() && GetOp(lines[pc]) == "flow.else") {
                             pc++; continue; 
                        }
                    }
                }
                else if (cmd == "else") {
                    size_t i = pc + 1;
                    int depth = 1; 
                    while(i < lines.size()) {
                        std::string op = GetOp(lines[i]);
                        if (op == "flow.if") depth++;
                        else if (op == "flow.endif") {
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
                             errMsg = "loop_start: 'in' must be a JSON list found:" + listJson;
                             isErr = true;
                         }
                     }

                     if (!isErr && listVal.IsList()) {
                         X::List list(listVal);
                         long long size = list.Size();
                         
                         // Handle optional 'index' variable
                         std::string indexVarName;
                         if (args.count("index")) {
                             indexVarName = args["index"].asString();
                         }
                         
                         // Handle optional 'from' and 'limit'
                         long long fromIdx = 0;
                         long long limit = -1;
                         if (args.count("from")) {
                             fromIdx = args["from"].isNumber() ? (long long)args["from"] : 0;
                         }
                         if (args.count("limit")) {
                             limit = args["limit"].isNumber() ? (long long)args["limit"] : -1;
                         }
                         
                         // Check if we are re-entering (iterating)
                         if (!loopStack.empty() && loopStack.back().first == pc) {
                             // Increment
                             loopStack.back().second++;
                         } else {
                             // New Loop — start from 'from' index
                             loopStack.push_back({pc, (size_t)fromIdx});
                         }

                         long long idx = (long long)loopStack.back().second;
                         
                         // Check limit
                         long long iterationsFromStart = idx - fromIdx;
                         bool withinLimit = (limit < 0) || (iterationsFromStart < limit);
                         
                         if (idx < size && withinLimit) {
                             m_ctx.vars[varName] = list[idx];
                             // Set index variable if specified
                             if (!indexVarName.empty()) {
                                 m_ctx.vars[indexVarName] = X::Value(idx);
                             }
                             // Proceed to next line (body)
                         } else {
                             // Used up
                             loopStack.pop_back();
                             // Skip to end
                             size_t end = FindBlockEnd(lines, pc, "loop");
                             pc = end; 
                             continue;
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
                else if (cmd == "continue") {
                     if (!loopStack.empty()) {
                         size_t start = loopStack.back().first;
                         pc = start; // Jump back to loop_start to re-evaluate/increment
                         continue;
                     }
                }
                else if (cmd == "retry_start") {
                    int count = 3;
                    int delay = 1000;
                    // Support both old "count"/"delay" and new "times"/"backoff_ms" arg names
                    if (args.count("times")) count = args["times"].isNumber() ? (int)args["times"] : std::stoi(args["times"].asString());
                    else if (args.count("count")) count = args["count"].isNumber() ? (int)args["count"] : std::stoi(args["count"].asString());
                    if (args.count("backoff_ms")) delay = args["backoff_ms"].isNumber() ? (int)args["backoff_ms"] : std::stoi(args["backoff_ms"].asString());
                    else if (args.count("delay")) delay = args["delay"].isNumber() ? (int)args["delay"] : std::stoi(args["delay"].asString());
                    
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
                    m_ctx.return_to = args.count("to") ? args["to"].asString() : "final";
                    return { true, "", -1, m_ctx.return_value, m_ctx.return_to };
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
                            errMsg = errs[0] + " | line: " + line;
                        }
                        if (args.count("as")) {
                            m_ctx.vars[args["as"].asString()] = m_ctx._last;
                        }
                        if (args.count("return") && args["return"].IsTrue()) {
                             m_ctx.return_value = m_ctx._last;
                             m_ctx.return_flag = true;
                             return { true, "", -1, m_ctx._last, "final" };
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
                        // v0.3 tool.call: args contains flat keys like
                        //   {"name":"seek_by_text","input_text":"...","as":"result","timeout_ms":30000}
                        // CasFilter::ExecuteExternalTool expects:
                        //   args["name"] = tool name
                        //   args["args"] = X::Dict of tool parameters
                        //   args["timeout_ms"] = optional timeout
                        // So we strip reserved keys and pack the rest into an X::Dict.

                        // Save reserved values before rebuilding
                        std::string asVar;
                        if (args.count("as")) asVar = args["as"].asString();

                        // Reserved keys that must NOT be forwarded as tool parameters
                        static const std::unordered_set<std::string> reservedKeys = {
                            "op", "name", "as", "timeout_ms", "target"
                        };

                        // Pack non-reserved keys into a dict for the tool
                        X::Dict toolArgs;
                        for (auto& kv : args) {
                            if (reservedKeys.count(kv.first)) continue;
                            toolArgs->Set(kv.first.c_str(), kv.second);
                        }

                        // Rebuild args map for the external handler interface
                        std::unordered_map<std::string, X::Value> handlerArgs;
                        if (args.count("name"))       handlerArgs["name"] = args["name"];
                        if (args.count("timeout_ms")) handlerArgs["timeout_ms"] = args["timeout_ms"];
                        handlerArgs["args"] = toolArgs;

                        m_ctx._last = m_externalHandler(ns, cmd, handlerArgs, m_ctx.metaData);
                        if (!asVar.empty()) {
                            m_ctx.vars[asVar] = m_ctx._last;
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
                    errMsg = "E2001 E_OP_UNKNOWN: Unknown namespace: " + ns;
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
                        return { false, errMsg, (int)pc + 1, X::Value(), "final" };
                    }
                }
                else {
                    LogError(errMsg);
                    return { false, errMsg, (int)pc + 1, X::Value(), "final" };
                }
            }

            pc++;
        }
        
        return { true, "", -1, m_ctx._last, m_ctx.return_to };
    }
}
