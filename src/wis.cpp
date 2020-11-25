/* Copyright (C) 2020  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "CodeGen/CodeGen.hpp"
#include "ErrorLogger/ErrorLogger.hpp"
#include "Parser/Parser.hpp"
#include "Parser/TypeResolver.hpp"
#include "Scanner/Scanner.hpp"
#include "VM/VM.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>

int main(const int, const char *const argv[]) {
    std::string main_path{argv[1]};
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

    //    if (!logger.had_error) {
    //        std::sort(Parser::parsed_modules.begin(), Parser::parsed_modules.end(),
    //            [](const auto &x1, const auto &x2) { return x1.second > x2.second; });
    //
    //        for (auto &module : Parser::parsed_modules) {
    //            std::cout << module.first.name << " -> depth: " << module.second << "\n";
    //        }
    //
    //        Generator generator{};
    //        for (auto &module : Parser::parsed_modules) {
    //            Generator::compiled_modules.emplace_back(generator.compile(module.first));
    //        }
    //        RuntimeModule main_compiled = generator.compile(main);
    //        main_compiled.top_level_code.emit_instruction(Instruction::HALT);
    //        VM vm{};
    //        vm.run(main_compiled);
    //        // std::cout << vm.stack[0].to_int();
    //    }
    return 0;
}