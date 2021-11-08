/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "AST/ASTPrinter.hpp"
#include "Backend/RuntimeManager.hpp"
#include "CLIConfigParser.hpp"
#include "ErrorLogger/ErrorLogger.hpp"
#include "Frontend/CompileManager.hpp"

#include <cxxopts.hpp>
#include <iostream>

void run(const char *const main_module, const CLIConfig *compile_config, const CLIConfig *runtime_config) {
    CompileContext compile_ctx{};
    compile_ctx.set_config(compile_config);

    CompileManager compile_manager{&compile_ctx, main_module, true, 0};

    compile_manager.parse_module();
    compile_manager.check_module();

    if (compile_config->contains("dump-ast")) {
        ASTPrinter printer{};
        for (auto &[module, depth] : compile_ctx.parsed_modules) {
            std::cout << "-<=== Module " << module.name << " ===>-\n\n";
            printer.print_stmts(module.statements);
        }
        std::cout << "-<=== Main Module ===>-\n";
        std::cout << "-<=== Module " << compile_manager.get_module().name << " ===>-\n\n";
        printer.print_stmts(compile_manager.get_module().statements);
    }

    if (not compile_config->contains("check") && not compile_ctx.logger.had_error()) {
        RuntimeContext runtime_ctx{};
        runtime_ctx.set_config(runtime_config);

        RuntimeManager runtime_manager{&runtime_ctx};

        for (auto &module : compile_ctx.parsed_modules) {
            std::cout << module.first.name << " -> depth: " << module.second << "\n";
        }

        runtime_manager.compile(&compile_ctx);
        if (runtime_config->contains("disassemble-code")) {
            runtime_manager.disassemble();
        }
        runtime_manager.run();
    }
}

int main(int argc, char *argv[]) {
    try {
        CLIConfigParser parser{argc, argv};

        const CLIConfig *compile_config = parser.get_compile_config();
        const CLIConfig *runtime_config = parser.get_runtime_config();

        if (parser.is_empty() || parser.is_help()) {
            std::cout << parser.get_help() << '\n';
            return 0;
        } else if (compile_config->contains("main")) {
            run(compile_config->get<std::string>("main").c_str(), compile_config, runtime_config);
        }
    } catch (const std::invalid_argument &e) { std::cout << e.what() << '\n'; }

    return 0;
}