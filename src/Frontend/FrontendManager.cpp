/* Copyright (C) 2020-2022  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "nyx/Frontend/FrontendManager.hpp"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

FrontendManager::FrontendManager(FrontendContext *ctx, fs::path path, bool is_main, std::size_t module_depth)
    : ctx{ctx} {
    if (is_main) {
        ctx->main_parent_path = fs::absolute(path).parent_path();
    } else {
        path = ctx->main_parent_path / path.c_str();
    }

    if (fs::is_directory(path)) {
        ctx->logger.fatal_error({"'", std::string{path}, "' represents a directory, not a file"});
        return;
    } else if (not fs::exists(path)) {
        ctx->logger.fatal_error({"No such file: '", std::string{path}, "'"});
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
        ctx->logger.fatal_error({"Unable to open module '", module_name.c_str(), "'"});
    }

    std::string source{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};

    module = Module{module_name.c_str(), module_path, source};
    if (is_main) {
        ctx->main = &module;
    }
    scanner = Scanner{ctx, &module, module.source};

    parser = Parser{ctx, &scanner, &module, module_depth};
    resolver = TypeResolver{ctx, &module};
}

void FrontendManager::parse_module() {
    module.statements = parser.program();
}

void FrontendManager::check_module() {
    resolver.check(module.statements);
}

std::string &FrontendManager::module_name() {
    return module.name;
}

fs::path FrontendManager::module_path() const {
    return module.full_path.parent_path();
}

Module &FrontendManager::get_module() {
    return module;
}

Module FrontendManager::move_module() {
    return std::move(module);
}