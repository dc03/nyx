/* See LICENSE at project root for license details */
#include "../AST.hpp"

LiteralValue::LiteralValue(int value) : value{value} {}
LiteralValue::LiteralValue(bool value) : value{value} {}
LiteralValue::LiteralValue(double value) : value{value} {}
LiteralValue::LiteralValue(std::nullptr_t) : value{nullptr} {}
LiteralValue::LiteralValue(std::string value) : value{value} {}

ExprTypeInfo::ExprTypeInfo(QualifiedTypeInfo info, Token token, bool is_lvalue)
    : info{info}, lexeme{std::move(token)}, is_lvalue{is_lvalue} {}
ExprTypeInfo::ExprTypeInfo(QualifiedTypeInfo info, FunctionStmt *func, Token token, bool is_lvalue)
    : info{info}, func{func}, lexeme{std::move(token)}, is_lvalue{is_lvalue} {}
ExprTypeInfo::ExprTypeInfo(QualifiedTypeInfo info, ClassStmt *class_, Token token, bool is_lvalue)
    : info{info}, class_{class_}, lexeme{std::move(token)}, is_lvalue{is_lvalue} {}