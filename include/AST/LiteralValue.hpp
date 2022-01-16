#pragma once

/* Copyright (C) 2020-2022  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef LITERAL_VALUE_HPP
#define LITERAL_VALUE_HPP

#include <string>
#include <variant>

struct LiteralValue {
    enum tag { INT, DOUBLE, STRING, BOOL, NULL_ };

    using IntType = std::int32_t;
    using FloatType = double;
    using StringType = std::string;
    using BooleanType = bool;
    using NullType = std::nullptr_t;

    std::variant<IntType, FloatType, StringType, BooleanType, NullType> value{};

    LiteralValue() = default;
    explicit LiteralValue(int value);
    explicit LiteralValue(double value);
    explicit LiteralValue(const std::string &value);
    explicit LiteralValue(std::string &&value);
    explicit LiteralValue(bool value);
    explicit LiteralValue(std::nullptr_t);

    // clang-format off
    [[nodiscard]] bool is_int()     const noexcept { return value.index() == LiteralValue::tag::INT; }
    [[nodiscard]] bool is_float()   const noexcept { return value.index() == LiteralValue::tag::DOUBLE; }
    [[nodiscard]] bool is_string()  const noexcept { return value.index() == LiteralValue::tag::STRING; }
    [[nodiscard]] bool is_bool()    const noexcept { return value.index() == LiteralValue::tag::BOOL; }
    [[nodiscard]] bool is_null()    const noexcept { return value.index() == LiteralValue::tag::NULL_; }
    [[nodiscard]] bool is_numeric() const noexcept { return value.index() == LiteralValue::tag::INT || value.index() == LiteralValue::tag::DOUBLE; }

    [[nodiscard]] IntType &to_int()            noexcept { return std::get<INT>(value); }
    [[nodiscard]] FloatType &to_float()        noexcept { return std::get<DOUBLE>(value); }
    [[nodiscard]] StringType &to_string()      noexcept { return std::get<STRING>(value); }
    [[nodiscard]] BooleanType &to_bool()       noexcept { return std::get<BOOL>(value); }
    [[nodiscard]] NullType &to_null()          noexcept { return std::get<NULL_>(value); }
    [[nodiscard]] FloatType to_numeric() const noexcept { return is_int() ? std::get<INT>(value) : std::get<DOUBLE>(value); }
    // clang-format on

    [[nodiscard]] std::size_t index() const noexcept { return value.index(); }
};

#endif