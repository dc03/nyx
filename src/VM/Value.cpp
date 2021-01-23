/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "Value.hpp"

#include "../Common.hpp"
#include "../Module.hpp"

#include <algorithm>

Value::Value(int value) : as{value} {}
Value::Value(bool value) : as{value} {}
Value::Value(double value) : as{value} {}
Value::Value(std::nullptr_t) : as{nullptr} {}
Value::Value(const std::string &value) : as{value} {}
Value::Value(std::string &&value) : as{std::move(value)} {}
Value::Value(Value *referred) : as{referred} {}
Value::Value(RuntimeFunction *function) : as{function} {}
Value::Value(List list) : as{std::move(list)} {}

bool Value::operator==(Value &other) const noexcept {
    if (is_ref()) {
        if (other.is_ref()) {
            return *to_referred() == *other.to_referred();
        } else {
            return *to_referred() == other;
        }
    } else {
        return as == other.as;
    }
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
        std::string &string_value = to_string();
        std::string result{};
        auto is_escape = [](char ch) {
            switch (ch) {
                case '\b':
                case '\n':
                case '\r':
                case '\t':
                case '\\': return true;
                default: return false;
            }
        };
        auto repr_escape = [](char ch) {
            switch (ch) {
                case '\b': return "\\b";
                case '\n': return "\\n";
                case '\r': return "\\r";
                case '\t': return "\\t";
                case '\\': return "\\\\";
                default: return "";
            }
        };
        result.reserve(string_value.size() + std::count_if(string_value.begin(), string_value.end(), is_escape));
        for (char ch : string_value) {
            if (is_escape(ch)) {
                result += repr_escape(ch);
            } else {
                result += ch;
            }
        }
        return "\""s + result + "\""s;
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

Value::operator bool() noexcept {
    if (is_int()) {
        return to_int() != 0;
    } else if (is_bool()) {
        return to_bool();
    } else if (is_double()) {
        return to_double() != 0;
    } else if (is_string()) {
        return !to_string().empty();
    } else if (is_null()) {
        return false;
    } else {
        unreachable();
    }
}