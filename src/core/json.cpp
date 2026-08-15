#include "json.h"
#include <cstring>
#include <cstdlib>
#include <cmath>

namespace sl::json {

static Value clone(const Value& o);
static void free_val(Value& v);

Value::Value(const Value& o) : type(o.type), b(o.b), num(o.num), str(o.str),
                               arr(o.arr ? new std::vector<Value>(*o.arr) : nullptr),
                               obj(o.obj ? new std::vector<std::pair<std::string, Value>>(*o.obj) : nullptr) {}

Value& Value::operator=(const Value& o) {
    if (this == &o) return *this;
    Value t(o);
    std::swap(type, t.type); std::swap(b, t.b); std::swap(num, t.num);
    std::swap(str, t.str); std::swap(arr, t.arr); std::swap(obj, t.obj);
    return *this;
}
Value::Value(Value&& o) noexcept : type(o.type), b(o.b), num(o.num), str(std::move(o.str)),
                                   arr(o.arr), obj(o.obj) { o.arr = nullptr; o.obj = nullptr; o.type = Type::Null; }
Value& Value::operator=(Value&& o) noexcept {
    if (this == &o) return *this;
    free_val(*this);
    type = o.type; b = o.b; num = o.num; str = std::move(o.str);
    arr = o.arr; obj = o.obj;
    o.arr = nullptr; o.obj = nullptr; o.type = Type::Null;
    return *this;
}
Value::~Value() { free_val(*this); }

static void free_val(Value& v) {
    delete v.arr; v.arr = nullptr;
    delete v.obj; v.obj = nullptr;
}

const Value* Value::get(const char* key) const {
    if (!is_obj()) return nullptr;
    for (auto& kv : *obj) if (kv.first == key) return &kv.second;
    return nullptr;
}
const Value& Value::at(size_t i) const {
    static Value nullv;
    if (!is_arr() || i >= arr->size()) return nullv;
    return (*arr)[i];
}
size_t Value::size() const {
    if (is_arr()) return arr->size();
    if (is_obj()) return obj->size();
    return 0;
}
std::string Value::as_string() const { return str; }
bool Value::as_bool() const { return b; }
double Value::as_number() const { return num; }
long long Value::as_long() const { return (long long)num; }

// ---------------- Parser ----------------
struct Parser {
    const char* p;
    const char* end;
    std::string err;

    void skip_ws() { while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++; }
    bool eof() { return p >= end; }
    char peek() { return p < end ? *p : '\0'; }
    bool fail(const char* m) { err = m; return false; }

