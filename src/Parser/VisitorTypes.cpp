/* See LICENSE at project root for license details */
#include "../AST.hpp"

LiteralValue::LiteralValue(int value) : tag{LiteralValue::INT}, as{value} {}
LiteralValue::LiteralValue(bool value) : tag{LiteralValue::BOOL}, as{value} {}
LiteralValue::LiteralValue(double value) : tag{LiteralValue::DOUBLE}, as{value} {}
LiteralValue::LiteralValue(std::nullptr_t) : tag{LiteralValue::NULL_}, as{nullptr} {}
LiteralValue::LiteralValue(std::string value) : tag{LiteralValue::STRING}, as{value} {}
LiteralValue::LiteralValue(LiteralValue &&other) noexcept {
    tag = other.tag;
    switch (other.tag) {
        case INT: as.integer = other.as.integer; break;
        case DOUBLE: as.real = other.as.real; break;
        case STRING: as.string = std::move(other.as.string); break;
        case BOOL: as.boolean = other.as.boolean; break;
        case NULL_: as.null = nullptr; break;
    }
}
LiteralValue::~LiteralValue() {
    if (tag == LiteralValue::STRING) {
        as.string.std::string::~string();
    }
}

ExprTypeInfo::ExprTypeInfo(QualifiedTypeInfo info, Token token, bool is_lvalue)
    : info{info}, lexeme{std::move(token)}, is_lvalue{is_lvalue} {}
ExprTypeInfo::ExprTypeInfo(QualifiedTypeInfo info, FunctionStmt *func, Token token, bool is_lvalue)
    : info{info}, func{func}, lexeme{std::move(token)}, is_lvalue{is_lvalue} {}
ExprTypeInfo::ExprTypeInfo(QualifiedTypeInfo info, ClassStmt *class_, Token token, bool is_lvalue)
    : info{info}, class_{class_}, lexeme{std::move(token)}, is_lvalue{is_lvalue} {}