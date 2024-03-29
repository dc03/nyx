/* Copyright (C) 2020-2022  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "nyx/Backend/BackendManager.hpp"

#include "nyx/Backend/VirtualMachine/Disassembler.hpp"
#include "nyx/CLIConfigParser.hpp"
#include "nyx/Common.hpp"

#include <algorithm>

BackendManager::BackendManager(BackendContext *ctx) : ctx{ctx} {
    generator.set_runtime_ctx(ctx);
    vm.set_runtime_ctx(ctx);

#if !NO_TRACE_VM
#define HAS_OPT(value) std::find(opts.begin(), opts.end(), value) != opts.end()
    vm.colors_enabled = not ctx->config->contains(NO_COLORIZE_OUTPUT);
    if (ctx->config->contains(TRACE_EXEC)) {
        const auto &opts = ctx->config->get<std::vector<std::string>>(TRACE_EXEC);
        vm.debug_print_stack = HAS_OPT("stack");
        vm.debug_print_frames = HAS_OPT("frame");
        vm.debug_print_modules = HAS_OPT("module");
        vm.debug_print_instructions = HAS_OPT("insn");
        vm.debug_print_module_init = HAS_OPT("module_init");
    }
#undef HAS_OPT
#endif
}

void BackendManager::compile(FrontendContext *compile_ctx) {
    compile_ctx->sort_modules();

    for (std::size_t i = 0; i < compile_ctx->parsed_modules.size(); i++) {
        ctx->module_path_map[compile_ctx->parsed_modules[i].first.full_path.c_str()] = i;
    }

    generator.set_compile_ctx(compile_ctx);

    for (auto &[module, depth] : compile_ctx->parsed_modules) {
        ctx->compiled_modules.emplace_back(generator.compile(module));
        ctx->compiled_modules.back().top_level_code.emit_instruction(Instruction::HALT, 0);
        ctx->compiled_modules.back().teardown_code.emit_instruction(Instruction::HALT, 0);
    }

    if (compile_ctx->main != nullptr) {
        main = generator.compile(*compile_ctx->main);
        main.top_level_code.emit_instruction(Instruction::HALT, 0);
        main.teardown_code.emit_instruction(Instruction::HALT, 0);
        ctx->main = &main;
    }
}

void BackendManager::disassemble() {
    disassemble_ctx(ctx, not ctx->config->contains(NO_COLORIZE_OUTPUT));
}

void BackendManager::run() {
    if (ctx->main != nullptr) {
        vm.set_function_module_info(ctx->main, ctx->compiled_modules.size());
    }

    std::size_t i = 0;
    for (auto &module : ctx->compiled_modules) {
        vm.set_function_module_info(&module, i++);
    }

    if (ctx->main != nullptr) {
        vm.run(main);
    }
}