#pragma once

/* Copyright (C) 2020-2022  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef FRONTEND_CONTEXT_HPP
#define FRONTEND_CONTEXT_HPP

#include "Module.hpp"
#include "nyx/ErrorLogger/ErrorLogger.hpp"

#include <filesystem>
#include <unordered_map>
#include <vector>

class CLIConfig;

struct FrontendContext {
    Module *main{};
    std::filesystem::path main_parent_path{};

    std::vector<std::pair<Module, std::size_t>> parsed_modules{};
    std::unordered_map<std::string, std::size_t> module_path_map{};

    const CLIConfig *config{};
    ErrorLogger logger{};

    Module *get_module_string(const std::string &module) noexcept;
    Module *get_module_path(const std::filesystem::path &path) noexcept;
    std::size_t get_module_index_string(const std::string &module) const noexcept;
    std::size_t get_module_index_path(const std::filesystem::path &path) const noexcept;

    void set_config(const CLIConfig *config);
    void sort_modules();
};

#endif