#pragma once
#include <unordered_map>
#include <memory>
#include <regex>
#include <string>
#include <vector>
#include <cstdio>
#include <sstream>
#include <cctype>
#include <stdexcept>
#include "xlang.h"
#include "ActionOps.h"
#include "ActionParser.h"
#include "log.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

//============================= ActionRunner =============================
class ActionRunner {
public:
	struct Result {
		X::Value output;                                   // final output (last or explicit return)
		std::vector<std::string> errors;
		std::unordered_map<std::string, X::Value> vars;   // snapshot
	};

	// Register an Ops provider (e.g., fs, http, search, amazonpage, ...)
	void Register(std::unique_ptr<ActionOps> ops) {
		const std::string& ns = ops->Namespace();
		ops_.emplace(ns, std::move(ops));
	}

	// Variables API
	void SetVar(const std::string& name, const X::Value& v) { ctx_.set(name, v); }
	bool HasVar(const std::string& name) const { return ctx_.has(name); }
	X::Value GetVar(const std::string& name) const { return ctx_.get(name); }

	// Optional: LLM usage helpers
	std::string LLMUsagePromptAll() const {
		std::ostringstream os;
		for (const auto& kv : ops_) os << kv.second->BuildLLMUsagePrompt() << "\n";
		return os.str();
	}
	std::string LLMUsagePromptFor(const std::string& ns) const {
		auto it = ops_.find(ns);
		return (it == ops_.end()) ? std::string{} : it->second->BuildLLMUsagePrompt();
	}

	// Run a mixed prompt with commands and prose.
	Result Run(const std::string& prompt) {
		LOG7 << "ActionRunner::Run: prompt \r\n\r\n" << LOG_GREEN << prompt <<LOG_RESET<<"\r\n" << LINE_END;
		Result res;
		ActionParser ex;
		auto flat = ex.Extract(prompt);

		// 1) Compile flat commands into a block tree
		std::vector<NodePtr> program;
		if (!compileBlocks(flat, program, res.errors)) {
			// structural error => early return
			res.output = X::Value();
			res.vars = ctx_.vars;
			return res;
		}

		// 2) Execute
		ExecState st;
		st.max_loop_iters = 10000; // safety guard; tune if needed
		auto r = execList(program, st, res.errors);
		if (r.signal == Signal::Return) {
			res.output = r.value;
		}
		else {
			res.output = st.last; // last command¡¯s output (could be null)
		}
		res.vars = ctx_.vars;
		return res;
	}

private:
	//============================= AST Nodes =============================
	enum class NType { Cmd, If, Loop, Break, Continue, Return };
	struct Node {
		NType t;
		virtual ~Node() = default;
	};
	using NodePtr = std::unique_ptr<Node>;

	struct CmdNode : Node {
		ParsedAction cmd;
		CmdNode(const ParsedAction& c) { t = NType::Cmd; cmd = c; }
	};

	struct IfNode : Node {
		std::string cond;                  // raw condition string
		std::vector<NodePtr> thenPart;
		std::vector<NodePtr> elsePart;
		IfNode() { t = NType::If; }
	};

	struct LoopNode : Node {
		std::string varName;               // "var"
		std::string indexName;             // "index" (optional)
		std::string inExpr;                // "in" (string; JSON array or delimited list), may contain ${}
		int64_t from = 0;
		int64_t limit = -1;
		std::vector<NodePtr> body;
		LoopNode() { t = NType::Loop; }
	};

	struct BreakNode : Node { BreakNode() { t = NType::Break; } };
	struct ContinueNode : Node { ContinueNode() { t = NType::Continue; } };
	struct ReturnNode : Node {
		std::string valueExpr;  // optional; if empty => return _last
		ReturnNode() { t = NType::Return; }
	};

	//=========================== Compile (flat -> tree) ===========================
	struct Frame {
		enum Kind { ROOT, IF, LOOP } kind;
		IfNode* ifn = nullptr;
		LoopNode* loop = nullptr;
		std::vector<NodePtr>* curList = nullptr;
		bool elseSeen = false;
	};

