#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef RUNTIME_MANAGER_HPP
#define RUNTIME_MANAGER_HPP

#include "Backend/CodeGenerators/ByteCodeGenerator.hpp"
#include "Backend/RuntimeContext.hpp"
#include "Backend/VirtualMachine/VirtualMachine.hpp"

class RuntimeManager {
    RuntimeContext *ctx{};

    RuntimeModule main{};

    ByteCodeGenerator generator{};
    VirtualMachine vm{};

  public:
    RuntimeManager() = default;
    explicit RuntimeManager(RuntimeContext *ctx);

    void compile(CompileContext *compile_ctx);
    void disassemble();
    void run();
};

#endif