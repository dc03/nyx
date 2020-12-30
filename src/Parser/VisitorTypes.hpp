#pragma once

/* Copyright (C) 2020  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef AST_TYPES_HPP
#define AST_TYPES_HPP

#include "../Token.hpp"

#include <memory>
#include <string>

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
    enum { INT, DOUBLE, STRING, BOOL, NULL_ } tag;
    union as {
        int integer;
        double real;
        std::string string;
        bool boolean;
        std::nullptr_t null;

        as() : string{} {}
        ~as() {}
        explicit as(int value) : integer{value} {}
        explicit as(double value) : real{value} {}
        explicit as(const std::string &value) : string{value} {}
        explicit as(std::string &&value) : string{std::move(value)} {}
        explicit as(bool value) : boolean{value} { boolean = value; }
        explicit as(std::nullptr_t) : null{nullptr} {}
    } as;

    explicit LiteralValue(int value);
    explicit LiteralValue(double value);
    explicit LiteralValue(const std::string &value);
    explicit LiteralValue(bool value);
    explicit LiteralValue(std::nullptr_t);

    LiteralValue(LiteralValue &&other) noexcept;
    ~LiteralValue();
};

using StmtVisitorType = void;
using ExprVisitorType = ExprTypeInfo;
using BaseTypeVisitorType = QualifiedTypeInfo;

#endif
