#pragma once
#include "ActionOps.h"
#include <string>
#include <unordered_map>
#include <algorithm>
#include <regex>
#include <cctype>
#include <cstdio>

// -------------------- StringOps : #str.* --------------------
class StringOps : public ActionOps {
public:
    const std::string& Namespace() const override {
        static std::string k = "str";
        return k;
    }

    X::Value Execute(const std::vector<std::string>& ns_parts,
        const std::string& command,
        std::unordered_map<std::string, X::Value>& args,
        ActionContext& ctx,
        std::vector<std::string>& errs) override
    {
        (void)ns_parts; (void)ctx;

        if (ieq(command, "len"))       return op_len(args, errs);
        if (ieq(command, "trim"))      return op_trim(args, errs);
        if (ieq(command, "lower"))     return op_lower(args, errs);
        if (ieq(command, "upper"))     return op_upper(args, errs);
        if (ieq(command, "contains"))  return op_contains(args, errs);
        if (ieq(command, "find"))      return op_find(args, errs);
        if (ieq(command, "replace"))   return op_replace(args, errs);
        if (ieq(command, "slice"))     return op_slice(args, errs);
        if (ieq(command, "match"))     return op_match(args, errs);

        errs.push_back("str: unknown command: " + command);
        return X::Value(); // null
    }

    std::vector<CommandInfo> DescribeCommands() const override {
        using A = Arg;
        std::vector<CommandInfo> v;

        v.push_back(CommandInfo{
            "len", "Length of string in code units.",
            { A{"s","string",true,"Input string.",""} },
            "number",
            { "#str.len{\"s\":\"${x}\",\"return\":true}" }
            });

        v.push_back(CommandInfo{
            "trim", "Trim leading/trailing whitespace.",
            { A{"s","string",true,"Input string.",""} },
            "string",
            { "#str.trim{\"s\":\"${x}\",\"return\":true}" }
            });

        v.push_back(CommandInfo{
            "lower", "Lowercase.",
            { A{"s","string",true,"Input string.",""} },
            "string",
            { "#str.lower{\"s\":\"${x}\",\"return\":true}" }
            });

        v.push_back(CommandInfo{
            "upper", "Uppercase.",
            { A{"s","string",true,"Input string.",""} },
            "string",
            { "#str.upper{\"s\":\"${x}\",\"return\":true}" }
            });

        v.push_back(CommandInfo{
            "contains", "Substring containment (case-sensitive).",
            { A{"s","string",true,"Haystack.",""}, A{"needle","string",true,"Needle.",""} },
            "bool",
            { "#str.contains{\"s\":\"${buf}\",\"needle\":\"TODO\",\"return\":true}" }
            });

        v.push_back(CommandInfo{
            "find", "0-based index of first occurrence, or -1.",
            { A{"s","string",true,"Haystack.",""}, A{"needle","string",true,"Needle.",""} },
            "number",
            { "#str.find{\"s\":\"${buf}\",\"needle\":\"TODO\",\"return\":true}" }
            });

        v.push_back(CommandInfo{
            "replace", "Replace all occurrences of 'from' with 'to'.",
            {
                A{"s","string",true,"Input string.",""},
                A{"from","string",true,"Substring to replace.",""},
                A{"to","string",true,"Replacement.",""}
            },
            "string",
            { "#str.replace{\"s\":\"${buf}\",\"from\":\"foo\",\"to\":\"bar\",\"return\":true}" }
            });

        v.push_back(CommandInfo{
            "slice", "Substring [start, end) (end exclusive).",
            {
                A{"s","string",true,"Input string.",""},
                A{"start","number",true,"Start index (>=0).",""},
                A{"end","number",false,"End index (exclusive).",""}
            },
            "string",
            { "#str.slice{\"s\":\"${buf}\",\"start\":0,\"end\":10,\"return\":true}" }
            });

        v.push_back(CommandInfo{
            "match", "Regex match; returns JSON string on success, false otherwise.",
            {
                A{"s","string",true,"Input string.",""},
                A{"regex","string",true,"ECMAScript regex.",""},
                A{"case","string",false,"\"sensitive\" or \"insensitive\" (default).","\"insensitive\""}
            },
            "bool or string (JSON: {\"ok\":true,\"match\":\"...\",\"groups\":[...],\"pos\":N})",
            { "#str.match{\"s\":\"${line}\",\"regex\":\"^err(or)?\",\"case\":\"sensitive\",\"return\":true}" }
            });

        return v;
    }

