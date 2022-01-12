#pragma once

/* Copyright (C) 2020-2022  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef COMMON_HPP
#define COMMON_HPP

#include <cassert>

#ifdef _MSC_VER
#define assume(Cond) __assume(Cond)
#define unreachable()                                                                                                  \
    do {                                                                                                               \
        assert(not "Error: Control flow cannot reach here.");                                                          \
        __assume(0);                                                                                                   \
    } while (0)
#elif defined(__clang__)
#define assume(Cond) __builtin_assume(Cond)
#define unreachable()                                                                                                  \
    do {                                                                                                               \
        assert(not "Error: Control flow cannot reach here.");                                                          \
        __builtin_unreachable();                                                                                       \
    } while (0)
#else
#define assume(Cond) ((Cond) ? static_cast<void>(0) : __builtin_unreachable())
#define unreachable()                                                                                                  \
    do {                                                                                                               \
        assert(not "Error: Control flow cannot reach here.");                                                          \
        __builtin_unreachable();                                                                                       \
    } while (0)
#endif

#define allocate_node(T, ...)                                                                                          \
    new T { __VA_ARGS__ }

// Disable VirtualMachine's tracing at compile time
#ifndef NO_TRACE_VM
#define NO_TRACE_VM 0
#else
#define NO_TRACE_VM 1
#endif

#endif