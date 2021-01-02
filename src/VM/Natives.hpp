#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef NATIVES_HPP
#define NATIVES_HPP

#include "../Parser/VisitorTypes.hpp"
#include "Value.hpp"

#include <string_view>
#include <vector>

struct NativeFn {
    Value (*code)(Value *);
    std::string name{};
    Type return_type{};
    std::vector<std::vector<Type>> arguments{};
    std::size_t arity;
};

extern std::vector<NativeFn> native_functions;

Value native_print(Value *args);
Value native_int(Value *args);
Value native_float(Value *args);
Value native_string(Value *args);
Value native_readline(Value *args);

#endif