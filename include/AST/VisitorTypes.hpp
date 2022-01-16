#pragma once

/* Copyright (C) 2020-2022  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef AST_TYPES_HPP
#define AST_TYPES_HPP

#include "AST/LiteralValue.hpp"
#include "AST/Token.hpp"

#include <memory>
#include <string>
#include <variant>

enum class Type { BOOL, INT, FLOAT, STRING, CLASS, LIST, TYPEOF, NULL_, FUNCTION, MODULE, TUPLE };

struct Expr;
struct Stmt;
struct BaseType;
struct ClassStmt;
struct FunctionStmt;

enum class NodeType;

using QualifiedTypeInfo = BaseType *;

struct ExprSynthesizedAttrs {
    QualifiedTypeInfo info{nullptr};
    FunctionStmt *func{nullptr};
    ClassStmt *class_{nullptr};
    // I'm using unions here to make different names for things with the same type which are used exclusively to each
    // other
    union {
        std::size_t module_index{};
        std::size_t stack_slot;
    };
    Token token{};
    bool is_lvalue{};
    enum class ScopeAccessType { CLASS, MODULE, CLASS_METHOD, MODULE_CLASS, MODULE_FUNCTION, NONE } scope_type{};

    ExprSynthesizedAttrs() = default;
    ExprSynthesizedAttrs(const ExprSynthesizedAttrs &) = default;
    ExprSynthesizedAttrs(QualifiedTypeInfo info, Token token, bool is_lvalue = false,
        ScopeAccessType scope_type = ScopeAccessType::NONE);
    ExprSynthesizedAttrs(QualifiedTypeInfo info, FunctionStmt *func, Token token, bool is_lvalue = false,
        ScopeAccessType scope_type = ScopeAccessType::NONE);
    ExprSynthesizedAttrs(QualifiedTypeInfo info, ClassStmt *class_, Token token, bool is_lvalue = false,
        ScopeAccessType scope_type = ScopeAccessType::CLASS);
    ExprSynthesizedAttrs(QualifiedTypeInfo info, std::size_t module_index, Token token,
        ScopeAccessType scope_type = ScopeAccessType::MODULE);
    ExprSynthesizedAttrs(QualifiedTypeInfo info, FunctionStmt *func, ClassStmt *class_, Token token,
        bool is_lvalue = false, ScopeAccessType scope_type = ScopeAccessType::CLASS_METHOD);
};

struct ExprInheritedAttrs {
    enum { EXPRESSION, STATEMENT, TYPE };
    std::variant<Expr *, Stmt *, BaseType *> parent{};

    NodeType type_tag();
};

using StmtVisitorType = void;
using ExprVisitorType = ExprSynthesizedAttrs;
using BaseTypeVisitorType = QualifiedTypeInfo;

#endif