    std::string BuildLLMUsagePrompt() const override {
        std::ostringstream os;
        os << "[#str ¡ª String Runtime]\n";
        os << "Pure functions on strings. Emit as: #str.<cmd>{...}\n";
        os << "Common: len, trim, lower, upper, contains, find, replace, slice, match\n\n";
        os << ActionOps::BuildLLMUsagePrompt();
        return os.str();
    }

private:
    // ---- helpers (no assumptions about defaults in X::Value) ----
    static bool ieq(const std::string& a, const std::string& b) {
        return a.size() == b.size() &&
            std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
            return std::tolower((unsigned char)x) == std::tolower((unsigned char)y);
                });
    }
    static std::string getS(const std::unordered_map<std::string, X::Value>& a, const char* k, const std::string& def = "") {
        auto it = a.find(k);
        if (it == a.end()) return def;
        const X::Value& v = it->second;
        if (v.isString()) return v.asString();
        if (v.isBool())   return v.asBool() ? "true" : "false";
        if (v.isNumber()) { char buf[64]; std::snprintf(buf, sizeof(buf), "%.15g", v.asNumber()); return std::string(buf); }
        return def; // null -> default
    }
    static bool getB(const std::unordered_map<std::string, X::Value>& a, const char* k, bool def = false) {
        auto it = a.find(k);
        if (it == a.end()) return def;
        const X::Value& v = it->second;
        if (v.isBool())   return v.asBool();
        if (v.isNumber()) return v.asNumber() != 0.0;
        if (v.isString()) {
            std::string s = v.asString(); toLowerInPlace(s);
            if (s == "true" || s == "1" || s == "yes" || s == "on") return true;
            if (s == "false" || s == "0" || s == "no" || s == "off") return false;
        }
        return def;
    }
    static long long getI64(const std::unordered_map<std::string, X::Value>& a, const char* k, long long def = 0) {
        auto it = a.find(k);
        if (it == a.end()) return def;
        const X::Value& v = it->second;
        if (v.isNumber()) return static_cast<long long>(v.asNumber());
        if (v.isString()) {
            try { return std::stoll(v.asString()); }
            catch (...) { return def; }
        }
        return def;
    }

    static void toLowerInPlace(std::string& s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    }
    static void toUpperInPlace(std::string& s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::toupper(c); });
    }
    static std::string trimCopy(const std::string& s) {
        size_t a = 0, b = s.size();
        while (a < b && std::isspace((unsigned char)s[a])) ++a;
        while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
        return s.substr(a, b - a);
    }
    static std::string replaceAll(std::string s, const std::string& from, const std::string& to) {
        if (from.empty()) return s;
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.size(), to);
            pos += to.size();
        }
        return s;
    }
    static std::string jsonEscape(const std::string& s) {
        std::string o; o.reserve(s.size() + 8);
        for (char c : s) {
            switch (c) {
            case '\\': o += "\\\\"; break;
            case '"':  o += "\\\""; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:   o.push_back(c); break;
            }
        }
        return o;
    }

    // ---- operations ----
    static X::Value op_len(const std::unordered_map<std::string, X::Value>& a, std::vector<std::string>& errs) {
        std::string s = getS(a, "s", "");
        (void)errs;
        return X::Value((long long)s.size());
    }
    static X::Value op_trim(const std::unordered_map<std::string, X::Value>& a, std::vector<std::string>& errs) {
        std::string s = getS(a, "s", "");
        (void)errs;
        return X::Value(trimCopy(s));
    }
    static X::Value op_lower(const std::unordered_map<std::string, X::Value>& a, std::vector<std::string>& errs) {
        std::string s = getS(a, "s", "");
        toLowerInPlace(s);
        (void)errs;
        return X::Value(s);
    }
    static X::Value op_upper(const std::unordered_map<std::string, X::Value>& a, std::vector<std::string>& errs) {
        std::string s = getS(a, "s", "");
        toUpperInPlace(s);
        (void)errs;
        return X::Value(s);
    }
    static X::Value op_contains(const std::unordered_map<std::string, X::Value>& a, std::vector<std::string>& errs) {
        std::string s = getS(a, "s", "");
        std::string needle = getS(a, "needle", "");
        (void)errs;
        if (needle.empty()) return X::Value(false);
        return X::Value(s.find(needle) != std::string::npos);
    }
    static X::Value op_find(const std::unordered_map<std::string, X::Value>& a, std::vector<std::string>& errs) {
        std::string s = getS(a, "s", "");
        std::string needle = getS(a, "needle", "");
        (void)errs;
        if (needle.empty()) return X::Value((long long)-1);
        auto pos = s.find(needle);
        return X::Value((long long)(pos == std::string::npos ? -1 : (long long)pos));
    }
    static X::Value op_replace(const std::unordered_map<std::string, X::Value>& a, std::vector<std::string>& errs) {
        std::string s = getS(a, "s", "");
        std::string from = getS(a, "from", "");
        std::string to = getS(a, "to", "");
        (void)errs;
        if (from.empty()) return X::Value(s); // nothing to do
        return X::Value(replaceAll(s, from, to));
    }
    static X::Value op_slice(const std::unordered_map<std::string, X::Value>& a, std::vector<std::string>& errs) {
        std::string s = getS(a, "s", "");
        long long start = getI64(a, "start", 0);
        bool hasEnd = a.find("end") != a.end();
        long long end = getI64(a, "end", (long long)s.size());
        (void)errs;

        if (start < 0) start = 0;
        if (end < 0) end = 0;
        if ((size_t)start > s.size()) start = (long long)s.size();
        if ((size_t)end > s.size()) end = (long long)s.size();
        if (!hasEnd) end = (long long)s.size();
        if (end < start) return X::Value(std::string());
        return X::Value(s.substr((size_t)start, (size_t)(end - start)));
    }
    static X::Value op_match(const std::unordered_map<std::string, X::Value>& a, std::vector<std::string>& errs) {
        std::string s = getS(a, "s", "");
        std::string pat = getS(a, "regex", "");
        std::string cs = getS(a, "case", "insensitive");
        if (pat.empty()) { errs.push_back("str.match: missing 'regex'"); return X::Value(); }

        std::regex::flag_type flags = std::regex::ECMAScript;
        if (!ieq(cs, "sensitive")) flags = (std::regex::flag_type)(flags | std::regex::icase);

        try {
            std::regex re(pat, flags);
            std::smatch m;
            if (!std::regex_search(s, m, re)) {
                return X::Value(false);
            }
            // Build compact JSON string: {"ok":true,"match":"...","groups":[...],"pos":N}
            std::string out = "{\"ok\":true,\"match\":\"";
            out += jsonEscape(m.str(0));
            out += "\",\"groups\":[";
            for (size_t i = 1; i < m.size(); ++i) {
                if (i > 1) out += ",";
                out += "\""; out += jsonEscape(m.str(i)); out += "\"";
            }
            out += "],\"pos\":";
            out += std::to_string(static_cast<long long>(m.position(0)));
            out += "}";
            return X::Value(out);
        }
        catch (const std::exception& e) {
            errs.push_back(std::string("str.match: regex error: ") + e.what());
            return X::Value();
        }
    }
};
