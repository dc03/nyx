#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef RUNTIME_MANAGER_HPP
#define RUNTIME_MANAGER_HPP

#include "Backend/RuntimeContext.hpp"
#include "Backend/VirtualMachine/VirtualMachine.hpp"
#include "Backend/CodeGen/CodeGen.hpp"

class RuntimeManager {
    RuntimeContext *ctx{};

    Generator generator{};
    VirtualMachine vm{};
};

#endif