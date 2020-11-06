#pragma once

/* See LICENSE at project root for license details */

#ifndef MODULE_HPP
#define MODULE_HPP

#include "AST.hpp"
#include "VM/Chunk.hpp"

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

struct RuntimeFunction {
    std::string name{};
    Value *stack_top{};
    Chunk code{};
    std::byte *ip{};
};

struct RuntimeModule {
    std::string name{};
    std::vector<RuntimeFunction> functions{};
    Chunk top_level_code{};
};

#endif