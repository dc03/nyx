/* See LICENSE at project root for license details */
#include "Object.hpp"

#include <utility>

Object::Object(int value): value{value}, object_type{TypeType::INT} {}
Object::Object(bool value): value{value}, object_type{TypeType::BOOL} {}
Object::Object(double value): value{value}, object_type{TypeType::FLOAT} {}
Object::Object(std::nullptr_t): value{nullptr}, object_type{TypeType::NULL_} {}
Object::Object(std::string value): value{value}, object_type{TypeType::STRING} {}
Object::Object(TypeType type, Token token): value{nullptr}, object_type{type}, lexeme{std::move(token)} {}

void Object::set_lexeme(const Token &token) noexcept {
    this->lexeme = token;
}