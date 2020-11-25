#pragma once

/* Copyright (C) 2020  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef INSTRUCTIONS_HPP
#define INSTRUCTIONS_HPP

#include <cstddef>

enum class Instruction : unsigned char {
    HALT, // Stop the interpreter
    POP,  // Pop the value at the top of the stack

    //    INT_SHORT, // 1 byte int literal
    //    INT_LONG,  // 3 byte int literal
    CONST_SHORT, // first 255 constant values
    CONST_LONG,  // rest of the constant values

    CONCAT, // string concatenation

    ADD, // Addition
    SUB, // Subtraction
    MUL, // Multiplication
    DIV, // Division
    MOD, // Integer modulo

    SHIFT_LEFT,  // Bitwise shift-left
    SHIFT_RIGHT, // Bitwise shift-right
    BIT_AND,     // Bitwise and
    BIT_OR,      // Bitwise or
    BIT_NOT,     // Bitwise not
    BIT_XOR,     // Bitwise xor

    NOT,     // Logical not
    EQUAL,   // Logical equal
    GREATER, // Logical greater
    LESSER,  // Logical lesser

    NEGATE, // Unary minus
};

#endif