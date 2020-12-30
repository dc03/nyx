#pragma once

/* Copyright (C) 2020  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef INSTRUCTIONS_HPP
#define INSTRUCTIONS_HPP

#include <cstddef>

enum class Instruction : unsigned char {
    HALT, // Stop the interpreter
    POP,  // Pop the value at the top of the stack

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

    TRUE,  // True
    FALSE, // False
    NULL_, // Null

    ACCESS_LOCAL_SHORT, // Get local from stack
    ACCESS_LOCAL_LONG,

    JUMP_FORWARD, // For Turing completeness (I think)
    JUMP_BACKWARD,
    JUMP_IF_FALSE,
    JUMP_IF_TRUE,
    POP_JUMP_IF_EQUAL,
    POP_JUMP_IF_FALSE,
    POP_JUMP_BACK_IF_TRUE,

    ASSIGN_LOCAL, // To assign to a value on the stack
    MAKE_REF_TO_LOCAL,
};

#endif