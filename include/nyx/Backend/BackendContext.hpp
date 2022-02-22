#pragma once

/* Copyright (C) 2020-2022  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef BACKEND_CONTEXT_HPP
#define BACKEND_CONTEXT_HPP

#include "RuntimeModule.hpp"
#include "nyx/ErrorLogger/ErrorLogger.hpp"

#include <filesystem>
#include <string_view>
#include <unordered_map>
#include <vector>

class CLIConfig;

struct BackendContext {
    RuntimeModule *main{};

    std::vector<RuntimeModule> compiled_modules{};
    std::unordered_map<std::string, std::size_t> module_path_map{};

    const CLIConfig *config{};
    ErrorLogger logger{};

    RuntimeModule *get_module_string(const std::string &module) noexcept;
    RuntimeModule *get_module_path(const std::filesystem::path &path) noexcept;
    std::size_t get_module_index_string(const std::string &module) noexcept;
    std::size_t get_module_index_path(const std::filesystem::path &path) noexcept;

    void set_config(const CLIConfig *config);
};

#endif