	bool compileBlocks(const std::vector<ParsedAction>& flat,
		std::vector<NodePtr>& out,
		std::vector<std::string>& errs)
	{
		std::vector<NodePtr>* cur = &out;
		std::vector<Frame> stack;
		stack.push_back(Frame{ Frame::ROOT, nullptr, nullptr, cur, false });

		auto pushNode = [&](NodePtr n) {
			stack.back().curList->push_back(std::move(n));
			};

		for (const auto& c : flat) {
			// Flow control markers are recognized by Runner
			if (!c.ns.empty() && c.ns[0] == "flow") {
				const std::string& name = c.command;
				if (equalsIgnoreCase(name, "LoopStart")) {
					auto ln = std::make_unique<LoopNode>();
					ln->varName = getArgStr(c.args, "var", "");
					ln->indexName = getArgStr(c.args, "index", "");
					ln->inExpr = getArgStr(c.args, "in", "");
					ln->from = (int64_t)getArgNum(c.args, "from", 0);
					ln->limit = (int64_t)getArgNum(c.args, "limit", -1);
					if (ln->varName.empty() || ln->inExpr.empty()) {
						errs.push_back("LoopStart requires 'var' and 'in'");
						return false;
					}
					auto ptr = ln.get();
					pushNode(std::move(ln));
					stack.push_back(Frame{ Frame::LOOP, nullptr, ptr, &ptr->body, false });
					continue;
				}
				if (equalsIgnoreCase(name, "LoopEnd")) {
					// pop LOOP
					if (stack.size() <= 1 || stack.back().kind != Frame::LOOP) {
						errs.push_back("LoopEnd without matching LoopStart");
						return false;
					}
					stack.pop_back();
					continue;
				}
				if (equalsIgnoreCase(name, "If")) {
					auto in = std::make_unique<IfNode>();
					in->cond = getArgStr(c.args, "cond", "");
					if (in->cond.empty()) {
						errs.push_back("If requires 'cond'");
						return false;
					}
					auto ptr = in.get();
					pushNode(std::move(in));
					stack.push_back(Frame{ Frame::IF, ptr, nullptr, &ptr->thenPart, false });
					continue;
				}
				if (equalsIgnoreCase(name, "Else")) {
					if (stack.size() <= 1 || stack.back().kind != Frame::IF || stack.back().elseSeen) {
						errs.push_back("Else without matching If (or duplicate Else)");
						return false;
					}
					stack.back().elseSeen = true;
					stack.back().curList = &stack.back().ifn->elsePart;
					continue;
				}
				if (equalsIgnoreCase(name, "EndIf")) {
					if (stack.size() <= 1 || stack.back().kind != Frame::IF) {
						errs.push_back("EndIf without matching If");
						return false;
					}
					stack.pop_back();
					continue;
				}
				if (equalsIgnoreCase(name, "Break")) {
					pushNode(std::make_unique<BreakNode>());
					continue;
				}
				if (equalsIgnoreCase(name, "Continue")) {
					pushNode(std::make_unique<ContinueNode>());
					continue;
				}
				if (equalsIgnoreCase(name, "Return")) {
					auto rn = std::make_unique<ReturnNode>();
					rn->valueExpr = getArgStr(c.args, "value", "");
					pushNode(std::move(rn));
					continue;
				}
				// Other flow.* (set/get/exists...) are executed as normal commands
			}

			// regular command node
			pushNode(std::make_unique<CmdNode>(c));
		}

		// all blocks must be closed
		if (stack.size() != 1) {
			errs.push_back("Unclosed block(s): missing LoopEnd or EndIf");
			return false;
		}
		return true;
	}

	//============================ Exec state / signals ============================
	enum class Signal { None, Continue, Break, Return };
	struct ExecResult {
		Signal   signal;
		X::Value value;

		ExecResult()                         // default used by `return {};`
			: signal(Signal::None), value(X::Value()) {
		}

		ExecResult(Signal s, const X::Value& v)
			: signal(s), value(v) {
		}
	};

	struct SavedVar {
		std::string name;
		bool had{ false };
		X::Value old;
	};

