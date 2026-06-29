#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <stdexcept>
#include <functional>
#include <iostream>
#include <sstream>

namespace Cas {

    class Value;

    using ListImpl = std::vector<Value>;
    using DictImpl = std::unordered_map<std::string, Value>;

    enum class ValueType {
        Invalid,
        None,
        Bool,
        Int64,
        Double,
        Str,
        List,
        Dict,
        Bin
    };

    struct ValueData {
        int64_t l;
        double d;
        std::string s;
        std::shared_ptr<ListImpl> list;
        std::shared_ptr<DictImpl> dict;
        std::shared_ptr<std::vector<uint8_t>> bin;

        ValueData() : l(0) {}
        ~ValueData() {} 
    };

    class Dict;
    class List;

    class Value {
        ValueType t = ValueType::Invalid;
        ValueData x;
    public:
        Value() {}
        Value(bool v) : t(ValueType::Bool) { x.l = v ? 1 : 0; }
        Value(int v) : t(ValueType::Int64) { x.l = v; }
        Value(long long v) : t(ValueType::Int64) { x.l = v; }
        Value(double v) : t(ValueType::Double) { x.d = v; }
        Value(const char* v) : t(ValueType::Str) { if (v) x.s = v; }
        Value(const std::string& v) : t(ValueType::Str) { x.s = v; }
        Value(Dict* d);
        Value(List* l);
        Value(const Dict& d);
        Value(const List& l);
        Value(const uint8_t* data, size_t size) : t(ValueType::Bin) {
            x.bin = std::make_shared<std::vector<uint8_t>>(data, data + size);
        }
        
        bool IsValid() const { return t != ValueType::Invalid; }
        ValueType GetType() const { return t; }
        bool IsNone() const { return t == ValueType::None; }
        bool IsObject() const { return t == ValueType::List || t == ValueType::Dict || t == ValueType::Str; }
        bool isList() const { return t == ValueType::List; }
        bool isDict() const { return t == ValueType::Dict; }
        bool isBin() const { return t == ValueType::Bin; }
        bool isString() const { return t == ValueType::Str; }
        bool isNumber() const { return t == ValueType::Int64 || t == ValueType::Double; }
        bool isBool() const { return t == ValueType::Bool; }
        
        bool IsList() const { return isList(); }
        bool IsDict() const { return isDict(); }
        bool IsBin() const { return isBin(); }
        bool IsString() const { return isString(); }
        bool IsNumber() const { return isNumber(); }
        bool IsBool() const { return isBool(); }

        int64_t GetInt() const { return (t == ValueType::Int64) ? x.l : (t == ValueType::Double ? (int64_t)x.d : 0); }
        double GetDouble() const { return (t == ValueType::Double) ? x.d : (t == ValueType::Int64 ? (double)x.l : 0.0); }
        std::string GetString() const { return (t == ValueType::Str) ? x.s : asString(); }
        std::shared_ptr<ListImpl> GetList() const { return x.list; }
        std::shared_ptr<DictImpl> GetDict() const { return x.dict; }
        std::shared_ptr<std::vector<uint8_t>> GetBin() const { return x.bin; }

        std::string asString() const;
        long long asNumber() const {
            if (t == ValueType::Int64 || t == ValueType::Bool) return x.l;
            if (t == ValueType::Double) return (long long)x.d;
            return 0;
        }
        bool asBool() const {
            if (t == ValueType::Bool) return x.l != 0;
            if (t == ValueType::Int64) return x.l != 0;
            if (t == ValueType::Double) return x.d != 0;
            return false;
        }
        bool IsTrue() const { return asBool(); }
        std::string ToString(bool withFormat = false) const { return asString(); }
        
        void Clone(); // deep copy if needed
        
        explicit operator double() const { return (t == ValueType::Double) ? x.d : (double)x.l; }
        explicit operator long long() const { return asNumber(); }
        explicit operator int() const { return (int)asNumber(); }
        explicit operator bool() const { return asBool(); }
        explicit operator std::string() const { return asString(); }
        
        bool operator==(const Value& other) const;
        bool operator!=(const Value& other) const { return !(*this == other); }
        
        friend class Dict;
        friend class List;
    };

    class Dict {
        std::shared_ptr<DictImpl> m_dict;
    public:
        Dict() { m_dict = std::make_shared<DictImpl>(); }
        Dict(const Value& v) {
            if (v.IsDict()) m_dict = v.GetDict();
            else m_dict = std::make_shared<DictImpl>();
        }
        
        Dict* operator->() { return this; }
        
        bool Has(const std::string& key) const {
            return m_dict->find(key) != m_dict->end();
        }
        bool Has(const char* key) const {
            return Has(std::string(key));
        }
        bool Has(const Value& key) const {
            return Has(key.asString());
        }
        Value Get(const std::string& key) const {
            auto it = m_dict->find(key);
            if (it != m_dict->end()) return it->second;
            return Value();
        }
        Value Get(const char* key) const {
            return Get(std::string(key));
        }
        Value Get(const Value& key) const {
            return Get(key.asString());
        }
        Value& operator[](const std::string& key) {
            return (*m_dict)[key];
        }
        Value& operator[](const char* key) {
            return (*m_dict)[key];
        }
        void Set(const std::string& key, const Value& val) {
            (*m_dict)[key] = val;
        }
        void Set(const char* key, const Value& val) {
            Set(std::string(key), val);
        }
        void Set(const Value& key, const Value& val) {
            Set(key.asString(), val);
        }
        bool Remove(const std::string& key) {
            return m_dict->erase(key) > 0;
        }
        bool Remove(const char* key) {
            return Remove(std::string(key));
        }
        bool Remove(const Value& key) {
            return Remove(key.asString());
        }
        size_t Size() const { return m_dict->size(); }
        
        using Dict_Enum = std::function<void(Value& k, Value& v)>;
        void Enum(Dict_Enum proc) {
            for (auto& kv : *m_dict) {
                Value keyVal(kv.first);
                proc(keyVal, kv.second);
            }
        }
        
        std::shared_ptr<DictImpl> GetImpl() const { return m_dict; }
    };

    class List {
        std::shared_ptr<ListImpl> m_list;
    public:
        List() { m_list = std::make_shared<ListImpl>(); }
        List(const Value& v) {
            if (v.IsList()) m_list = v.GetList();
            else m_list = std::make_shared<ListImpl>();
        }
        
        List* operator->() { return this; }
        
        void AddItem(const Value& val) { m_list->push_back(val); }
        List& operator+=(const Value& val) {
            AddItem(val);
            return *this;
        }
        size_t Size() const { return m_list->size(); }
        Value Get(long long idx) const {
            if (idx >= 0 && idx < (long long)m_list->size()) return (*m_list)[idx];
            return Value();
        }
        Value& operator[](long long idx) { return (*m_list)[idx]; }
        
        void RemoveAll() { m_list->clear(); }
        
        std::shared_ptr<ListImpl> GetImpl() const { return m_list; }
    };

    inline Value::Value(Dict* d) : t(ValueType::Dict) { x.dict = d->GetImpl(); }
    inline Value::Value(List* l) : t(ValueType::List) { x.list = l->GetImpl(); }
    inline Value::Value(const Dict& d) : t(ValueType::Dict) { x.dict = d.GetImpl(); }
    inline Value::Value(const List& l) : t(ValueType::List) { x.list = l.GetImpl(); }

}
