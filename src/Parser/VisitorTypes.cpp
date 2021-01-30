/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "../AST.hpp"
#include "../Common.hpp"

LiteralValue::LiteralValue(int value) : value{value} {}
LiteralValue::LiteralValue(bool value) : value{value} {}
LiteralValue::LiteralValue(double value) : value{value} {}
LiteralValue::LiteralValue(std::nullptr_t) : value{nullptr} {}
LiteralValue::LiteralValue(const std::string &value) : value{value} {}
LiteralValue::LiteralValue(std::string &&value) : value{std::move(value)} {}

ExprTypeInfo::ExprTypeInfo(QualifiedTypeInfo info, Token token, bool is_lvalue)
    : info{info}, token{std::move(token)}, is_lvalue{is_lvalue}, scope_type{ScopeType::NONE} {}
ExprTypeInfo::ExprTypeInfo(QualifiedTypeInfo info, FunctionStmt *func, Token token, bool is_lvalue)
    : info{info}, func{func}, token{std::move(token)}, is_lvalue{is_lvalue}, scope_type{ScopeType::NONE} {}
ExprTypeInfo::ExprTypeInfo(QualifiedTypeInfo info, ClassStmt *class_, Token token, bool is_lvalue)
    : info{info}, class_{class_}, token{std::move(token)}, is_lvalue{is_lvalue}, scope_type{ScopeType::CLASS} {}
ExprTypeInfo::ExprTypeInfo(QualifiedTypeInfo info, std::size_t module_index, Token token)
    : info{info},
      module_index{module_index},
      token{std::move(token)},
      is_lvalue{false},
      scope_type{ScopeType::MODULE} {}
ExprTypeInfo::ExprTypeInfo(QualifiedTypeInfo info, FunctionStmt *func, ClassStmt *class_, Token token, bool is_lvalue)
    : info{info},
      func{func},
      class_{class_},
      token{std::move(token)},
      is_lvalue{is_lvalue},
      scope_type{ScopeType::NONE} {}
ExprTypeInfo::ExprTypeInfo(std::size_t stack_slot, bool is_ref) : stack_slot{stack_slot}, is_ref{is_ref} {}