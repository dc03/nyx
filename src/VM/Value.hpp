#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef VALUE_HPP
#define VALUE_HPP

#include "List.hpp"
#include "SharedUniquePtr.hpp"

#include <string>
#include <variant>
#include <vector>

struct RuntimeFunction;

struct Value {
    enum tag { INT, DOUBLE, STRING, BOOL, NULL_, PRIMITIVE_REF, FUNCTION, LIST };
    std::variant<int, double, std::string, bool, std::nullptr_t, Value *, RuntimeFunction *, SharedUniquePtr<List>> as;

    Value() = default;

    explicit Value(int value);
    explicit Value(double value);
    explicit Value(const std::string &value);
    explicit Value(std::string &&value);
    explicit Value(bool value);
    explicit Value(std::nullptr_t);
    explicit Value(Value *referred);
    explicit Value(RuntimeFunction *function);
    explicit Value(List &&list);
    explicit Value(const SharedUniquePtr<List> &list);

    // clang-format off
    [[nodiscard]] bool is_int()      const noexcept { return as.index() == Value::tag::INT; }
    [[nodiscard]] bool is_double()   const noexcept { return as.index() == Value::tag::DOUBLE; }
    [[nodiscard]] bool is_string()   const noexcept { return as.index() == Value::tag::STRING; }
    [[nodiscard]] bool is_bool()     const noexcept { return as.index() == Value::tag::BOOL; }
    [[nodiscard]] bool is_null()     const noexcept { return as.index() == Value::tag::NULL_; }
    [[nodiscard]] bool is_numeric()  const noexcept { return as.index() == Value::tag::INT || as.index() == Value::tag::DOUBLE; }
    [[nodiscard]] bool is_ref()      const noexcept { return as.index() == Value::tag::PRIMITIVE_REF; }
    [[nodiscard]] bool is_function() const noexcept { return as.index() == Value::tag::FUNCTION; }
    [[nodiscard]] bool is_list()     const noexcept { return as.index() == Value::tag::LIST; }

    [[nodiscard]] int &to_int()                        noexcept { return std::get<INT>(as); }
    [[nodiscard]] double &to_double()                  noexcept { return std::get<DOUBLE>(as); }
    [[nodiscard]] std::string &to_string()             noexcept { return std::get<STRING>(as); }
    [[nodiscard]] bool &to_bool()                      noexcept { return std::get<BOOL>(as); }
    [[nodiscard]] std::nullptr_t &to_null()            noexcept { return std::get<NULL_>(as); }
    [[nodiscard]] double to_numeric()            const noexcept { return is_int() ? std::get<INT>(as) : std::get<DOUBLE>(as); }
    [[nodiscard]] Value *to_referred()           const noexcept { return std::get<PRIMITIVE_REF>(as); }
    [[nodiscard]] RuntimeFunction *to_function() const noexcept { return std::get<FUNCTION>(as); }
    [[nodiscard]] List &to_list()                      noexcept { return *std::get<LIST>(as); }
    // clang-format on

    [[nodiscard]] bool operator==(Value &other) noexcept;
    [[nodiscard]] std::string repr() noexcept;
    [[nodiscard]] explicit operator bool() noexcept;
};

#endif