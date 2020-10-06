#pragma once

/* See LICENSE at project root for license details */

#ifndef OBJECT_HPP
#  define OBJECT_HPP

#include <string>
#include <variant>

#include "../Token.hpp"

enum class TypeType {
    BOOL,
    INT,
    FLOAT,
    STRING,
    CLASS,
    LIST,
    TYPEOF,
    NULL_
};

struct Object {
    std::variant<int, double, std::string, bool, std::nullptr_t> value;
    TypeType object_type{};
    Token lexeme{};

    Object(int value);
    Object(double value);
    Object(std::string value);
    Object(bool value);
    Object(std::nullptr_t);

    Object(TypeType type, Token token);

    void set_lexeme(const Token &lexeme) noexcept;
};

using T = Object;

#endif