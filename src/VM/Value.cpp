/* Copyright (C) 2020  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "Value.hpp"

#include "../Common.hpp"

Value::Value(Value &&other) noexcept {
    *this = std::move(other);
}

Value &Value::operator=(Value &&other) noexcept {
    tag = other.tag;
    switch (other.tag) {
        case INT: as.integer = other.as.integer; break;
        case DOUBLE: as.real = other.as.real; break;
        case STRING: as.string = std::move(other.as.string); break;
        case BOOL: as.boolean = other.as.boolean; break;
        case NULL_: as.null = nullptr; break;
    }
    return *this;
}

Value::Value(const Value &other) {
    *this = other;
}

Value &Value::operator=(const Value &other) {
    tag = other.tag;
    switch (other.tag) {
        case INT: as.integer = other.as.integer; break;
        case DOUBLE: as.real = other.as.real; break;
        case STRING: as.string = other.as.string; break;
        case BOOL: as.boolean = other.as.boolean; break;
        case NULL_: as.null = nullptr; break;
    }
    return *this;
}

Value::~Value() {
    if (tag == Value::STRING) {
        as.string.std::string::~string();
    }
}

Value::Value(int value) : tag{Value::INT}, as{value} {}
Value::Value(bool value) : tag{Value::BOOL}, as{value} {}
Value::Value(double value) : tag{Value::DOUBLE}, as{value} {}
Value::Value(std::nullptr_t) : tag{Value::NULL_}, as{nullptr} {}
Value::Value(std::string value) : tag{Value::STRING}, as{value} {}