/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "Backend/RuntimeContext.hpp"

#include "Backend/VirtualMachine/Value.hpp"
// Really weird compile errors without this include
// Basically, `Chunk` in `RuntimeModule` only forward declares `Value`, but does not include `Value` header file, so
// trying to access the `std::vector<RuntimeModule>` in any manner fails because it is made up of an incomplete type

RuntimeModule *RuntimeContext::get_module_string(const std::string &module) noexcept {
    auto it = module_path_map.find(module);
    if (it != module_path_map.end()) {
        return &compiled_modules[it->second];
    } else {
        return nullptr;
    }
}

RuntimeModule *RuntimeContext::get_module_path(const std::filesystem::path &path) noexcept {
    return get_module_string(path.c_str());
}

std::size_t RuntimeContext::get_module_index_string(const std::string &module) noexcept {
    auto it = module_path_map.find(module);
    if (it != module_path_map.end()) {
        return it->second;
    } else {
        return static_cast<std::size_t>(-1);
    }
}

std::size_t RuntimeContext::get_module_index_path(const std::filesystem::path &path) noexcept {
    return get_module_index_string(path.c_str());
}