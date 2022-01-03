/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "AST/AST.hpp"
#include "Common.hpp"

LiteralValue::LiteralValue(int value) : value{value} {}
LiteralValue::LiteralValue(bool value) : value{value} {}
LiteralValue::LiteralValue(double value) : value{value} {}
LiteralValue::LiteralValue(std::nullptr_t) : value{nullptr} {}
LiteralValue::LiteralValue(const std::string &value) : value{value} {}
LiteralValue::LiteralValue(std::string &&value) : value{std::move(value)} {}

ExprSynthesizedAttrs::ExprSynthesizedAttrs(
    QualifiedTypeInfo info, Token token, bool is_lvalue, ScopeAccessType scope_type)
    : info{info}, token{std::move(token)}, is_lvalue{is_lvalue}, scope_type{scope_type} {}
ExprSynthesizedAttrs::ExprSynthesizedAttrs(
    QualifiedTypeInfo info, FunctionStmt *func, Token token, bool is_lvalue, ScopeAccessType scope_type)
    : info{info}, func{func}, token{std::move(token)}, is_lvalue{is_lvalue}, scope_type{scope_type} {}
ExprSynthesizedAttrs::ExprSynthesizedAttrs(
    QualifiedTypeInfo info, ClassStmt *class_, Token token, bool is_lvalue, ScopeAccessType scope_type)
    : info{info}, class_{class_}, token{std::move(token)}, is_lvalue{is_lvalue}, scope_type{scope_type} {}
ExprSynthesizedAttrs::ExprSynthesizedAttrs(
    QualifiedTypeInfo info, std::size_t module_index, Token token, ScopeAccessType scope_type)
    : info{info}, module_index{module_index}, token{std::move(token)}, is_lvalue{false}, scope_type{scope_type} {}
ExprSynthesizedAttrs::ExprSynthesizedAttrs(QualifiedTypeInfo info, FunctionStmt *func, ClassStmt *class_, Token token,
    bool is_lvalue, ScopeAccessType scope_type)
    : info{info}, func{func}, class_{class_}, token{std::move(token)}, is_lvalue{is_lvalue}, scope_type{scope_type} {}

NodeType ExprInheritedAttrs::type_tag() {
    switch (parent.index()) {
        case EXPRESSION: return std::get<EXPRESSION>(parent)->type_tag();
        case STATEMENT: return std::get<STATEMENT>(parent)->type_tag();
        case TYPE: return std::get<TYPE>(parent)->type_tag();
    }
}