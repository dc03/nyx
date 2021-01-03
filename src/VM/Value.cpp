/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "Value.hpp"

#include "../Common.hpp"
#include "../Module.hpp"

Value::Value(Value &&other) noexcept : tag{tag::NULL_}, as{} {
    *this = std::move(other);
}

Value &Value::operator=(Value &&other) noexcept {
    if (other.tag != tag::STRING && tag == tag::STRING) {
        as.string.~basic_string<char>();
    }
    switch (other.tag) {
        case tag::INT: as.integer = other.as.integer; break;
        case tag::DOUBLE: as.real = other.as.real; break;
        case tag::STRING: {
            if (tag != tag::STRING) {
                new(&as.string) std::string{};
            }
            as.string = std::move(other.as.string);
            break;
        }
        case tag::BOOL: as.boolean = other.as.boolean; break;
        case tag::NULL_: as.null = nullptr; break;
        case tag::PRIMITIVE_REF: as.reference = other.as.reference; break;
        case tag::FUNCTION: as.function = other.as.function; break;
    }
    tag = other.tag;
    return *this;
}

Value::Value(const Value &other) : tag{tag::NULL_}, as{} {
    *this = other;
}

Value &Value::operator=(const Value &other) {
    if (other.tag != tag::STRING && tag == tag::STRING) {
        as.string.~basic_string<char>();
    }
    switch (other.tag) {
        case tag::INT: as.integer = other.as.integer; break;
        case tag::DOUBLE: as.real = other.as.real; break;
        case tag::STRING: {
            if (tag != tag::STRING) {
                new(&as.string) std::string{};
            }
            as.string = other.as.string;
            break;
        }
        case tag::BOOL: as.boolean = other.as.boolean; break;
        case tag::NULL_: as.null = nullptr; break;
        case tag::PRIMITIVE_REF: as.reference = other.as.reference; break;
        case tag::FUNCTION: as.function = other.as.function; break;
    }
    tag = other.tag;
    return *this;
}

Value::~Value() {
    if (tag == Value::tag::STRING) {
        as.string.~basic_string<char>();
    }
}

Value::Value(int value) : tag{Value::tag::INT}, as{value} {}
Value::Value(bool value) : tag{Value::tag::BOOL}, as{value} {}
Value::Value(double value) : tag{Value::tag::DOUBLE}, as{value} {}
Value::Value(std::nullptr_t) : tag{Value::tag::NULL_}, as{nullptr} {}
Value::Value(const std::string &value) : tag{Value::tag::STRING}, as{value} {}
Value::Value(std::string &&value) : tag{Value::tag::STRING}, as{std::move(value)} {}
Value::Value(Value *referred) : tag{Value::tag::PRIMITIVE_REF}, as{referred} {}
Value::Value(RuntimeFunction *function) : tag{Value::tag::FUNCTION}, as{function} {}

bool Value::operator==(Value &other) noexcept {
    if (is_numeric()) {
        return to_numeric() == other.to_numeric();
    } else if (is_string()) {
        return to_string() == other.to_string();
    } else if (is_bool()) {
        return to_bool() == other.to_bool();
    } else if (is_null()) {
        return true;
    } else if (is_function()) {
        return to_function() == other.to_function();
    } else if (is_ref()) {
        if (other.is_ref()) {
            return *to_referred() == *other.to_referred();
        } else {
            return *to_referred() == other;
        }
    }
    return false;
}

std::string Value::repr() noexcept {
    if (is_int()) {
        return std::to_string(to_int());
    } else if (is_double()) {
        return std::to_string(to_double());
    } else if (is_bool()) {
        return to_bool() ? "true" : "false";
    } else if (is_null()) {
        return "null";
    } else if (is_string()) {
        using namespace std::string_literals;
        return "\""s + to_string() + "\""s;
    } else if (is_ref()) {
        char name[15];
        std::sprintf(name, "ref to %p", reinterpret_cast<void *>(to_referred()));
        return std::string{name};
    } else if (is_function()) {
        using namespace std::string_literals;
        char name[15];
        std::sprintf(name, "%p", reinterpret_cast<void *>(to_function()));
        return "<function '"s + to_function()->name + "' at "s + std::string{name} + ">"s;
    }
    unreachable();
}