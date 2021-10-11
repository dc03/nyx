/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "Frontend/CompileManager.hpp"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

CompileManager::CompileManager(CompileContext *ctx, fs::path path, bool is_main, std::size_t module_depth) : ctx{ctx} {
    if (is_main) {
        ctx->main_parent_path = fs::absolute(path).parent_path();
    } else {
        path = ctx->main_parent_path / path.c_str();
    }

    if (fs::is_directory(path)) {
        compile_error({"'", std::string{path}, "' represents a directory, not a file"});
        return;
    } else if (not fs::exists(path)) {
        compile_error({"No such file: '", std::string{path}, "'"});
        return;
    }

    fs::path module_path = fs::absolute(path);

    fs::path module_name = module_path.filename().stem();
    fs::path module_parent = module_path.parent_path();

    if (is_main) {
        ctx->main_parent_path = module_parent;
    }

    std::ifstream file(module_path, std::ios::in);
    if (not file.is_open()) {
        compile_error({"Unable to open module '", module_name.c_str(), "'"});
    }

    std::string source{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};

    logger.set_module_name(module_name.native());
    logger.set_source(source);

    module = Module{module_name.c_str(), module_path, source};
    if (is_main) {
        ctx->main = &module;
    }
    scanner.set_source(module.source);

    parser = Parser{ctx, &scanner, &module, module_depth};
    resolver = TypeResolver{ctx, &module};
}

void CompileManager::parse_module() {
    module.statements = parser.program();
}

void CompileManager::check_module() {
    resolver.check(module.statements);
}

std::string &CompileManager::module_name() {
    return module.name;
}

fs::path CompileManager::module_path() const {
    return module.full_path.parent_path();
}

Module &CompileManager::get_module() {
    return module;
}

Module CompileManager::move_module() {
    return std::move(module);
}