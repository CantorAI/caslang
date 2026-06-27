#include "xlang_value.h"

namespace Cas {

    std::string Value::asString() const {
        switch (t) {
            case ValueType::Invalid: return "Invalid";
            case ValueType::None: return "null";
            case ValueType::Bool: return x.l ? "true" : "false";
            case ValueType::Int64: return std::to_string(x.l);
            case ValueType::Double: {
                std::ostringstream ss;
                ss << x.d;
                return ss.str();
            }
            case ValueType::Str: return x.s;
            case ValueType::List: return "[List]"; // Placeholder for JSON serialization if needed
            case ValueType::Dict: return "{Dict}";
            default: return "";
        }
    }

    void Value::Clone() {
        if (t == ValueType::List && x.list) {
            auto newList = std::make_shared<ListImpl>(*x.list);
            x.list = newList;
        } else if (t == ValueType::Dict && x.dict) {
            auto newDict = std::make_shared<DictImpl>(*x.dict);
            x.dict = newDict;
        }
    }

    bool Value::operator==(const Value& other) const {
        if (t != other.t) {
            // Type coercion for numbers
            if (isNumber() && other.isNumber()) {
                double v1 = (t == ValueType::Double) ? x.d : (double)x.l;
                double v2 = (other.t == ValueType::Double) ? other.x.d : (double)other.x.l;
                return v1 == v2;
            }
            return false;
        }
        
        switch (t) {
            case ValueType::Invalid:
            case ValueType::None: return true;
            case ValueType::Bool:
            case ValueType::Int64: return x.l == other.x.l;
            case ValueType::Double: return x.d == other.x.d;
            case ValueType::Str: return x.s == other.x.s;
            case ValueType::List: return x.list == other.x.list; // pointer equality
            case ValueType::Dict: return x.dict == other.x.dict; // pointer equality
            default: return false;
        }
    }

}