	struct ExecState {
		X::Value last = X::Value();
		int max_loop_iters = 10000;
	};

	//=============================== Execution ===================================
	ExecResult execList(std::vector<NodePtr>& list, ExecState& st, std::vector<std::string>& errs) {
		LOG7 << "execList: " << list.size() << " nodes" << LINE_END;
		for (auto& n : list) {
			auto r = execNode(*n, st, errs);
			if (r.signal == Signal::Continue || r.signal == Signal::Break || r.signal == Signal::Return)
				return r;
		}
		return {};
	}

	ExecResult execNode(Node& n, ExecState& st, std::vector<std::string>& errs) {
		switch (n.t) {
		case NType::Cmd:      return execCmd(static_cast<CmdNode&>(n), st, errs);
		case NType::If:       return execIf(static_cast<IfNode&>(n), st, errs);
		case NType::Loop:     return execLoop(static_cast<LoopNode&>(n), st, errs);
		case NType::Break:    return ExecResult{ Signal::Break, X::Value() };
		case NType::Continue: return ExecResult{ Signal::Continue, X::Value() };
		case NType::Return:   return execReturn(static_cast<ReturnNode&> (n), st);
		}
		return {};
	}

	ExecResult execReturn(ReturnNode& rn, ExecState& st) {
		if (rn.valueExpr.empty()) {
			return { Signal::Return, st.last };
		}
		std::string v = interpolate(rn.valueExpr);
		// Try to parse bool/number; else keep as string
		if (ieq(v, "true"))  return { Signal::Return, X::Value(true) };
		if (ieq(v, "false")) return { Signal::Return, X::Value(false) };
		// number?
		char* e = nullptr;
		double d = std::strtod(v.c_str(), &e);
		if (e && *e == '\0' && !v.empty()) return { Signal::Return, X::Value(d) };
		return { Signal::Return, X::Value(v) };
	}

	ExecResult execIf(IfNode& in, ExecState& st, std::vector<std::string>& errs) {
		bool cond = evalCond(in.cond, errs);
		if (!errs.empty()) return {};
		if (cond) return execList(in.thenPart, st, errs);
		else      return execList(in.elsePart, st, errs);
	}

	ExecResult execLoop(LoopNode& ln, ExecState& st, std::vector<std::string>& errs) {
		// Resolve iterable
		std::string listStr = interpolate(ln.inExpr);
		auto items = parseArrayLike(listStr);

		size_t start = (ln.from < 0) ? 0 : (size_t)ln.from;
		if (start > items.size()) return {};
		if (ln.limit >= 0 && (size_t)ln.limit < items.size() - start)
			items.resize(start + (size_t)ln.limit);

		// Save shadowed vars
		SavedVar savedVar{ ln.varName, ctx_.has(ln.varName), ctx_.get(ln.varName) };
		SavedVar savedIdx;
		if (!ln.indexName.empty())
			savedIdx = SavedVar{ ln.indexName, ctx_.has(ln.indexName), ctx_.get(ln.indexName) };

		ExecResult r;
		int iterCount = 0;
		for (size_t i = start; i < items.size(); ++i) {
			if (++iterCount > st.max_loop_iters) { errs.push_back("Loop exceeded max iterations"); break; }

			ctx_.set(ln.varName, X::Value(items[i]));
			if (!ln.indexName.empty())
				ctx_.set(ln.indexName, X::Value((int64_t)i));

			r = execList(ln.body, st, errs);
			if (r.signal == Signal::Return) return r;
			if (r.signal == Signal::Break) { r.signal = Signal::None; break; }
			if (r.signal == Signal::Continue) { r.signal = Signal::None; continue; }
			// else continue
		}

		// restore shadowed vars
		if (savedVar.had) ctx_.set(savedVar.name, savedVar.old);
		// if it didn't exist before, leave current loop var as-is or remove?
		else ctx_.vars.erase(savedVar.name);

		if (!ln.indexName.empty()) {
			if (savedIdx.had) ctx_.set(savedIdx.name, savedIdx.old);
			else ctx_.vars.erase(savedIdx.name);
		}

		return {};
	}

