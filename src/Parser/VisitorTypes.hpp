#pragma once

/* See LICENSE at project root for license details */

#ifndef AST_TYPES_HPP
#define AST_TYPES_HPP

#include "../Token.hpp"

#include <memory>
#include <string>
#include <variant>

enum class Type { BOOL, INT, FLOAT, STRING, CLASS, LIST, TYPEOF, NULL_, FUNCTION };

struct Expr;
struct BaseType;
struct ClassStmt;
struct FunctionStmt;

using QualifiedTypeInfo = BaseType *;

struct ExprTypeInfo {
    QualifiedTypeInfo info{nullptr};
    FunctionStmt *func{nullptr};
    ClassStmt *class_{nullptr};
    Token lexeme{};
    bool is_lvalue{};

    ExprTypeInfo() = default;
    ExprTypeInfo(QualifiedTypeInfo info, Token token, bool is_lvalue = false);
    ExprTypeInfo(QualifiedTypeInfo info, FunctionStmt *func, Token token, bool is_lvalue = false);
    ExprTypeInfo(QualifiedTypeInfo info, ClassStmt *class_, Token token, bool is_lvalue = false);
};

struct LiteralValue {
    enum { INT, DOUBLE, STRING, BOOL, NULL_ } tag;
    union as {
        int integer;
        double real;
        std::string string;
        bool boolean;
        std::nullptr_t null;

        as() { null = nullptr; }
        explicit as(int value) { integer = value; }
        explicit as(double value) { real = value; }
        explicit as(const std::string &value) { string = value; }
        explicit as(bool value) { boolean = value; }
        explicit as(std::nullptr_t) { null = nullptr; }
        ~as() {}
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