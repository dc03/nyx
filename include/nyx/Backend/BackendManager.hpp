#pragma once

/* Copyright (C) 2020-2022  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef BACKEND_MANAGER_HPP
#define BACKEND_MANAGER_HPP

#include "BackendContext.hpp"
#include "nyx/Backend/CodeGenerators/ByteCodeGenerator.hpp"
#include "nyx/Backend/VirtualMachine/VirtualMachine.hpp"

class BackendManager {
    BackendContext *ctx{};

    RuntimeModule main{};

    ByteCodeGenerator generator{};
    VirtualMachine vm{};

  public:
    BackendManager() = default;
    explicit BackendManager(BackendContext *ctx);

    void compile(FrontendContext *compile_ctx);
    void disassemble();
    void run();
};

#endif