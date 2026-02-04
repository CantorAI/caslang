#pragma once
#include "xlang.h"
#include <string>
#include <regex>
#include <vector>
#include <algorithm>

namespace CasLang {

    // Simple Expression Evaluator (Arithmetic + Logic)
    // Supports: + - * / ( ) on numbers, and basic boolean/null literals
    inline X::Value EvaluateExpr(const std::string& expr) {
        std::string s = expr;
        
        // Helper to trim
        auto trim = [](std::string& str) {
            str.erase(0, str.find_first_not_of(" \t"));
            str.erase(str.find_last_not_of(" \t") + 1);
        };
        
        std::string t = s;
        trim(t);
        if (t.empty()) return X::Value();
        if (t[0] == '=') t = t.substr(1); 
        
        // MVP: Regex for binary ops with simplified precedence
        // Scan for lowest precedence operator to split the tree
        // Precedence: (+ -) < (* /)
        
        // Basic parser: manually scan for split point
        int balance = 0;
        int splitPos = -1;
        int opPrior = 999; 
        
        for (int i = (int)t.size() - 1; i >= 0; i--) {
            char c = t[i];
            if (c == ')') balance++;
            else if (c == '(') balance--;
            else if (balance == 0) {
                if (c == '+' || c == '-') {
                    if (opPrior >= 1) { splitPos = i; opPrior = 1; }
                }
                else if (c == '*' || c == '/') {
                    if (opPrior > 2) { splitPos = i; opPrior = 2; }
                }
            }
        }
        
        if (splitPos != -1) {
            std::string leftS = t.substr(0, splitPos);
            std::string rightS = t.substr(splitPos + 1);
            char opChar = t[splitPos];
            
            X::Value vL = EvaluateExpr(leftS);
            X::Value vR = EvaluateExpr(rightS);
            
            double dL = vL.isNumber() ? (double)vL : 0;
            double dR = vR.isNumber() ? (double)vR : 0;
            
            if (opChar == '+') return X::Value(dL + dR);
            if (opChar == '-') return X::Value(dL - dR);
            if (opChar == '*') return X::Value(dL * dR);
            if (opChar == '/') return X::Value(dR != 0 ? dL / dR : 0);
        }
        
        // Leaf: Number
        try {
            size_t idx;
            double d = std::stod(t, &idx);
            if (idx == t.size()) return X::Value(d);
        } catch(...) {}
        
        // Literals
        if (t == "true") return X::Value(true);
        if (t == "false") return X::Value(false);
        if (t == "null") return X::Value();
        
        // String literal
        if (t.size()>=2 && ( (t.front()=='"'&&t.back()=='"') || (t.front()=='\''&&t.back()=='\'') )) {
            return X::Value(t.substr(1, t.size()-2));
        }

        return X::Value(0); 
    }
}
