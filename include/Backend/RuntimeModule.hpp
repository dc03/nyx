#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef RUNTIME_MODULE_HPP
#define RUNTIME_MODULE_HPP

#include "AST/AST.hpp"
#include "Backend/VirtualMachine/Chunk.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

struct RuntimeModule;

struct RuntimeFunction {
    Chunk code{};
    std::size_t arity{};
    std::string name{};
    RuntimeModule *module{};
};

struct RuntimeModule {
    Chunk top_level_code{};
    std::unordered_map<std::string, RuntimeFunction> functions{};
    std::string name{};
    std::filesystem::path path{};

    RuntimeModule() noexcept = default;

    RuntimeModule(RuntimeModule &&other) noexcept
        : top_level_code{std::move(other.top_level_code)},
          functions{std::move(other.functions)},
          name{std::move(other.name)} {}

    RuntimeModule &operator=(RuntimeModule &&other) noexcept {
        top_level_code = std::move(other.top_level_code);
        functions = std::move(other.functions);
        name = std::move(other.name);
        return *this;
    }

    RuntimeModule(const RuntimeModule &other) = delete;

    RuntimeModule &operator=(const RuntimeModule &other) = delete;
};

#endif