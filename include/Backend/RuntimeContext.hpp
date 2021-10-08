#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef COMPILER_CONTEXT_HPP
#define COMPILER_CONTEXT_HPP

#include "Backend/VirtualMachine/Module.hpp"
#include "ErrorLogger/ErrorLogger.hpp"

#include <vector>

struct RuntimeContext {
    RuntimeModule *main{};

    std::vector<RuntimeModule> compiled_modules{};
};

#endif