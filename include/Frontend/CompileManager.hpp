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

#include <filesystem>

class CompileManager {
    CompileContext *ctx{};

    Module module{};

    Scanner scanner{};
    Parser parser{};
    TypeResolver resolver{};

  public:
    CompileManager() noexcept = default;
    CompileManager(CompileContext *ctx, std::filesystem::path path, bool is_main, std::size_t module_depth = 0);

    void parse_module();
    void check_module();

    std::string &module_name();
    std::filesystem::path module_path() const;

    Module &get_module();
    Module move_module();
};

#endif