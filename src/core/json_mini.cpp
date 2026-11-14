#include "json_mini.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace hl3 {

namespace {

void append_utf8(std::string& out, uint32_t cp) {
    if (cp <= 0x7Fu) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FFu) {
        out.push_back(static_cast<char>(0xC0u | (cp >> 6)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else if (cp <= 0xFFFFu) {
        out.push_back(static_cast<char>(0xE0u | (cp >> 12)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else if (cp <= 0x10FFFFu) {
        out.push_back(static_cast<char>(0xF0u | (cp >> 18)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    }
}

class Parser {
public:
    explicit Parser(const std::string& s) : s_(s) {}

    bool parse_root(JsonValue& out, std::string* err) {
        skip_ws();
        if (!parse_value(out, err)) return false;
        skip_ws();
        if (pos_ != s_.size()) {
            return fail(err, "Trailing bytes after JSON document");
        }
        return true;
    }

private:
    const std::string& s_;
    size_t pos_ = 0;

    void skip_ws() {
        while (pos_ < s_.size()) {
            unsigned char c = static_cast<unsigned char>(s_[pos_]);
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                ++pos_;
            } else {
                break;
            }
        }
    }

    bool fail(std::string* err, const char* msg) {
        if (err) *err = msg;
        return false;
    }

    bool parse_value(JsonValue& out, std::string* err) {
        skip_ws();
        if (pos_ >= s_.size()) return fail(err, "Unexpected end of JSON");
        char c = s_[pos_];
        if (c == '{') return parse_object(out, err);
        if (c == '[') return parse_array(out, err);
        if (c == '"') {
            std::string str;
            if (!parse_string(str, err)) return false;
            out = JsonValue::make_string(std::move(str));
            return true;
        }
        if (c == 't' && s_.compare(pos_, 4, "true") == 0) {
            pos_ += 4;
            out = JsonValue::make_bool(true);
            return true;
        }
        if (c == 'f' && s_.compare(pos_, 5, "false") == 0) {
            pos_ += 5;
            out = JsonValue::make_bool(false);
            return true;
        }
        if (c == 'n' && s_.compare(pos_, 4, "null") == 0) {
            pos_ += 4;
            out = JsonValue::make_null();
            return true;
        }
        if (c == '-' || (c >= '0' && c <= '9')) return parse_number(out, err);
        return fail(err, "Unexpected token in JSON");
    }

    bool parse_hex4(uint32_t& code, std::string* err) {
        if (pos_ + 4 > s_.size()) return fail(err, "Truncated \\uXXXX escape");
        code = 0;
        for (int i = 0; i < 4; ++i) {
            char c = s_[pos_++];
            code <<= 4;
            if (c >= '0' && c <= '9') code |= static_cast<uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') code |= static_cast<uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') code |= static_cast<uint32_t>(c - 'A' + 10);
            else return fail(err, "Invalid hex digit in \\u escape");
        }
        return true;
    }

    bool parse_string(std::string& out, std::string* err) {
        if (pos_ >= s_.size() || s_[pos_] != '"') return fail(err, "Expected '\"'");
        ++pos_;
        out.clear();
        while (pos_ < s_.size()) {
            char c = s_[pos_++];
            if (c == '"') return true;
            if (c == '\\') {
                if (pos_ >= s_.size()) return fail(err, "Truncated escape");
                char e = s_[pos_++];
                switch (e) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u': {
                        uint32_t cp = 0;
                        if (!parse_hex4(cp, err)) return false;
                        if (cp >= 0xD800u && cp <= 0xDBFFu && pos_ + 2 <= s_.size() &&
                            s_[pos_] == '\\' && s_[pos_ + 1] == 'u') {
                            pos_ += 2;
                            uint32_t low = 0;
                            if (!parse_hex4(low, err)) return false;
                            if (low >= 0xDC00u && low <= 0xDFFFu) {
                                cp = 0x10000u + (((cp - 0xD800u) << 10) | (low - 0xDC00u));
                            }
                        }
                        append_utf8(out, cp);
                        break;
                    }
                    default: return fail(err, "Invalid JSON string escape");
                }
            } else {
                out.push_back(c);
            }
        }
        return fail(err, "Unterminated JSON string");
    }

    bool parse_number(JsonValue& out, std::string* err) {
        size_t start = pos_;
        if (s_[pos_] == '-') ++pos_;
        if (pos_ >= s_.size() || !std::isdigit(static_cast<unsigned char>(s_[pos_]))) {
            return fail(err, "Invalid JSON number");
        }
        while (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_]))) ++pos_;
        if (pos_ < s_.size() && s_[pos_] == '.') {
            ++pos_;
            if (pos_ >= s_.size() || !std::isdigit(static_cast<unsigned char>(s_[pos_]))) {
                return fail(err, "Invalid JSON fraction");
            }
            while (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_]))) ++pos_;
        }
        if (pos_ < s_.size() && (s_[pos_] == 'e' || s_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < s_.size() && (s_[pos_] == '+' || s_[pos_] == '-')) ++pos_;
            if (pos_ >= s_.size() || !std::isdigit(static_cast<unsigned char>(s_[pos_]))) {
                return fail(err, "Invalid JSON exponent");
            }
            while (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_]))) ++pos_;
        }
        std::string slice = s_.substr(start, pos_ - start);
        char* endptr = nullptr;
        double d = std::strtod(slice.c_str(), &endptr);
        if (endptr == slice.c_str()) return fail(err, "Failed parsing JSON number");
        out = JsonValue::make_number(d);
        return true;
    }

    bool parse_array(JsonValue& out, std::string* err) {
        ++pos_;  // '['
        out = JsonValue::make_array();
        skip_ws();
        if (pos_ < s_.size() && s_[pos_] == ']') {
            ++pos_;
            return true;
        }
        while (true) {
            JsonValue elem;
            if (!parse_value(elem, err)) return false;
            out.arr_val.push_back(std::move(elem));
            skip_ws();
            if (pos_ >= s_.size()) return fail(err, "Unterminated JSON array");
            if (s_[pos_] == ']') {
                ++pos_;
                return true;
            }
            if (s_[pos_] != ',') return fail(err, "Expected ',' in JSON array");
            ++pos_;
        }
    }

    bool parse_object(JsonValue& out, std::string* err) {
        ++pos_;  // '{'
        out = JsonValue::make_object();
        skip_ws();
        if (pos_ < s_.size() && s_[pos_] == '}') {
            ++pos_;
            return true;
        }
        while (true) {
            skip_ws();
            std::string key;
            if (!parse_string(key, err)) return false;
            skip_ws();
            if (pos_ >= s_.size() || s_[pos_] != ':') return fail(err, "Expected ':' after JSON key");
            ++pos_;
            JsonValue val;
            if (!parse_value(val, err)) return false;
            out.obj_val[std::move(key)] = std::move(val);
            skip_ws();
            if (pos_ >= s_.size()) return fail(err, "Unterminated JSON object");
            if (s_[pos_] == '}') {
                ++pos_;
                return true;
            }
            if (s_[pos_] != ',') return fail(err, "Expected ',' in JSON object");
            ++pos_;
        }
    }
};

