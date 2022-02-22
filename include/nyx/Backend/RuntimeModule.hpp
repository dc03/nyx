#pragma once

/* Copyright (C) 2020-2022  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef RUNTIME_MODULE_HPP
#define RUNTIME_MODULE_HPP

#include "nyx/AST/AST.hpp"
#include "nyx/Backend/VirtualMachine/Chunk.hpp"

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
    std::size_t module_index{};
};

struct RuntimeModule {
    Chunk top_level_code{};
    Chunk teardown_code{};
    std::unordered_map<std::string, RuntimeFunction> functions{};
    std::string name{};
    std::filesystem::path path{};

    RuntimeModule() noexcept = default;

    RuntimeModule(RuntimeModule &&other) noexcept = default;

    RuntimeModule &operator=(RuntimeModule &&other) noexcept = default;

    RuntimeModule(const RuntimeModule &other) = delete;

    RuntimeModule &operator=(const RuntimeModule &other) = delete;
};

#endif