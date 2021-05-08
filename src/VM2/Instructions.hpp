#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef VM2_INSTRUCTIONS_HPP
#define VM2_INSTRUCTIONS_HPP

enum class Instruction {
    HALT,
    POP,
    /* Push constants on stack */
    CONST_SHORT,
    CONST_LONG,
    /* Integer operations */
    IADD,
    ISUB,
    IMUL,
    IDIV,
    IMOD,
    INEG, // (unary -)
    /* Floating point operations */
    FADD,
    FSUB,
    FMUL,
    FDIV,
    FMOD,
    FNEG, // (unary -)
    /* Floating <-> integral conversions */
    FLOAT_TO_INT,
    INT_TO_FLOAT,
    /* Bitwise operations */
    SHIFT_LEFT,
    SHIFT_RIGHT,
    BIT_AND,
    BIT_OR,
    BIT_NOT,
    BIT_XOR,
    /* Logical operations */
    NOT,
    EQUAL,
    GREATER,
    LESSER,
    /* Constant operations */
    PUSH_TRUE,
    PUSH_FALSE,
    PUSH_NULL,
    /* Jump operations */
    JUMP_FORWARD,
    JUMP_BACKWARD,
    JUMP_IF_TRUE,
    JUMP_IF_FALSE,
    POP_JUMP_IF_EQUAL,
    POP_JUMP_IF_FALSE,
    POP_JUMP_BACK_IF_TRUE,
    /* Local variable operations */
    ASSIGN_LOCAL,
    ACCESS_LOCAL_SHORT,
    ACCESS_LOCAL_LONG,
    MAKE_REF_TO_LOCAL,
    DEREF,
    /* Global variable operations */
    ASSIGN_GLOBAL,
    ACCESS_GLOBAL_SHORT,
    ACCESS_GLOBAL_LONG,
    MAKE_REF_TO_GLOBAL,
    /* Function calls */
    LOAD_FUNCTION,
    CALL_FUNCTION,
    CALL_NATIVE,
    RETURN,
    TRAP_RETURN,
    /* Copying */
    COPY,
};

#endif