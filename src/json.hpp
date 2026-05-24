#ifndef TINYJSON_JSON_HPP
#define TINYJSON_JSON_HPP
#include <string>
#include <vector>

#include "util/variant.hpp"

namespace json {
struct Number;
struct String;
struct Boolean;
struct Null;
struct Array;
struct Object;

using Value = Variant<Number, String, Boolean, Null, Array, Object>;

struct Number {
    double value;
};

struct String {
    std::string value;
};

struct Boolean {
    bool value;
};

struct Null {
};

struct Array {
    std::vector<Value> value;
};

struct Object {
    struct KeyValue;
    std::vector<KeyValue> children;

    template <class T>
    auto find(std::string_view key) -> T* {
        const auto p = find(key);
        if(!p) {
            return nullptr;
        }
        return p->get<T>();
    }

    template <class T>
    auto find(std::string_view key) const -> const T* {
        return const_cast<Object*>(this)->find<T>(key);
    }

    auto find(std::string_view key) -> Value*;
    auto find(std::string_view key) const -> const Value*;
    auto operator[](std::string_view key) -> Value&;
};

struct Object::KeyValue {
    std::string key;
    Value       value;
};

// helper
template <class Arg>
auto array_append(Array& array, Arg&& arg) -> void {
    array.value.push_back(Value::create<std::remove_cvref_t<Arg>>(std::move(arg)));
}

template <class Arg, class... Args>
auto array_append(Array& array, Arg&& arg, Args&&... args) -> void {
    array.value.push_back(Value::create<std::remove_cvref_t<Arg>>(std::move(arg)));
    array_append(array, std::forward<Args>(args)...);
}

template <class... Args>
auto make_array(Args&&... args) -> Array {
    auto array = Array();
    array.value.reserve(sizeof...(Args));
    array_append(array, std::forward<Args>(args)...);
    return array;
}

template <class Arg>
auto object_append(Object& object, const std::string_view key, Arg&& arg) -> void {
    object[key] = Value::create<std::remove_cvref_t<Arg>>(std::move(arg));
}

template <class Arg, class... Args>
auto object_append(Object& object, const std::string_view key, Arg&& arg, Args&&... args) -> void {
    object_append(object, key, std::move(arg));
    object_append(object, std::forward<Args>(args)...);
}

template <class Arg, class... Args>
auto make_object(const std::string_view key, Arg&& arg, Args&&... args) -> Object {
    auto object = Object();
    object_append(object, key, std::move(arg), std::forward<Args>(args)...);
    return object;
}

// parser.cpp
struct ParseOpts {
    bool allow_comments        = true;
    bool allow_trailing_commas = true;
};
auto parse(std::string_view str, ParseOpts opts = {}) -> std::optional<Object>;

// deparser.cpp
auto deparse(const Object& object) -> std::string;
} // namespace json

#endif // TINYJSON_JSON_HPP
