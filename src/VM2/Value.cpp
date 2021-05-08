/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "Value.hpp"

#include "../Common.hpp"

#include <algorithm>
#include <cstring>

Value::Value() noexcept : w_invalid{}, tag{Tag::INVALID} {}
Value::Value(w_int_t value) noexcept : w_int{value}, tag{Tag::INT} {}
Value::Value(w_float_t value) noexcept : w_float{value}, tag{Tag::FLOAT} {}
Value::Value(w_str_t value) noexcept : w_str{value}, tag{Tag::STRING} {}
Value::Value(w_bool_t value) noexcept : w_bool{value}, tag{Tag::BOOL} {}
Value::Value(w_null_t value) noexcept : w_null{value}, tag{Tag::NULL_} {}
Value::Value(w_ref_t value) noexcept : w_ref{value}, tag{Tag::REF} {}
Value::Value(w_fun_t value) noexcept : w_fun{value}, tag{Tag::FUNCTION} {}

std::string Value::repr() const noexcept {
    if (tag == Tag::INT) {
        return std::to_string(w_int);
    } else if (tag == Tag::FLOAT) {
        return std::to_string(w_float);
    } else if (tag == Tag::STRING) {
        using namespace std::string_literals;
        std::string string_value{w_str};
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
    } else if (tag == Tag::BOOL) {
        return w_bool ? "true" : "false";
    } else if (tag == Tag::NULL_) {
        return "null";
    } else if (tag == Tag::REF) {
        char name[35];
        std::sprintf(name, "ref to %p", reinterpret_cast<void *>(w_ref));
        return {name};
    } else if (tag == Tag::FUNCTION) {
        char name[50];
        std::sprintf(name, "<function %s at %p>", w_fun->name.c_str(), reinterpret_cast<void *>(w_fun));
        return {name};
    } else if (tag == Tag::INVALID) {
        return {"<invalid!>"};
    }

    unreachable();
}

Value::operator bool() const noexcept {
    if (tag == Tag::INT) {
        return w_int != 0;
    } else if (tag == Tag::FLOAT) {
        return w_float != 0;
    } else if (tag == Tag::STRING) {
        return w_str != nullptr && *w_str != '\0';
    } else if (tag == Tag::BOOL) {
        return w_bool;
    } else if (tag == Tag::NULL_) {
        return false;
    } else if (tag == Tag::REF) {
        return (bool)(*w_ref);
    } else if (tag == Tag::FUNCTION) {
        return true;
    } else if (tag == Tag::INVALID) {
        return false;
    }

    unreachable();
}

bool Value::operator==(const Value &other) const noexcept {
    if (tag != Tag::REF && tag != other.tag) {
        return false;
    } else if (tag == Tag::INT) {
        return w_int == other.w_int;
    } else if (tag == Tag::FLOAT) {
        return w_float == other.w_float;
    } else if (tag == Tag::STRING) {
        return std::strcmp(w_str, other.w_str) == 0;
    } else if (tag == Tag::BOOL) {
        return w_bool == other.w_bool;
    } else if (tag == Tag::NULL_) {
        return true;
    } else if (tag == Tag::REF) {
        if (other.tag == Tag::REF) {
            return (w_ref == other.w_ref) || (*w_ref == *other.w_ref);
        } else {
            return *w_ref == other;
        }
    } else if (tag == Tag::FUNCTION) {
        return w_fun == other.w_fun;
    } else if (tag == Tag::INVALID) {
        return true;
    }

    unreachable();
}

bool Value::operator<(const Value &other) const noexcept {
    if (tag != Tag::REF && tag != other.tag) {
        return false;
    } else if (tag == Tag::INT) {
        return w_int < other.w_int;
    } else if (tag == Tag::FLOAT) {
        return w_float < other.w_float;
    } else if (tag == Tag::STRING) {
        return w_str != other.w_str && std::strcmp(w_str, other.w_str) < 0;
    } else if (tag == Tag::BOOL) {
        return w_bool == other.w_bool;
    } else if (tag == Tag::NULL_) {
        return true;
    } else if (tag == Tag::REF) {
        if (other.tag == Tag::REF) {
            return w_ref != other.w_ref && *w_ref < *other.w_ref;
        } else {
            return *w_ref < other;
        }
    } else if (tag == Tag::INVALID) {
        return false;
    }

    unreachable();
}

bool Value::operator>(const Value &other) const noexcept {
    if (tag != Tag::REF && tag != other.tag) {
        return false;
    } else if (tag == Tag::INT) {
        return w_int < other.w_int;
    } else if (tag == Tag::FLOAT) {
        return w_float < other.w_float;
    } else if (tag == Tag::STRING) {
        return w_str != other.w_str && std::strcmp(w_str, other.w_str) > 0;
    } else if (tag == Tag::BOOL) {
        return w_bool == other.w_bool;
    } else if (tag == Tag::NULL_) {
        return true;
    } else if (tag == Tag::REF) {
        if (other.tag == Tag::REF) {
            return w_ref != other.w_ref && *w_ref > *other.w_ref;
        } else {
            return *w_ref > other;
        }
    } else if (tag == Tag::INVALID) {
        return false;
    }

    unreachable();
}