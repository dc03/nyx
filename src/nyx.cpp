/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "AST/ASTPrinter.hpp"
#include "Backend/RuntimeManager.hpp"
#include "ErrorLogger/ErrorLogger.hpp"
#include "Frontend/CompileManager.hpp"

#include <cxxopts.hpp>
#include <iostream>

void run_module(const char *const main_module, cxxopts::ParseResult &result) {
    CompileContext compile_ctx{};
    CompileManager compile_manager{&compile_ctx, main_module, true, 0};

    compile_manager.parse_module();
    compile_manager.check_module();

    if (result.count("dump-ast")) {
        ASTPrinter printer{};
        for (auto &[module, depth] : compile_ctx.parsed_modules) {
            std::cout << "-<=== Module " << module.name << " ===>-\n\n";
            printer.print_stmts(module.statements);
        }
        std::cout << "-<=== Main Module ===>-\n";
        std::cout << "-<=== Module " << compile_manager.get_module().name << " ===>-\n\n";
        printer.print_stmts(compile_manager.get_module().statements);
    }

    if (not result.count("check") && not logger.had_error) {
        RuntimeContext runtime_ctx{};
        RuntimeManager runtime_manager{&runtime_ctx};

        for (auto &module : compile_ctx.parsed_modules) {
            std::cout << module.first.name << " -> depth: " << module.second << "\n";
        }

        runtime_manager.compile(&compile_ctx);
        if (result.count("disassemble-code")) {
            runtime_manager.disassemble();
        }
        runtime_manager.run();
    }
}

int main(int argc, char *argv[]) {
    cxxopts::Options options{"nyx", "A small and simple interpreted language"};

    // clang-format off
    options.add_options()
        ("check", "Do not run the code, only parse and type check it")
        ("dump-ast", "Dump the contents of the AST after parsing and typechecking", cxxopts::value<bool>()->default_value("false"))
        ("disassemble-code", "Disassemble the byte code produced for the VM", cxxopts::value<bool>()->default_value("false"))
        ("main", "The module from which to start execution", cxxopts::value<std::string>())
        ("trace-exec-stack", "Print the contents of the stack as the VM executes code", cxxopts::value<bool>()->default_value("false"))
        ("trace-exec-insn", "Print the instructions as they are executed by the VM", cxxopts::value<bool>()->default_value("false"))
        ("h,help", "Print usage");
    // clang-format on

    try {
        cxxopts::ParseResult result = options.parse(argc, argv);
        if (result.arguments().empty() || result.count("help")) {
            std::cout << options.help() << '\n';
            return 0;
        } else if (result.count("main")) {
            run_module(result["main"].as<std::string>().c_str(), result);
        }
    } catch (cxxopts::OptionException &ex) { std::cout << ex.what() << '\n'; }
    return 0;
}