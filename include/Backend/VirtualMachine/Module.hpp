#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef MODULE_HPP
#define MODULE_HPP

#include "AST/AST.hpp"
#include "Chunk.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct Module {
    std::string name{};
    std::string module_directory{};
    std::unordered_map<std::string_view, ClassStmt *> classes{};
    std::unordered_map<std::string_view, FunctionStmt *> functions{};
    std::vector<StmtNode> statements{};
    std::vector<std::size_t> imported{}; // Indexes into Parser::parsed_modules (better than pointers)

    explicit Module(std::string_view name, std::string_view dir) : name{name}, module_directory{dir} {}

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