	ExecResult execCmd(CmdNode& c, ExecState& st, std::vector<std::string>& errs) {
		LOG7 << "execCmd: #" << (c.cmd.ns.empty() ? "" : (c.cmd.ns[0] + ".")) << c.cmd.command << LINE_END;
		// Copy args, interpolate strings
		auto args = c.cmd.args;
		for (auto& kv : args) {
			if (kv.second.isString()) kv.second = X::Value(interpolate(kv.second.asString()));
		}

		// Control fields
		std::string asVar;
		bool wantReturn = false;
		auto itAs = args.find("as");
		if (itAs != args.end() && itAs->second.isString()) { asVar = itAs->second.asString(); args.erase(itAs); }
		auto itRet = args.find("return");
		if (itRet != args.end()) { wantReturn = itRet->second.isBool() ? itRet->second.asBool() : false; args.erase(itRet); }

		X::Value out = X::Value();

		// Built-in flow.* (set/get/exists) for convenience
		if (!c.cmd.ns.empty() && c.cmd.ns[0] == "flow") {
			const auto& cmd = c.cmd.command;
			if (equalsIgnoreCase(cmd, "set")) {
				std::string name = getArgStr(args, "name", "");
				if (name.empty()) { errs.push_back("flow.set requires 'name'"); }
				else {
					auto it = args.find("value");
					if (it == args.end()) { errs.push_back("flow.set requires 'value'"); }
					else { ctx_.set(name, it->second); out = it->second; }
				}
			}
			else if (equalsIgnoreCase(cmd, "get")) {
				std::string name = getArgStr(args, "name", "");
				if (name.empty()) { errs.push_back("flow.get requires 'name'"); }
				else out = ctx_.get(name);
			}
			else if (equalsIgnoreCase(cmd, "exists")) {
				std::string name = getArgStr(args, "name", "");
				out = X::Value(ctx_.has(name));
			}
			else {
				// Other flow.* go through Ops if registered
				out = dispatch(c.cmd.ns, c.cmd.command, args, errs);
			}
		}
		else {
			out = dispatch(c.cmd.ns, c.cmd.command, args, errs);
		}

		st.last = out;
		ctx_.set("_last", st.last);

		if (!asVar.empty()) ctx_.set(asVar, out);
		if (wantReturn) return { Signal::Return, out };
		return {};
	}

	//============================= Dispatch to Ops ================================
	X::Value dispatch(const std::vector<std::string>& ns,
		const std::string& command,
		const std::unordered_map<std::string, X::Value>& args,
		std::vector<std::string>& errs)
	{
		if (ns.empty()) { errs.push_back("Missing namespace for command: " + command); return X::Value(); }
		const std::string& root = ns.front();
		auto itOp = ops_.find(root);
		if (itOp == ops_.end()) { errs.push_back("Unknown namespace: #" + root + "." + command); return X::Value(); }
		// const_cast because Execute signature expects non-const args (some Ops mutate a copy)
		auto a = args;
		return itOp->second->Execute(ns, command, a, ctx_, errs);
	}

	//=============================== Utilities ====================================
	// ${var} interpolation (strings only)
	std::string interpolate(const std::string& s) const {
		static const std::regex pattern(R"(\$\{([A-Za-z_][A-Za-z0-9_\-]*)\})");
		std::string out; out.reserve(s.size() + 16);
		std::sregex_iterator it(s.begin(), s.end(), pattern), end;
		size_t lastPos = 0;
		for (; it != end; ++it) {
			size_t pos = (size_t)it->position(), len = (size_t)it->length();
			out.append(s, lastPos, pos - lastPos);
			std::string name = (*it)[1].str();
			out += varAsString(name);
			lastPos = pos + len;
		}
		out.append(s, lastPos, std::string::npos);
		return out;
	}
	std::string varAsString(const std::string& name) const {
		auto v = ctx_.get(name);
		if (v.isString()) return v.asString();
		if (v.isBool())   return v.asBool() ? "true" : "false";
		if (v.isNumber()) { char buf[64]; std::snprintf(buf, sizeof(buf), "%.15g", v.asNumber()); return std::string(buf); }
		return ""; // null -> empty
	}

