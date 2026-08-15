#pragma once
#include <string>
#include <vector>

// Минимальный JSON DOM с парсером. Не зависит от CRT-функций БЕЗ безопасных
// вариантов STL — используем std::string. Нужен только для ЧТЕНИЯ:
//  - version_manifest_v2.json (список версий)
//  - <version>.json (libraries, downloads, arguments, assetIndex)
//  - user_data/instances.json
//  - settings.json
namespace sl::json {

enum class Type { Null, Bool, Number, String, Array, Object };

struct Value;
using Array = std::vector<Value>; // fwd; defined below via shared pointer? use by-value with heap indirection

struct Pair;

// Value хранит скаляр по значению и контейнеры указением (для рекурсии).
// Хранить std::vector<Value> по значению нельзя (incomplete), поэтому держим
// указатель на данные для Array/Object.
struct Value {
    Type type = Type::Null;
    bool b = false;
    double num = 0;
    std::string str;
    std::vector<Value>* arr = nullptr;
    std::vector<std::pair<std::string, Value>>* obj = nullptr;

    Value() = default;
    explicit Value(Type t) : type(t) {
        if (t == Type::Array) arr = new std::vector<Value>();
        if (t == Type::Object) obj = new std::vector<std::pair<std::string, Value>>();
    }
    Value(const Value& o);
    Value& operator=(const Value& o);
    ~Value();
    Value(Value&& o) noexcept;
    Value& operator=(Value&& o) noexcept;

    bool is_obj() const { return type == Type::Object && obj; }
    bool is_arr() const { return type == Type::Array && arr; }
    bool is_str() const { return type == Type::String; }
    bool is_bool() const { return type == Type::Bool; }
    bool is_num() const { return type == Type::Number; }
    bool is_null() const { return type == Type::Null; }

    // Доступ к полю объекта. Возвращает отсутствующий узел (Null), если нет.
    const Value* get(const char* key) const;
    // Индексация массива.
    const Value& at(size_t i) const;
    size_t size() const;
    std::string as_string() const;
    bool as_bool() const;
    double as_number() const;
    long long as_long() const;
};

// Parse: возвращает true при успехе. value будет заполнен.
bool parse(const std::string& text, Value& out, std::string* err = nullptr);

// Удобная сериализация (для settings/instances сохранения).
std::string dump(const Value& v, int indent = 2, int depth = 0);

} // namespace sl::json