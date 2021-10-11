#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef INSTRUCTIONS_HPP
#define INSTRUCTIONS_HPP

enum class Instruction {
    HALT,
    POP,
    /* Push constants on stack */
    CONSTANT,
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
    ACCESS_LOCAL,
    MAKE_REF_TO_LOCAL,
    DEREF,
    /* Global variable operations */
    ASSIGN_GLOBAL,
    ACCESS_GLOBAL,
    MAKE_REF_TO_GLOBAL,
    /* Function calls */
    LOAD_FUNCTION_SAME_MODULE,
    LOAD_FUNCTION_MODULE_INDEX,
    LOAD_FUNCTION_MODULE_PATH,
    CALL_FUNCTION,
    CALL_NATIVE,
    RETURN,
    TRAP_RETURN,
    /* String instructions */
    CONSTANT_STRING,
    INDEX_STRING,
    CHECK_STRING_INDEX,
    POP_STRING,
    CONCATENATE,
    /* List instructions */
    MAKE_LIST,
    COPY_LIST,
    APPEND_LIST,
    POP_FROM_LIST,
    ASSIGN_LIST,
    INDEX_LIST,
    MAKE_REF_TO_INDEX,
    CHECK_LIST_INDEX,
    ACCESS_LOCAL_LIST,
    ACCESS_GLOBAL_LIST,
    ASSIGN_LOCAL_LIST,
    ASSIGN_GLOBAL_LIST,
    POP_LIST,
    /* Miscellaneous */
    ACCESS_FROM_TOP,
    ASSIGN_FROM_TOP,
    EQUAL_SL, // Equality operation for lists and strings
    /* Move instructions */
    MOVE_LOCAL,
    MOVE_GLOBAL,
    MOVE_INDEX,
    /* Swap instructions */
    SWAP, // Swaps the top two values on the stack
};

#endif