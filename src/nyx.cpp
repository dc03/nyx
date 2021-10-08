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
        ASTPrinter{}.print_stmts(compile_manager.get_module().statements);
    }

    if (not result.count("check") && not logger.had_error) {
        RuntimeContext runtime_ctx{};
        RuntimeManager runtime_manager{&runtime_ctx};

        for (auto &module : compile_ctx.parsed_modules) {
            std::cout << module.first.name << " -> depth: " << module.second << "\n";
        }

        runtime_manager.compile(&compile_ctx);
//        Generator generator{};
//        for (auto &module : compile_ctx.parsed_modules) {
//            Generator::compiled_modules.emplace_back(generator.compile(module.first));
//        }
//        RuntimeModule main_compiled = generator.compile(compile_manager.get_module());
//        main_compiled.top_level_code.emit_instruction(Instruction::HALT, 0);

        if (result.count("disassemble-code")) {
//            disassemble_chunk(main_compiled.top_level_code, compile_manager.get_module().name);
//            std::cout << '\n';
//            for (auto &[function_name, function] : main_compiled.functions) {
//                disassemble_chunk(function.code, function_name);
//            }
            runtime_manager.disassemble();
        }

//        VirtualMachine vm{!!result.count("trace-exec-stack"), !!result.count("trace-exec-insn")};
//        vm.run(main_compiled);
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