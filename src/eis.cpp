/* See LICENSE at project root for license details */
#include "ErrorLogger/ErrorLogger.hpp"
#include "Module.hpp"
#include "Parser/Parser.hpp"
#include "Parser/TypeResolver.hpp"
#include "Scanner/Scanner.hpp"

#include <fstream>
#include <iostream>
#include <iterator>

int main(const int, const char *const argv[]) {
    std::string main_path{argv[1]};
    std::ifstream file(main_path, std::ios::in);
    std::string source{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};

    logger.set_module_name("__main__");
    logger.set_source(source);
    Scanner scanner{source};

    std::size_t path_index = main_path.find_last_of('/');
    std::string main_dir = main_path.substr(0, path_index);
    Module main{"__main__", main_dir + "/"};

    std::vector<ClassStmt *> classes{};
    std::vector<FunctionStmt *> functions{};
    Parser parser{scanner.scan(), main};
    auto &&foo = parser.program();

    if (!logger.had_error) {
        TypeResolver resolver{main};
        resolver.check(foo);
    }
    return 0;
}