    bool parse_value(Value& v) {
        skip_ws();
        if (eof()) return fail("unexpected end");
        char c = *p;
        if (c == '{') return parse_object(v);
        if (c == '[') return parse_array(v);
        if (c == '"') return parse_string(v.str), v.type = Type::String, true;
        if (c == 't') { expect("true"); v.type = Type::Bool; v.b = true; return true; }
        if (c == 'f') { expect("false"); v.type = Type::Bool; v.b = false; return true; }
        if (c == 'n') { expect("null"); v.type = Type::Null; return true; }
        if (c == '-' || (c >= '0' && c <= '9')) return parse_number(v);
        return fail("unexpected token");
    }
    void expect(const char* word) {
        while (*word && p < end && *p == *word) { p++; word++; }
    }
    void parse_string(std::string& out) {
        out.clear();
        if (p < end && *p == '"') p++;
        while (p < end && *p != '"') {
            unsigned char c = (unsigned char)*p;
            if (c == '\\' && p + 1 < end) {
                p++;
                char e = *p;
                switch (e) {
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case '"': out += '"'; break;
                    case '/': out += '/'; break;
                    case '\\': out += '\\'; break;
                    case 'u': {
                        if (p + 4 < end) {
                            unsigned cp = 0;
                            for (int i = 0; i < 4 && p + 1 < end; i++) {
                                p++;
                                char h = *p;
                                cp <<= 4;
                                if (h >= '0' && h <= '9') cp |= (unsigned)(h - '0');
                                else if (h >= 'a' && h <= 'f') cp |= (unsigned)(h - 'a' + 10);
                                else if (h >= 'A' && h <= 'F') cp |= (unsigned)(h - 'A' + 10);
                            }
                            if (cp >= 0x0800) { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
                            else if (cp >= 0x80) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
                            else out += (char)cp;
                        }
                        break;
                    }
                    default: out += e;
                }
                p++;
            } else {
                out += (char)c;
                p++;
            }
        }
        if (p < end) p++; // closing quote
    }
    bool parse_number(Value& v) {
        const char* start = p;
        if (p < end && (*p == '-' || *p == '+')) p++;
        while (p < end && ((*p >= '0' && *p <= '9') || *p == '.' || *p == 'e' || *p == 'E' || *p == '+' || *p == '-')) p++;
        std::string s(start, (size_t)(p - start));
        v.num = std::strtod(s.c_str(), nullptr);
        v.type = Type::Number;
        return true;
    }
    bool parse_array(Value& v) {
        p++; // '['
        v.type = Type::Array;
        v.arr = new std::vector<Value>();
        skip_ws();
        if (peek() == ']') { p++; return true; }
        for (;;) {
            skip_ws();
            Value item;
            if (!parse_value(item)) return fail("bad array item");
            v.arr->push_back(std::move(item));
            skip_ws();
            if (peek() == ',') { p++; continue; }
            if (peek() == ']') { p++; return true; }
            return fail("expected , or ]");
        }
    }
    bool parse_object(Value& v) {
        p++; // '{'
        v.type = Type::Object;
        v.obj = new std::vector<std::pair<std::string, Value>>();
        skip_ws();
        if (peek() == '}') { p++; return true; }
        for (;;) {
            skip_ws();
            if (peek() != '"') return fail("expected string key");
            std::string key;
            parse_string(key);
            skip_ws();
            if (peek() != ':') return fail("expected :");
            p++;
            Value item;
            if (!parse_value(item)) return fail("bad object value");
            v.obj->push_back({ key, std::move(item) });
            skip_ws();
            if (peek() == ',') { p++; continue; }
            if (peek() == '}') { p++; return true; }
            return fail("expected , or }");
        }
    }
};

bool parse(const std::string& text, Value& out, std::string* err) {
    Parser ps;
    ps.p = text.c_str();
    ps.end = text.c_str() + text.size();
    if (!ps.parse_value(out)) {
        if (err) *err = ps.err;
        return false;
    }
    ps.skip_ws();
    if (!ps.eof()) {
        if (err) *err = "trailing data";
        return false;
    }
    return true;
}

// ---------------- Serializer ----------------
static void dump_str(const std::string& s, std::string& o) {
    o += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\t': o += "\\t"; break;
            case '\r': o += "\\r"; break;
            default:
                if (c < 0x20) { char t[4]; std::snprintf(t, 4, "\\u%04x", c); o += t; }
                else o += (char)c;
        }
    }
    o += '"';
}

static void dump_val(const Value& v, std::string& o, int indent, int depth) {
    switch (v.type) {
        case Type::Null: o += "null"; break;
        case Type::Bool: o += v.b ? "true" : "false"; break;
        case Type::Number: {
            double d = v.num;
            if (d == (long long)d) {
                char t[32]; std::snprintf(t, 32, "%lld", (long long)d); o += t;
            } else {
                char t[32]; std::snprintf(t, 32, "%.6f", d);
                o += t;
            }
            break;
        }
        case Type::String: dump_str(v.str, o); break;
        case Type::Array: {
            o += '[';
            if (indent > 0 && v.arr && !v.arr->empty()) {
                for (size_t i = 0; i < v.arr->size(); i++) {
                    if (i) o += ",";
                    o += '\n';
                    for (int k = 0; k < (depth + 1) * indent; k++) o += ' ';
                    dump_val((*v.arr)[i], o, indent, depth + 1);
                }
                o += '\n';
                for (int k = 0; k < depth * indent; k++) o += ' ';
            }
            o += ']';
            break;
        }
        case Type::Object: {
            o += '{';
            if (indent > 0 && v.obj && !v.obj->empty()) {
                for (size_t i = 0; i < v.obj->size(); i++) {
                    if (i) o += ",";
                    o += '\n';
                    for (int k = 0; k < (depth + 1) * indent; k++) o += ' ';
                    dump_str((*v.obj)[i].first, o);
                    o += ':';
                    if (indent > 0) o += ' ';
                    dump_val((*v.obj)[i].second, o, indent, depth + 1);
                }
                o += '\n';
                for (int k = 0; k < depth * indent; k++) o += ' ';
            }
            o += '}';
            break;
        }
    }
}

std::string dump(const Value& v, int indent, int depth) {
    std::string o;
    dump_val(v, o, indent, depth);
    return o;
}

} // namespace sl::json