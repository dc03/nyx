#pragma once

/* See LICENSE at project root for license details */

#ifndef MODULE_HPP
#define MODULE_HPP

#include "AST.hpp"

#include <string>
#include <string_view>
#include <vector>

struct Module {
    std::string name{};
    std::string module_directory{};
    std::vector<ClassStmt *> classes{};
    std::vector<FunctionStmt *> functions{};
    std::vector<stmt_node_t> statements{};
    std::vector<Module> imported{};

    explicit Module(std::string_view name, std::string_view dir) : name{name}, module_directory{dir} {}
};

#endif