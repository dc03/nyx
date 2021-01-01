/* Copyright (C) 2020  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "Value.hpp"

#include "../Common.hpp"
#include "../Module.hpp"

Value::Value(Value &&other) noexcept : tag{NULL_}, as{} {
    *this = std::move(other);
}

Value &Value::operator=(Value &&other) noexcept {
    if (other.tag != STRING && tag == STRING) {
        delete[] as.string;
    }
    switch (other.tag) {
        case INT: as.integer = other.as.integer; break;
        case DOUBLE: as.real = other.as.real; break;
        case STRING: {
            if (tag != STRING) {
                as.string = nullptr;
            }
            std::swap(as.string, other.as.string);
            break;
        }
        case BOOL: as.boolean = other.as.boolean; break;
        case NULL_: as.null = nullptr; break;
        case PRIMITIVE_REF: as.reference = other.as.reference; break;
        case FUNCTION: as.function = other.as.function; break;
    }
    tag = other.tag;
    return *this;
}

Value::Value(const Value &other) : tag{NULL_}, as{} {
    *this = other;
}

Value &Value::operator=(const Value &other) {
    if (other.tag != STRING && tag == STRING) {
        delete[] as.string;
    }
    switch (other.tag) {
        case INT: as.integer = other.as.integer; break;
        case DOUBLE: as.real = other.as.real; break;
        case STRING: {
            if (tag != STRING) {
                as.string = nullptr;
            }
            if (other.as.string == nullptr) {
                delete[] as.string;
                as.string = nullptr;
                return *this;
            }
            std::size_t len = std::strlen(other.as.string);
            if (as.string == nullptr) {
                as.string = new char[len + 1];
            }
            std::memcpy(as.string, other.as.string, len + 1);
            as.string[len] = '\0';
            break;
        }
        case BOOL: as.boolean = other.as.boolean; break;
        case NULL_: as.null = nullptr; break;
        case PRIMITIVE_REF: as.reference = other.as.reference; break;
        case FUNCTION: as.function = other.as.function; break;
    }
    tag = other.tag;
    return *this;
}

Value::~Value() {
    if (tag == Value::STRING) {
        delete[] as.string;
    }
}

Value::Value(int value) : tag{Value::INT}, as{value} {}
Value::Value(bool value) : tag{Value::BOOL}, as{value} {}
Value::Value(double value) : tag{Value::DOUBLE}, as{value} {}
Value::Value(std::nullptr_t) : tag{Value::NULL_}, as{nullptr} {}
Value::Value(const char *value) : tag{Value::STRING}, as{value} {}
Value::Value(Value *referred) : tag{Value::PRIMITIVE_REF}, as{referred} {}
Value::Value(RuntimeFunction *function) : tag{Value::FUNCTION}, as{function} {}

bool Value::operator==(const Value &other) const noexcept {
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

std::string Value::repr() const noexcept {
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