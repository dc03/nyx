#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef VM2_VALUE_HPP
#define VM2_VALUE_HPP

#include "Module.hpp"

#include <cstdint>
#include <string>

struct Value {
    struct PlaceHolder {};

    using w_int_t = std::int32_t;
    using w_float_t = double;
    using w_str_t = char *;
    using w_bool_t = bool;
    using w_null_t = std::nullptr_t;
    using w_ref_t = Value *;
    using w_fun_t = RuntimeFunction *;

    union {
        PlaceHolder w_invalid;

        w_int_t w_int;
        w_float_t w_float;
        w_str_t w_str;
        w_bool_t w_bool;
        w_null_t w_null;
        w_ref_t w_ref;
        w_fun_t w_fun;
    };

    enum class Tag { INVALID, INT, FLOAT, STRING, BOOL, NULL_, REF, FUNCTION } tag;

    Value() noexcept;
    explicit Value(w_int_t value) noexcept;
    explicit Value(w_float_t value) noexcept;
    explicit Value(w_str_t value) noexcept;
    explicit Value(w_bool_t value) noexcept;
    explicit Value(w_null_t value) noexcept;
    explicit Value(w_ref_t value) noexcept;
    explicit Value(w_fun_t value) noexcept;

    [[nodiscard]] std::string repr() const noexcept;
    [[nodiscard]] explicit operator bool() const noexcept;
    [[nodiscard]] bool operator==(const Value &other) const noexcept;
    [[nodiscard]] bool operator<(const Value &other) const noexcept;
    [[nodiscard]] bool operator>(const Value &other) const noexcept;
};

#endif