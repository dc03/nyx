#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef COMPILE_MANAGER_HPP
#define COMPILE_MANAGER_HPP

#include "Backend/CodeGen/CodeGen.hpp"
#include "Frontend/CompileContext.hpp"
#include "Frontend/Parser/Parser.hpp"
#include "Frontend/Parser/TypeResolver.hpp"
#include "Frontend/Scanner/Scanner.hpp"

class CompileManager {
    CompileContext *ctx{};

    Module module{};

    Scanner scanner{};
    Parser parser{};
    TypeResolver resolver{};

  public:
    CompileManager() noexcept = default;
    CompileManager(std::string_view module_path);
};

#endif