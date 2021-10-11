/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "Frontend/CompileContext.hpp"

#include "Backend/VirtualMachine/Value.hpp"
// Really weird compile errors without this include
// Basically, `Chunk` in `RuntimeModule` only forward declares `Value`, but does not include `Value` header file, so
// trying to access the `std::vector<RuntimeModule>` in any manner fails because it is made up of an incomplete type

#include <algorithm>

Module *CompileContext::get_module_string(const std::string &module) noexcept {
    auto it = module_path_map.find(module);
    if (it != module_path_map.end()) {
        return &parsed_modules[it->second].first;
    } else {
        return nullptr;
    }
}

Module *CompileContext::get_module_path(const std::filesystem::path &path) noexcept {
    return get_module_string(path.c_str());
}

std::size_t CompileContext::get_module_index_string(const std::string &module) const noexcept {
    auto it = module_path_map.find(module);
    if (it != module_path_map.end()) {
        return it->second;
    } else {
        return static_cast<size_t>(-1);
    }
}

std::size_t CompileContext::get_module_index_path(const std::filesystem::path &path) const noexcept {
    return get_module_index_string(path.c_str());
}

void CompileContext::sort_modules() {
    std::sort(parsed_modules.begin(), parsed_modules.end(),
        [](const auto &x1, const auto &x2) { return x1.second > x2.second; });

    for (std::size_t i = 0; i < parsed_modules.size(); i++) {
        module_path_map[parsed_modules[i].first.name] = i;
    }
}