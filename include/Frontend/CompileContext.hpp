#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef COMPILE_CONTEXT_HPP
#define COMPILE_CONTEXT_HPP

#include "ErrorLogger/ErrorLogger.hpp"
#include "Frontend/Module.hpp"

#include <filesystem>
#include <unordered_map>
#include <vector>

struct CompileContext {
    Module *main{};
    std::filesystem::path main_parent_path{};

    std::vector<std::pair<Module, std::size_t>> parsed_modules{};
    std::unordered_map<std::string, std::size_t> module_path_map{};

    ErrorLogger logger{};

    Module *get_module_string(const std::string &module) noexcept;
    Module *get_module_path(const std::filesystem::path &path) noexcept;
    std::size_t get_module_index_string(const std::string &module) const noexcept;
    std::size_t get_module_index_path(const std::filesystem::path &path) const noexcept;

    void sort_modules();
};

#endif