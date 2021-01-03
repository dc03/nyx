#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef AST_TYPES_HPP
#define AST_TYPES_HPP

#include "../Token.hpp"

#include <memory>
#include <string>
#include <variant>

enum class Type { BOOL, INT, FLOAT, STRING, CLASS, LIST, TYPEOF, NULL_, FUNCTION, MODULE };

struct Expr;
struct BaseType;
struct ClassStmt;
struct FunctionStmt;

using QualifiedTypeInfo = BaseType *;

struct ExprTypeInfo {
    QualifiedTypeInfo info{nullptr};
    FunctionStmt *func{nullptr};
    ClassStmt *class_{nullptr};
    // I'm using unions here to make different names for things with the same type which are used exclusively to each
    // other
    union {
        std::size_t module_index;
        std::size_t stack_slot;
    };
    Token lexeme{};
    union {
        bool is_lvalue;
        bool is_ref; // Although this is tracked in `info`, CodeGen does not use that parameter
    };
    enum class ScopeType { CLASS, MODULE, NONE } scope_type{};

    ExprTypeInfo() = default;
    ExprTypeInfo(const ExprTypeInfo &) = default;
    ExprTypeInfo(QualifiedTypeInfo info, Token token, bool is_lvalue = false);
    ExprTypeInfo(QualifiedTypeInfo info, FunctionStmt *func, Token token, bool is_lvalue = false);
    ExprTypeInfo(QualifiedTypeInfo info, ClassStmt *class_, Token token, bool is_lvalue = false);
    ExprTypeInfo(QualifiedTypeInfo info, std::size_t module_index, Token token);
    ExprTypeInfo(QualifiedTypeInfo info, FunctionStmt *func, ClassStmt *class_, Token token, bool is_lvalue = false);
    ExprTypeInfo(
        std::size_t stack_slot, bool is_ref = false); // This is meant for use in CodeGen for making references work
};

struct LiteralValue {
    enum tag { INT, DOUBLE, STRING, BOOL, NULL_ };
    std::variant<int, double, std::string, bool, std::nullptr_t> value;

    explicit LiteralValue(int value);
    explicit LiteralValue(double value);
    explicit LiteralValue(const std::string &value);
    explicit LiteralValue(std::string &&value);
    explicit LiteralValue(bool value);
    explicit LiteralValue(std::nullptr_t);

    // clang-format off
    [[nodiscard]] bool is_int()     const noexcept { return value.index() == LiteralValue::tag::INT; }
    [[nodiscard]] bool is_double()  const noexcept { return value.index() == LiteralValue::tag::DOUBLE; }
    [[nodiscard]] bool is_string()  const noexcept { return value.index() == LiteralValue::tag::STRING; }
    [[nodiscard]] bool is_bool()    const noexcept { return value.index() == LiteralValue::tag::BOOL; }
    [[nodiscard]] bool is_null()    const noexcept { return value.index() == LiteralValue::tag::NULL_; }
    [[nodiscard]] bool is_numeric() const noexcept { return value.index() == LiteralValue::tag::INT || value.index() == LiteralValue::tag::DOUBLE; }

    [[nodiscard]] int &to_int()                   noexcept { return std::get<INT>(value); }
    [[nodiscard]] double &to_double()             noexcept { return std::get<DOUBLE>(value); }
    [[nodiscard]] std::string &to_string()        noexcept { return std::get<STRING>(value); }
    [[nodiscard]] bool &to_bool()                 noexcept { return std::get<BOOL>(value); }
    [[nodiscard]] std::nullptr_t &to_null()       noexcept { return std::get<NULL_>(value); }
    [[nodiscard]] double to_numeric()       const noexcept { return is_int() ? std::get<INT>(value) : std::get<DOUBLE>(value); }
    // clang-format on

    [[nodiscard]] std::size_t index() const noexcept { return value.index(); }
};

using StmtVisitorType = void;
using ExprVisitorType = ExprTypeInfo;
using BaseTypeVisitorType = QualifiedTypeInfo;

#endif
