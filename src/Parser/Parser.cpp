#include <variant>

#include "Parser.hpp"
#include "../Scanner/Scanner.hpp"

int main() {
    Scanner scanner{"(5 + 6 - 4 * (3 + 4))"};
    auto &&foo = scanner.scan();
    Parser<std::variant<double, int>> parser{foo};
    auto &&a = parser.expression();
}