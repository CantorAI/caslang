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
#include "cas_value.h"
#include <string>
#include <regex>
#include <vector>
#include <algorithm>

namespace CasLang {

    // Simple Expression Evaluator (Arithmetic + Logic)
    // Supports: + - * / ( ) on numbers, and basic boolean/null literals
    inline Cas::Value EvaluateExpr(const std::string& expr) {
        std::string s = expr;
        
        // Helper to trim
        auto trim = [](std::string& str) {
            str.erase(0, str.find_first_not_of(" \t"));
            str.erase(str.find_last_not_of(" \t") + 1);
        };
        
        std::string t = s;
        trim(t);
        if (t.empty()) return Cas::Value();
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
            
            Cas::Value vL = EvaluateExpr(leftS);
            Cas::Value vR = EvaluateExpr(rightS);
            
            double dL = vL.isNumber() ? (double)vL : 0;
            double dR = vR.isNumber() ? (double)vR : 0;
            
            if (opChar == '+') return Cas::Value(dL + dR);
            if (opChar == '-') return Cas::Value(dL - dR);
            if (opChar == '*') return Cas::Value(dL * dR);
            if (opChar == '/') return Cas::Value(dR != 0 ? dL / dR : 0);
        }
        
        // Leaf: Number
        try {
            size_t idx;
            double d = std::stod(t, &idx);
            if (idx == t.size()) return Cas::Value(d);
        } catch(...) {}
        
        // Literals
        if (t == "true") return Cas::Value(true);
        if (t == "false") return Cas::Value(false);
        if (t == "null") return Cas::Value();
        
        // String literal
        if (t.size()>=2 && ( (t.front()=='"'&&t.back()=='"') || (t.front()=='\''&&t.back()=='\'') )) {
            return Cas::Value(t.substr(1, t.size()-2));
        }

        return Cas::Value(0); 
    }
}
