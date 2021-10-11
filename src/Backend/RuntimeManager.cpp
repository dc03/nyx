/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "Backend/RuntimeManager.hpp"

#include "Backend/VirtualMachine/Disassembler.hpp"

#include <algorithm>

RuntimeManager::RuntimeManager(RuntimeContext *ctx) : ctx{ctx} {
    generator.set_runtime_ctx(ctx);
    vm.set_runtime_ctx(ctx);
}

void RuntimeManager::compile(CompileContext *compile_ctx) {
    compile_ctx->sort_modules();

    for (std::size_t i = 0; i < compile_ctx->parsed_modules.size(); i++) {
        ctx->module_path_map[compile_ctx->parsed_modules[i].first.full_path.c_str()] = i;
    }

    generator.set_compile_ctx(compile_ctx);

    for (auto &[module, depth] : compile_ctx->parsed_modules) {
        ctx->compiled_modules.emplace_back(generator.compile(module));
    }

    if (compile_ctx->main != nullptr) {
        main = generator.compile(*compile_ctx->main);
        main.top_level_code.emit_instruction(Instruction::HALT, 0);
        ctx->main = &main;
    }
}

void RuntimeManager::disassemble() {
    disassemble_ctx(ctx);
}

void RuntimeManager::run() {
    if (ctx->main != nullptr) {
        vm.set_function_module_pointers(ctx->main);
    }

    for (auto &module : ctx->compiled_modules) {
        vm.set_function_module_pointers(&module);
    }

    if (ctx->main != nullptr) {
        vm.run(main);
    }
}