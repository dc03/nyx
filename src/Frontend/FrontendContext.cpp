/* Copyright (C) 2020-2022  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "nyx/Frontend/FrontendContext.hpp"

#include "nyx/CLIConfigParser.hpp"

#include <algorithm>

Module *FrontendContext::get_module_string(const std::string &module) noexcept {
    auto it = module_path_map.find(module);
    if (it != module_path_map.end()) {
        return &parsed_modules[it->second].first;
    } else {
        return nullptr;
    }
}

Module *FrontendContext::get_module_path(const std::filesystem::path &path) noexcept {
    return get_module_string(path.c_str());
}

std::size_t FrontendContext::get_module_index_string(const std::string &module) const noexcept {
    auto it = module_path_map.find(module);
    if (it != module_path_map.end()) {
        return it->second;
    } else {
        return static_cast<size_t>(-1);
    }
}

std::size_t FrontendContext::get_module_index_path(const std::filesystem::path &path) const noexcept {
    return get_module_index_string(path.c_str());
}

void FrontendContext::set_config(const CLIConfig *config) {
    this->config = config;

    if (config->contains(NO_COLORIZE_OUTPUT)) {
        logger.set_color(false);
    }
}

void FrontendContext::sort_modules() {
    std::sort(parsed_modules.begin(), parsed_modules.end(),
        [](const auto &x1, const auto &x2) { return x1.second > x2.second; });

    for (std::size_t i = 0; i < parsed_modules.size(); i++) {
        module_path_map[parsed_modules[i].first.name] = i;
    }
}