#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef MODULE_HPP
#define MODULE_HPP

#include "AST/AST.hpp"
#include "Chunk.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

struct Module {
    std::string name{};
    std::filesystem::path module_directory{};
    std::string source{};
    std::unordered_map<std::string_view, ClassStmt *> classes{};
    std::unordered_map<std::string_view, FunctionStmt *> functions{};
    std::vector<StmtNode> statements{};
    std::vector<std::size_t> imported{}; // Indexes into `parsed_modules` in the current `CompilerContext`

    Module() noexcept = default;
    explicit Module(std::string_view name, std::filesystem::path dir, std::string source)
        : name{name}, module_directory{std::move(dir)}, source{std::move(source)} {}

    Module(const Module &) = default;
    Module &operator=(const Module &) = default;
    Module(Module &&) noexcept = default;
    Module &operator=(Module &&) noexcept = default;
    ~Module() = default;
};

struct RuntimeFunction {
    Chunk code{};
    std::size_t arity{};
    std::string name{};
};

struct RuntimeModule {
    Chunk top_level_code{};
    std::unordered_map<std::string, RuntimeFunction> functions{};
    std::string name{};
};

#endif