	static bool equalsIgnoreCase(const std::string& a, const std::string& b) {
		if (a.size() != b.size()) return false;
		for (size_t i = 0; i < a.size(); ++i)
			if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) return false;
		return true;
	}
	static bool ieq(const std::string& a, const std::string& b) {
		return equalsIgnoreCase(a, b);
	}
	static std::string getArgStr(const std::unordered_map<std::string, X::Value>& a,
		const char* k, const std::string& def) {
		auto it = a.find(k);
		if (it != a.end()) {
			X::Value v = it->second;       // remove constness by value-copy
			if (v.isString()) return v.asString();
		}
		return def;
	}

	static double getArgNum(const std::unordered_map<std::string, X::Value>& a,
		const char* k, double def)
	{
		auto it = a.find(k);
		if (it != a.end()) {
			X::Value v = it->second;      // make a non-const copy
			if (v.isNumber())             // now OK
				return v.asNumber();
		}
		return def;
	}


	static std::vector<std::string> parseArrayLike(const std::string& s) {
		std::vector<std::string> out;
		if (s.empty()) return out;
		try {
			json j = json::parse(s);
			if (j.is_array()) {
				for (auto& v : j) out.push_back(v.is_string() ? v.get<std::string>() : v.dump());
				return out;
			}
		}
		catch (...) { /* not JSON */ }
		// Fallback: split by newline or comma
		std::string cur; cur.reserve(64);
		auto flush = [&]() { if (!cur.empty()) { trimInPlace(cur); if (!cur.empty()) out.push_back(cur); cur.clear(); } };
		for (char c : s) { if (c == '\n' || c == ',') flush(); else cur.push_back(c); }
		flush();
		return out;
	}
	static void trimInPlace(std::string& x) {
		size_t a = 0, b = x.size();
		while (a < b && std::isspace((unsigned char)x[a])) ++a;
		while (b > a && std::isspace((unsigned char)x[b - 1])) --b;
		x = x.substr(a, b - a);
	}

	//========================= Condition Expression Eval ==========================
	// Grammar:
	//   expr  := or
	//   or    := and ( '||' and )*
	//   and   := cmp ( '&&' cmp )*
	//   cmp   := sum ( ('=='|'!='|'<'|'<='|'>'|'>=') sum )?
	//   sum   := unary  ( '+' unary | '-' unary )*   (we don't need arithmetic; kept for numeric literals only)
	//   unary := '!' unary | primary
	//   primary := NUMBER | STRING | TRUE | FALSE | '(' expr ')' | VAR
	// VAR is ${name} expanded into a token before parsing (we pass the raw string and expand during tokenization).
	//
	// Comparison: numeric if both sides parse as numbers; else lexicographic (case-sensitive).
	//
	bool evalCond(const std::string& raw, std::vector<std::string>& errs) {
		try {
			tokenizeCond(raw);
			pos_ = 0;
			bool v = parseOr();
			if (pos_ != toks_.size()) throw std::runtime_error("Unexpected token after expression");
			return v;
		}
		catch (const std::exception& e) {
			errs.push_back(std::string("If.cond parse error: ") + e.what());
			return false;
		}
	}

	enum TKind { TK_END, TK_LP, TK_RP, TK_AND, TK_OR, TK_NOT, TK_EQ, TK_NE, TK_LT, TK_LE, TK_GT, TK_GE, TK_NUM, TK_STR, TK_TRUE, TK_FALSE };
	struct Tok { TKind k; std::string s; double d = 0; };

	std::vector<Tok> toks_;
	size_t pos_{ 0 };

	void tokenizeCond(const std::string& raw) {
		toks_.clear();
		std::string s = raw;
		// Expand ${var} to literal tokens: string/number/bool/null-as-empty
		// We do a simple pass: replace ${var} with the var's string form (quoted if string)
		// but simpler: the tokenizer will recognize ${...} and inline-resolve.
		size_t i = 0, n = s.size();
		while (i < n) {
			char c = s[i];
			if (std::isspace((unsigned char)c)) { ++i; continue; }
			if (c == '(') { toks_.push_back({ TK_LP }); ++i; continue; }
			if (c == ')') { toks_.push_back({ TK_RP }); ++i; continue; }
			if (c == '!') {
				if (i + 1 < n && s[i + 1] == '=') { toks_.push_back({ TK_NE }); i += 2; }
				else { toks_.push_back({ TK_NOT }); ++i; }
				continue;
			}
			if (c == '&' && i + 1 < n && s[i + 1] == '&') { toks_.push_back({ TK_AND }); i += 2; continue; }
			if (c == '|' && i + 1 < n && s[i + 1] == '|') { toks_.push_back({ TK_OR });  i += 2; continue; }
			if (c == '=' && i + 1 < n && s[i + 1] == '=') { toks_.push_back({ TK_EQ }); i += 2; continue; }
			if (c == '<') {
				if (i + 1 < n && s[i + 1] == '=') { toks_.push_back({ TK_LE }); i += 2; }
				else { toks_.push_back({ TK_LT }); ++i; }
				continue;
			}
			if (c == '>') {
				if (i + 1 < n && s[i + 1] == '=') { toks_.push_back({ TK_GE }); i += 2; }
				else { toks_.push_back({ TK_GT }); ++i; }
				continue;
			}
			if (c == '"') { // string literal
				++i; std::string out;
				while (i < n && s[i] != '"') {
					if (s[i] == '\\' && i + 1 < n) {
						char e = s[++i];
						switch (e)
						{
						case 'n': out.push_back('\n');
							break;
						case 'r': out.push_back('\r');
							break;
						case 't': out.push_back('\t'); 
							break;
						case '"': out.push_back('"'); 
							break;
						case '\\': out.push_back('\\'); 
							break;
						default: out.push_back(e); 
							break;
						}
						++i; continue;
					}
					out.push_back(s[i++]);
				}
				if (i >= n || s[i] != '"') throw std::runtime_error("Unclosed string");
				++i;
				toks_.push_back({ TK_STR, out });
				continue;
			}
			// ${var}
			if (c == '$' && i + 1 < n && s[i + 1] == '{') {
				size_t j = i + 2;
				while (j < n && s[j] != '}') ++j;
				if (j >= n) throw std::runtime_error("Unclosed ${var}");
				std::string name = s.substr(i + 2, j - (i + 2));
				auto v = ctx_.get(name);
				if (v.isBool()) { toks_.push_back({ v.asBool() ? TK_TRUE : TK_FALSE }); }
				else if (v.isNumber()) { toks_.push_back({ TK_NUM, "", v.asNumber() }); }
				else if (v.isString()) { toks_.push_back({ TK_STR, v.asString() }); }
				else { toks_.push_back({ TK_STR, "" }); }
				i = j + 1; continue;
			}
			// true/false
			if (isIdentStart(c)) {
				size_t j = i + 1;
				while (j < n && isIdentChar(s[j])) ++j;
				std::string id = s.substr(i, j - i);
				if (id == "true")  toks_.push_back({ TK_TRUE });
				else if (id == "false") toks_.push_back({ TK_FALSE });
				else {
					// try number
					char* e = nullptr;
					double d = std::strtod(id.c_str(), &e);
					if (e && *e == '\0') toks_.push_back({ TK_NUM,"",d });
					else toks_.push_back({ TK_STR,id }); // fallback string literal (unquoted)
				}
				i = j; continue;
			}
			// number literal
			if (std::isdigit((unsigned char)c) || (c == '.')) {
				size_t j = i + 1;
				while (j < n && (std::isdigit((unsigned char)s[j]) || s[j] == '.' || s[j] == 'e' || s[j] == 'E' || s[j] == '+' || s[j] == '-')) ++j;
				char* e = nullptr; double d = std::strtod(s.substr(i, j - i).c_str(), &e);
				if (e && *e == '\0') { toks_.push_back({ TK_NUM,"",d }); i = j; continue; }
			}
			throw std::runtime_error(std::string("Unexpected char: ") + c);
		}
		// no TK_END needed; we check pos_==size
	}

	bool parseOr() {
		bool v = parseAnd();
		while (match(TK_OR)) {
			bool r = parseAnd();
			v = v || r;
		}
		return v;
	}
	bool parseAnd() {
		bool v = parseCmp();
		while (match(TK_AND)) {
			bool r = parseCmp();
			v = v && r;
		}
		return v;
	}
	bool parseCmp() {
		auto L = parseUnaryValue();
		if (match(TK_EQ)) { auto R = parseUnaryValue(); return cmp(L, R) == 0; }
		if (match(TK_NE)) { auto R = parseUnaryValue(); return cmp(L, R) != 0; }
		if (match(TK_LT)) { auto R = parseUnaryValue(); return cmp(L, R) < 0; }
		if (match(TK_LE)) { auto R = parseUnaryValue(); return cmp(L, R) <= 0; }
		if (match(TK_GT)) { auto R = parseUnaryValue(); return cmp(L, R) > 0; }
		if (match(TK_GE)) { auto R = parseUnaryValue(); return cmp(L, R) >= 0; }
		// if no comparator, truthiness
		return truthy(L);
	}
	struct V { bool isNum = false; double d = 0; std::string s; bool isBool = false; bool b = false; };
	V parseUnaryValue() {
		if (match(TK_NOT)) {
			auto v = parseUnaryValue();
			return V{ true, truthy(v) ? 0.0 : 1.0, "", true, !truthy(v) };
		}
		if (match(TK_LP)) {
			bool v = parseOr();
			expect(TK_RP);
			return V{ true, v ? 1.0 : 0.0, "", true, v };
		}
		if (peek(TK_TRUE)) { advance(); return V{ true,1.0,"",true,true }; }
		if (peek(TK_FALSE)) { advance(); return V{ true,0.0,"",true,false }; }
		if (peek(TK_NUM)) { auto t = advance(); return V{ true,t.d,"",false,false }; }
		if (peek(TK_STR)) { auto t = advance(); return V{ false,0.0,t.s,false,false }; }
		throw std::runtime_error("Expected value");
	}
	static bool truthy(const V& v) {
		if (v.isBool) return v.b;
		if (v.isNum)  return v.d != 0.0;
		if (!v.s.empty()) {
			std::string t; t.reserve(v.s.size());
			for (char c : v.s) t.push_back((char)std::tolower((unsigned char)c));
			if (t == "0" || t == "false" || t == "no" || t == "off" || t == "null") return false;
			return true;
		}
		return false;
	}
	static int cmp(const V& L, const V& R) {
		if (L.isNum && R.isNum) {
			if (L.d < R.d) return -1;
			if (L.d > R.d) return 1;
			return 0;
		}
		// string compare (case-sensitive)
		if (L.isNum && !R.isNum) {
			char buf[64]; std::snprintf(buf, sizeof(buf), "%.15g", L.d);
			return std::string(buf).compare(R.s);
		}
		if (!L.isNum && R.isNum) {
			char buf[64]; std::snprintf(buf, sizeof(buf), "%.15g", R.d);
			return L.s.compare(std::string(buf));
		}
		return L.s.compare(R.s);
	}
	bool match(TKind k) { if (pos_ < toks_.size() && toks_[pos_].k == k) { ++pos_; return true; } return false; }
	bool peek(TKind k) { return pos_ < toks_.size() && toks_[pos_].k == k; }
	Tok advance() { return toks_[pos_++]; }
	void expect(TKind k) { if (!match(k)) throw std::runtime_error("Expected token missing"); }
	static bool isIdentStart(char c) { return std::isalpha((unsigned char)c) || c == '_'; }
	static bool isIdentChar(char c) { return std::isalnum((unsigned char)c) || c == '_' || c == '-'; }

	//===================== Members ======================
	std::unordered_map<std::string, std::unique_ptr<ActionOps>> ops_;
	ActionContext ctx_;
};
