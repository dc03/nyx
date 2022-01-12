#pragma once

/* Copyright (C) 2020-2022  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef MODULE_HPP
#define MODULE_HPP

#include "AST/AST.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

struct Module {
    std::string name{};
    std::filesystem::path full_path{};
    std::string source{};
    std::unordered_map<std::string_view, ClassStmt *> classes{};
    std::unordered_map<std::string_view, FunctionStmt *> functions{};
    std::vector<StmtNode> statements{};
    std::vector<std::size_t> imported{};        // Indexes into `parsed_modules` in the current `CompilerContext`
    std::vector<TypeNode> type_scratch_space{}; // Stores temporary types allocated in TypeResolver

    Module() noexcept = default;
    explicit Module(std::string_view name, std::filesystem::path full_path, std::string source)
        : name{name}, full_path{std::move(full_path)}, source{std::move(source)} {}

    Module(const Module &) = default;
    Module &operator=(const Module &) = default;
    Module(Module &&) noexcept = default;
    Module &operator=(Module &&) noexcept = default;
    ~Module() = default;
};

#endif