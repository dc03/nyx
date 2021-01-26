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
Value::Value(List &&list) : as{SharedUniquePtr<List>::create(std::move(list))} {}
Value::Value(const SharedUniquePtr<List> &list) : as{SharedUniquePtr<List>{list}} {}

bool Value::operator==(Value &other) noexcept {
    if (is_ref()) {
        if (other.is_ref()) {
            return *to_referred() == *other.to_referred();
        } else {
            return *to_referred() == other;
        }
    } else {
        assume(as.index() == other.as.index());
        if (is_int()) {
            return to_int() == other.to_int();
        } else if (is_double()) {
            return to_double() == other.to_double();
        } else if (is_string()) {
            return to_string() == other.to_string();
        } else if (is_bool()) {
            return to_bool() == other.to_bool();
        } else if (is_null()) {
            return true;
        } else if (is_list()) {
            return to_list() == other.to_list();
        } else if (is_function()) {
            return to_function() == other.to_function();
        }
    }
    unreachable();
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
                case '\'':
                case '\"':
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
                case '\'': return "\\\'";
                case '\"': return "\\\"";
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
    } else if (is_list()) {
        using namespace std::string_literals;
        std::string result = "[";
        List &list = to_list();
        auto stringify_elems = [&result](auto &arg) {
            if (arg.empty()) {
                return;
            }
            auto begin = arg.begin();
            for (; begin != arg.end() - 1; begin++) {
                Value element{*begin};
                result += element.repr() + ", ";
            }
            result += Value{*begin}.repr();
        };
        if (list.is_int_list()) {
            stringify_elems(list.to_int_list());
        } else if (list.is_float_list()) {
            stringify_elems(list.to_float_list());
        } else if (list.is_string_list()) {
            stringify_elems(list.to_string_list());
        } else if (list.is_bool_list()) {
            auto begin = list.to_bool_list().begin();
            for (; begin != list.to_bool_list().end() - 1; begin++) {
                result += (*begin ? "true"s : "false"s) + ", ";
            }
            result += (*begin ? "true" : "false");
        } else if (list.is_ref_list()) {
            stringify_elems(list.to_ref_list());
        } else if (list.is_list_list()) {
            if (list.to_list_list().empty()) {
                return "[]";
            }
            auto begin = list.to_list_list().begin();
            for (; begin != list.to_list_list().end() - 1; begin++) {
                Value as_value{SharedUniquePtr<List>{*begin}};
                result += as_value.repr() + ", ";
            }
            Value as_value{SharedUniquePtr<List>{*begin}};
            result += as_value.repr();
        }
        result += "]";
        return result;
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