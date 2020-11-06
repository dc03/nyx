#pragma once

/* See LICENSE at project root for license details */

#ifndef INSTRUCTIONS_HPP
#define INSTRUCTIONS_HPP

#include <cstddef>

enum class Instruction : unsigned char {
    HALT,
//    INT_SHORT, // 1 byte int literal
//    INT_LONG,  // 3 byte int literal
    CONST_SHORT, // first 255 constant values
    CONST_LONG,  // rest of the constant values

    ADDS, // string concatenation

    ADDI, // Integer addition
    ADDF, // Float addition
    SUBI, // Integer subtraction
    SUBF, // Float subtraction
    MULI, // Integer multiplication
    MULF, // Float multiplication
    DIVI, // Integer division
    DIVF, // Float division

    SHL,     // Bitwise shift-left
    SHR,     // Bitwise shift-right
    BIT_AND, // Bitwise and
    BIT_OR,  // Bitwise or
    BIT_NOT, // Bitwise not
    BIT_XOR, // Bitwise xor

    NOT, // Logical not

    NEGATE, // Unary minus
};

#endif