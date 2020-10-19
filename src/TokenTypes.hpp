#pragma once

/* See LICENSE at project root for license details */

#ifndef TOKEN_TYPES_HPP
#    define TOKEN_TYPES_HPP

// Empty comments indicate precedence levels
enum class TokenType {
    //
    COMMA,
    //
    EQUAL,
    PLUS_EQUAL,
    MINUS_EQUAL,
    STAR_EQUAL,
    SLASH_EQUAL,
    QUESTION,
    COLON,
    //
    OR,
    //
    AND,
    //
    BIT_OR,
    //
    BIT_XOR,
    //
    BIT_AND,
    //
    NOT_EQUAL,
    EQUAL_EQUAL,
    //
    GREATER,
    GREATER_EQUAL,
    LESS,
    LESS_EQUAL,
    //
    RIGHT_SHIFT,
    LEFT_SHIFT,
    //
    MINUS,
    PLUS,
    //
    MODULO,
    SLASH,
    STAR,
    //
    NOT,
    BIT_NOT,
    PLUS_PLUS,
    MINUS_MINUS,
    //
    DOT,
    LEFT_PAREN,
    RIGHT_PAREN,
    LEFT_INDEX,
    RIGHT_INDEX,
    LEFT_BRACE,
    RIGHT_BRACE,

    // Semicolon
    SEMICOLON,
    // Arrow
    ARROW,

    // Literals
    IDENTIFIER,
    STRING_VALUE,
    INT_VALUE,
    FLOAT_VALUE,

    // Keywords
    /* AND, */ BOOL,
    BREAK,
    CASE,
    CLASS,
    CONST,
    CONTINUE,
    DEFAULT,
    ELSE,
    FALSE,
    FLOAT,
    FN,
    FOR,
    IF,
    IMPORT,
    INT,
    NULL_,
    /* OR, */ PROTECTED,
    PRIVATE,
    PUBLIC,
    REF,
    RETURN,
    STRING,
    SUPER,
    SWITCH,
    THIS,
    TRUE,
    TYPE,
    TYPEOF,
    VAL,
    VAR,
    WHILE,

    NONE,
    END_OF_LINE,
    END_OF_FILE
};

#endif
