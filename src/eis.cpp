/* See LICENSE at project root for license details */
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <variant>

#include "Parser/Parser.hpp"
#include "Scanner/Scanner.hpp"

int main(int argc, char *argv[]) {
    std::ifstream file(argv[1], std::ios::in);
    std::string source{std::istreambuf_iterator<char>{file},
                       std::istreambuf_iterator<char>{}};
    logger.set_source(source);
    Scanner scanner{source};
    using T = std::variant<int, double, std::string, bool, std::nullptr_t>;
    Parser<T> parser{scanner.scan()};
    while (parser.peek().type != TokenType::END_OF_FILE) {
        try {
            auto &&foo = parser.statement();
        } catch (...) {}
        //std::cout << foo->to_string() << '\n';
        //std::cout << dynamic_cast<ExpressionStmt<T>*>(foo.get())->expr->string_tag() << '\n';
    }
    return 0;
}