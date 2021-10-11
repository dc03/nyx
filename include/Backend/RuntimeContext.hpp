#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef COMPILER_CONTEXT_HPP
#define COMPILER_CONTEXT_HPP

#include "ErrorLogger/ErrorLogger.hpp"
#include "RuntimeModule.hpp"

#include <filesystem>
#include <string_view>
#include <unordered_map>
#include <vector>

struct RuntimeContext {
    RuntimeModule *main{};

    std::vector<RuntimeModule> compiled_modules{};
    std::unordered_map<std::string, std::size_t> module_path_map{};

    RuntimeModule *get_module_string(const std::string &module) noexcept;
    RuntimeModule *get_module_path(const std::filesystem::path &path) noexcept;
    std::size_t get_module_index_string(const std::string &module) noexcept;
    std::size_t get_module_index_path(const std::filesystem::path &path) noexcept;
};

#endif