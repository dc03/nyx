#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef NATIVES_HPP
#define NATIVES_HPP

#include "Value.hpp"

struct NativeFn {
    Value (*code)(Value *);
    std::size_t arity;
};

Value native_print(Value *args);
Value native_int(Value *args);
Value native_float(Value *args);
Value native_string(Value *args);

#endif