void escape_json_str(const std::string& s, std::string& out) {
    out.push_back('"');
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20u) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
                break;
        }
    }
    out.push_back('"');
}

void dump_impl(const JsonValue& v, int indent, int depth, std::string& out) {
    auto nl_indent = [&](int d) {
        if (indent >= 0) {
            out.push_back('\n');
            out.append(static_cast<size_t>(d * indent), ' ');
        }
    };

    switch (v.type) {
        case JsonValue::Type::Null: out += "null"; break;
        case JsonValue::Type::Bool: out += (v.bool_val ? "true" : "false"); break;
        case JsonValue::Type::Number: {
            if (std::isfinite(v.num_val) && std::floor(v.num_val) == v.num_val &&
                std::fabs(v.num_val) <= 9007199254740991.0) {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(v.num_val));
                out += buf;
            } else {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "%.15g", v.num_val);
                out += buf;
            }
            break;
        }
        case JsonValue::Type::String: escape_json_str(v.str_val, out); break;
        case JsonValue::Type::Array: {
            if (v.arr_val.empty()) {
                out += "[]";
                break;
            }
            out.push_back('[');
            for (size_t i = 0; i < v.arr_val.size(); ++i) {
                if (i > 0) out.push_back(',');
                nl_indent(depth + 1);
                dump_impl(v.arr_val[i], indent, depth + 1, out);
            }
            nl_indent(depth);
            out.push_back(']');
            break;
        }
        case JsonValue::Type::Object: {
            if (v.obj_val.empty()) {
                out += "{}";
                break;
            }
            out.push_back('{');
            bool first = true;
            for (const auto& kv : v.obj_val) {
                if (!first) out.push_back(',');
                first = false;
                nl_indent(depth + 1);
                escape_json_str(kv.first, out);
                out += (indent >= 0 ? ": " : ":");
                dump_impl(kv.second, indent, depth + 1, out);
            }
            nl_indent(depth);
            out.push_back('}');
            break;
        }
    }
}

}  // namespace

const JsonValue* JsonValue::get(const std::string& key) const {
    if (type != Type::Object) return nullptr;
    auto it = obj_val.find(key);
    return (it != obj_val.end()) ? &it->second : nullptr;
}

std::string JsonValue::get_string(const std::string& key, const std::string& def) const {
    const JsonValue* v = get(key);
    return (v && v->is_string()) ? v->str_val : def;
}

double JsonValue::get_number(const std::string& key, double def) const {
    const JsonValue* v = get(key);
    return (v && v->is_number()) ? v->num_val : def;
}

int64_t JsonValue::get_int(const std::string& key, int64_t def) const {
    const JsonValue* v = get(key);
    return (v && v->is_number()) ? static_cast<int64_t>(v->num_val) : def;
}

bool JsonValue::get_bool(const std::string& key, bool def) const {
    const JsonValue* v = get(key);
    return (v && v->is_bool()) ? v->bool_val : def;
}

std::string JsonValue::dump(int indent) const {
    std::string out;
    dump_impl(*this, indent, 0, out);
    return out;
}

bool JsonValue::parse(const std::string& text, JsonValue& out, std::string* err) {
    Parser p(text);
    return p.parse_root(out, err);
}

}  // namespace hl3

