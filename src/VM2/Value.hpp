#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef VM2_VALUE_HPP
#define VM2_VALUE_HPP

#include "Module.hpp"
#include "StringCacher.hpp"

#include <cstdint>
#include <string>

struct Value {
    struct PlaceHolder {};

    using IntType = std::int32_t;
    using FloatType = double;
    using StringType = const HashedString *;
    using BoolType = bool;
    using NullType = std::nullptr_t;
    using ReferenceType = Value *;
    using FunctionType = RuntimeFunction *;
    using ListType = std::vector<Value>;

    union {
        PlaceHolder w_invalid;

        IntType w_int;
        FloatType w_float;
        StringType w_str;
        BoolType w_bool;
        NullType w_null;
        ReferenceType w_ref;
        FunctionType w_fun;
        ListType *w_list;
    };

    enum class Tag { INVALID, INT, FLOAT, STRING, BOOL, NULL_, REF, FUNCTION, LIST } tag;

    Value() noexcept;
    explicit Value(IntType value) noexcept;
    explicit Value(FloatType value) noexcept;
    explicit Value(StringType value) noexcept;
    explicit Value(BoolType value) noexcept;
    explicit Value(NullType value) noexcept;
    explicit Value(ReferenceType value) noexcept;
    explicit Value(FunctionType value) noexcept;
    explicit Value(ListType *value) noexcept;

    [[nodiscard]] std::string repr() const noexcept;
    [[nodiscard]] explicit operator bool() const noexcept;
    [[nodiscard]] bool operator==(const Value &other) const noexcept;
    [[nodiscard]] bool operator<(const Value &other) const noexcept;
    [[nodiscard]] bool operator>(const Value &other) const noexcept;
};

#endif