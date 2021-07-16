/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "ASTPrinter.hpp"
#include "CodeGen/CodeGen.hpp"
#include "ErrorLogger/ErrorLogger.hpp"
#include "Parser/Parser.hpp"
#include "Parser/TypeResolver.hpp"
#include "Scanner/Scanner.hpp"
#include "VirtualMachine/Disassembler.hpp"
#include "VirtualMachine/VirtualMachine.hpp"

#include <algorithm>
#include <cxxopts.hpp>
#include <fstream>
#include <iostream>

void run_module(const char *const main_module, cxxopts::ParseResult &result) {
    std::string main_path{main_module};
    std::ifstream file(main_path, std::ios::in);
    std::string source{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};

    std::size_t path_index = main_path.find_last_of('/');
    std::string main_dir = main_path.substr(0, path_index);
    std::string main_name = main_path.substr(path_index + 1);

    logger.set_module_name(main_name);
    logger.set_source(source);
    Scanner scanner{source};

    Module main{main_name, main_dir + "/"};

    Parser parser{scanner.scan(), main, 0};
    TypeResolver resolver{main};
    main.statements = parser.program();
    resolver.check(main.statements);
    if (result.count("dump-ast")) {
        ASTPrinter{}.print_stmts(main.statements);
    }

    if (not result.count("check") && not logger.had_error) {
        std::sort(Parser::parsed_modules.begin(), Parser::parsed_modules.end(),
            [](const auto &x1, const auto &x2) { return x1.second > x2.second; });

        for (auto &module : Parser::parsed_modules) {
            std::cout << module.first.name << " -> depth: " << module.second << "\n";
        }

        Generator generator{};
        for (auto &module : Parser::parsed_modules) {
            Generator::compiled_modules.emplace_back(generator.compile(module.first));
        }
        RuntimeModule main_compiled = generator.compile(main);
        main_compiled.top_level_code.emit_instruction(Instruction::HALT, 0);

        if (result.count("disassemble-code")) {
            disassemble(main_compiled.top_level_code, main_name);
            std::cout << '\n';
            for (auto &[function_name, function] : main_compiled.functions) {
                disassemble(function.code, function_name);
            }
        }

        VirtualMachine vm{!!result.count("trace-exec-stack"), !!result.count("trace-exec-insn")};
        vm.run(main_compiled);
    }
}

int main(int argc, char *argv[]) {
    cxxopts::Options options{"wis", "A small and simple interpreted language"};

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