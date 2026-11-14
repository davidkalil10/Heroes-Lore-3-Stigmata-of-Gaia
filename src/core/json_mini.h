#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace hl3 {

// Lightweight JSON value model for config files, Google Drive v3 API, save packages, and GitHub Releases.
struct JsonValue {
    enum class Type { Null, Bool, Number, String, Array, Object };
    using Array = std::vector<JsonValue>;
    using Object = std::map<std::string, JsonValue>;

    Type type = Type::Null;
    bool bool_val = false;
    double num_val = 0.0;
    std::string str_val;
    Array arr_val;
    Object obj_val;

    static JsonValue make_null() { return JsonValue{}; }
    static JsonValue make_bool(bool b) {
        JsonValue v;
        v.type = Type::Bool;
        v.bool_val = b;
        return v;
    }
    static JsonValue make_number(double n) {
        JsonValue v;
        v.type = Type::Number;
        v.num_val = n;
        return v;
    }
    static JsonValue make_string(std::string s) {
        JsonValue v;
        v.type = Type::String;
        v.str_val = std::move(s);
        return v;
    }
    static JsonValue make_array(std::vector<JsonValue> a = {}) {
        JsonValue v;
        v.type = Type::Array;
        v.arr_val = std::move(a);
        return v;
    }
    static JsonValue make_object() {
        JsonValue v;
        v.type = Type::Object;
        return v;
    }
    static JsonValue make_object(Object obj) {
        JsonValue v;
        v.type = Type::Object;
        v.obj_val = std::move(obj);
        return v;
    }

    bool is_null() const { return type == Type::Null; }
    bool is_bool() const { return type == Type::Bool; }
    bool is_number() const { return type == Type::Number; }
    bool is_string() const { return type == Type::String; }
    bool is_array() const { return type == Type::Array; }
    bool is_object() const { return type == Type::Object; }

    const std::string& as_string() const { return str_val; }
    const Array& as_array() const { return arr_val; }
    const Object& as_object() const { return obj_val; }

    const JsonValue* get(const std::string& key) const;
    std::string get_string(const std::string& key, const std::string& def = "") const;
    double get_number(const std::string& key, double def = 0.0) const;
    int64_t get_int(const std::string& key, int64_t def = 0) const;
    bool get_bool(const std::string& key, bool def = false) const;

    std::string dump(int indent = 2) const;
    static bool parse(const std::string& text, JsonValue& out, std::string* err = nullptr);
};

}  // namespace hl3

