/* See LICENSE at project root for license details */
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include "ErrorLogger/ErrorLogger.hpp"
#include "Parser/Parser.hpp"
#include "Parser/TypeResolver.hpp"
#include "Scanner/Scanner.hpp"

int main(int, char *argv[]) {
    std::ifstream file(argv[1], std::ios::in);
    std::string source{std::istreambuf_iterator<char>{file},
                       std::istreambuf_iterator<char>{}};
    logger.set_source(source);
    Scanner scanner{source};
    std::vector<ClassStmt*> classes{};
    std::vector<FunctionStmt*> functions{};
    std::vector<VarStmt*> globals{};
    Parser parser{scanner.scan(), classes, functions, globals};
    auto &&foo = parser.program();
    TypeResolver resolver{classes, functions, globals};
    return 0;
}