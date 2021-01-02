#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef COMMON_HPP
#define COMMON_HPP

#ifdef _MSC_VER
#define assume(Cond)  __assume(Cond)
#define unreachable() __assume(0)
#elif defined(__clang__)
#define assume(Cond)  __builtin_assume(Cond)
#define unreachable() __builtin_unreachable()
#else
#define assume(Cond)  ((Cond) ? static_cast<void>(0) : __builtin_unreachable())
#define unreachable() __builtin_unreachable()
#endif

#define allocate_node(T, ...)                                                                                          \
    new T { __VA_ARGS__ }

#endif