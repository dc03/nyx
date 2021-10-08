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
    std::sort(compile_ctx->parsed_modules.begin(), compile_ctx->parsed_modules.end(),
        [](const auto &x1, const auto &x2) { return x1.second > x2.second; });

    generator.set_compile_ctx(compile_ctx);

    if (compile_ctx->main != nullptr) {
        main = generator.compile(*compile_ctx->main);
        main.top_level_code.emit_instruction(Instruction::HALT, 0);
        ctx->main = &main;
    }

    for (auto &[module, depth] : compile_ctx->parsed_modules) {
        ctx->compiled_modules.emplace_back(generator.compile(module));
    }
}

void RuntimeManager::disassemble() {
    disassemble_ctx(ctx);
}

void RuntimeManager::run() {
    if (ctx->main != nullptr) {
        vm.run(main);
    }
}