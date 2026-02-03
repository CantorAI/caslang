#pragma once
#include <string>
#include <vector>
#include <optional>
#include <cctype>
#include <stdexcept>
#include <unordered_map>

#include <nlohmann/json.hpp>
#include "xlang.h"

using json = nlohmann::json;

namespace CasLang {

struct ParsedAction {
    std::vector<std::string> ns; // e.g. {"fs"}
    std::string command;         // e.g. "open"
    std::unordered_map<std::string, X::Value> args; // <-- changed type
    size_t start = 0, end = 0;
    std::string error;
};

class CasParser {
public:
    std::vector<ParsedAction> Extract(const std::string& text) {
        src_ = &text; i_ = 0; n_ = text.size();
        std::vector<ParsedAction> out;
        while (i_ < n_) {
            if ((*src_)[i_] == '#') {
                auto maybe = parseOne();
                if (maybe.has_value()) out.emplace_back(std::move(*maybe));
                else ++i_;
            }
            else { ++i_; }
        }
        return out;
    }

private:
    std::optional<ParsedAction> parseOne() {
        size_t hash = i_++; // '#'
        ParsedAction pa; pa.start = hash;

        // qualified name
        std::vector<std::string> parts;
        std::string id;
        if (!parseIdent(id)) return std::nullopt;
        parts.push_back(std::move(id));
        while (peek() == '.') { ++i_; if (!parseIdent(id)) return std::nullopt; parts.push_back(id); }
        pa.command = parts.back(); parts.pop_back(); pa.ns = std::move(parts);

        skipWS();
        if (peek() != '{') { pa.end = i_; return pa; } // no-arg command

        std::string obj = readBalanced('{', '}');
        if (obj.empty()) { pa.error = "Unbalanced JSON braces"; pa.end = i_; return pa; }

        try {
            json j = json::parse(obj);
            if (!j.is_object()) throw std::runtime_error("Payload must be a JSON object");
            // Only scalar values -> X::Value
            for (auto it = j.begin(); it != j.end(); ++it) {
                const auto& k = it.key();
                const auto& v = it.value();
                pa.args.emplace(k, toXScalar(v));
            }
        }
        catch (const std::exception& e) {
            pa.error = std::string("JSON parse error: ") + e.what();
        }
        pa.end = i_;
        return pa;
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

    // helpers
    bool parseIdent(std::string& out) {
        if (i_ >= n_) return false;
        char c = (*src_)[i_];
        if (!(std::isalpha((unsigned char)c) || c == '_')) return false;
        size_t s = i_++;
        while (i_ < n_) {
            char d = (*src_)[i_];
            if (std::isalnum((unsigned char)d) || d == '_' || d == '-') ++i_;
            else break;
        }
        out = src_->substr(s, i_ - s);
        return true;
    }
    std::string readBalanced(char open, char close) {
        if (peek() != open) return {};
        size_t start = i_;
        int depth = 0;
        bool inStr = false; char quote = 0; bool esc = false;
        do {
            char c = (*src_)[i_++];
            if (inStr) { if (esc) { esc = false; continue; } if (c == '\\') { esc = true; continue; } if (c == quote) inStr = false; continue; }
            if (c == '"' || c == '\'') { inStr = true; quote = c; continue; }
            if (c == open) ++depth; else if (c == close) --depth;
        } while (i_ < n_ && depth>0);
        if (depth != 0) return {};
        return src_->substr(start, i_ - start);
    }
    void skipWS() { while (i_ < n_) { char c = (*src_)[i_]; if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { ++i_; continue; } break; } }
    char peek() const { return (i_ < n_) ? (*src_)[i_] : '\0'; }

    const std::string* src_{ nullptr };
    size_t i_{ 0 }, n_{ 0 };
};
}
