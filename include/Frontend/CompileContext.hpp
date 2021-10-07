#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef COMPILE_CONTEXT_HPP
#define COMPILE_CONTEXT_HPP

#include "Backend/VirtualMachine/Module.hpp"
#include "ErrorLogger/ErrorLogger.hpp"

#include <vector>

struct CompileContext {
    std::size_t main_module{};
    std::vector<std::pair<Module, std::size_t>> parsed_modules{};
};

#endif