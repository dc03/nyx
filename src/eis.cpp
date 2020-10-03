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
    Scanner scanner{source};
    using T = std::variant<int, double, std::string, bool, std::nullptr_t>;
    Parser<T> parser{scanner.scan()};
    //try {
        parser.expression();
        parser.expression();
        auto &&foo = parser.expression();
        if (foo->to_string() == "CallExpr") {
            auto &&bar = dynamic_cast<CallExpr<T>*>(foo.get());
            std::cout << bar->function->to_string();
        }
    //} catch (ParseException &err) {
     //   std::cerr << err.what();
   // }
    return 0;
}