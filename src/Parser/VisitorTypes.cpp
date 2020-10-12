/* See LICENSE at project root for license details */
#include "../AST.hpp"

LiteralValue::LiteralValue(bool value): value{value} {}
LiteralValue::LiteralValue(double value): value{value} {}
LiteralValue::LiteralValue(std::nullptr_t): value{nullptr} {}
LiteralValue::LiteralValue(std::string value): value{value} {}

ExprTypeInfo::ExprTypeInfo(QualifiedTypeInfo info, Token token): info{info}, lexeme{std::move(token)} {}
ExprTypeInfo::ExprTypeInfo(FunctionStmt *func, Token token): func{func}, lexeme{std::move(token)} {}
ExprTypeInfo::ExprTypeInfo(ClassStmt *class_, Token token): class_{class_}, lexeme{std::move(token)} {}