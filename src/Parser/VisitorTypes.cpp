/* See LICENSE at project root for license details */
#include "../AST.hpp"

LiteralValue::LiteralValue(bool value): value{value} {}
LiteralValue::LiteralValue(double value): value{value} {}
LiteralValue::LiteralValue(std::nullptr_t): value{nullptr} {}
LiteralValue::LiteralValue(std::string value): value{value} {}
ExprTypeInfo::ExprTypeInfo(QualifiedTypeInfo info, Token token): info{info}, lexeme{std::move(token)} {}
ExprTypeInfo::ExprTypeInfo(FunctionStmt *stmt, Token token): func{stmt}, lexeme{std::move(token)} {}