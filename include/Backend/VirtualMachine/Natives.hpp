#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef NATIVES_HPP
#define NATIVES_HPP

#include "AST/VisitorTypes.hpp"
#include "Value.hpp"

#include <string>
#include <vector>

class VirtualMachine;

struct NativeFn {
    Value (*code)(VirtualMachine &, Value *);
    std::string name{};
    Type return_type{};
    std::vector<std::vector<Type>> arguments{};
    std::size_t arity;
};

extern std::vector<NativeFn> native_functions;

Value native_print(VirtualMachine &vm, Value *args);
Value native_int(VirtualMachine &vm, Value *args);
Value native_float(VirtualMachine &vm, Value *args);
Value native_string(VirtualMachine &vm, Value *args);
Value native_readline(VirtualMachine &vm, Value *args);
Value native_size(VirtualMachine &vm, Value *args);

#endif