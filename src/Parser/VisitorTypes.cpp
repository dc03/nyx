/* See LICENSE at project root for license details */
#include "../AST.hpp"

LiteralValue::LiteralValue(int value): value{value} {}
LiteralValue::LiteralValue(bool value): value{value} {}
LiteralValue::LiteralValue(double value): value{value} {}
LiteralValue::LiteralValue(std::nullptr_t): value{nullptr} {}
LiteralValue::LiteralValue(std::string value): value{value} {}

ExprTypeInfo::ExprTypeInfo(QualifiedTypeInfo info, Token token): info{info}, lexeme{std::move(token)} {}
ExprTypeInfo::ExprTypeInfo(QualifiedTypeInfo info, FunctionStmt *func, Token token):
    info{info}, func{func}, lexeme{std::move(token)} {}
ExprTypeInfo::ExprTypeInfo(QualifiedTypeInfo info, ClassStmt *class_, Token token):
    info{info}, class_{class_}, lexeme{std::move(token)} {}