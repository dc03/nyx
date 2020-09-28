#pragma once

#ifndef TOKEN_TYPES_HPP
#  define TOKEN_TYPES_HPP

enum class TokenType {
    // Single character tokens
    LEFT_PAREN, RIGHT_PAREN, LEFT_BRACE, RIGHT_BRACE,
    COMMA, DOT, SEMICOLON, COLON, QUESTION_MARK,
    BIT_XOR, BIT_AND, BIT_OR, BIT_NEGATE, MODULO,

    // One or two character tokens
    MINUS, MINUS_EQUAL,
    PLUS, PLUS_EQUAL,
    SLASH, SLASH_EQUAL,
    STAR, STAR_EQUAL,
    BANG, BANG_EQUAL,
    EQUAL, EQUAL_EQUAL,
    GREATER, GREATER_EQUAL,
    LEFT_SHIFT, RIGHT_SHIFT,
    LESS, LESS_EQUAL,

    // Literals
    IDENTIFIER, STRING, INTEGER, FLOAT,

    // Keywords
   

    END_OF_FILE
};

#endif
