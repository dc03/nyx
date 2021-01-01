#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef TOKEN_HPP
#define TOKEN_HPP

#include "TokenTypes.hpp"

#include <string>

struct Token {
    TokenType type;
    std::string lexeme;
    std::size_t line;
    std::size_t start;
    std::size_t end;

    Token() = default;

    Token(TokenType type, std::string lexeme, std::size_t line, std::size_t start, std::size_t end)
        : type{type}, lexeme{std::move(lexeme)}, line{line}, start{start}, end{end} {}

    bool operator==(const Token &other) { return lexeme == other.lexeme; }
};

#endif
