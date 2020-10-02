/* See LICENSE at project root for license details */
#include <iostream>

#include "ErrorLogger.hpp"

ErrorLogger logger{};

void error(const std::string_view message, std::size_t line) {
    logger.had_error = true;
    std::cerr << "!-| line " << line << " | Error: " << message << '\n';
}
void runtime_error(const std::string_view message, std::size_t line) {
    logger.had_runtime_error = true;
    std::cerr << "!-| line " << line << " | Error: " << message << '\n';
}

void error(const std::string_view message, std::size_t line_number, std::string_view line) {
    logger.had_error = true;
    std::cerr << "!-| line " << line_number << " | Error: " << message << "\n  |\n";
    std::cerr << " >| ";
    for (char ch : line) {
        std::cerr << ch;
        if (ch == '\n') {
            std::cerr << " >| ";
        }
    }
    std::cerr << '\n';
}

void runtime_error(const std::string_view message, std::size_t line_number, std::string_view line) {
    logger.had_runtime_error = true;
    std::cerr << "!-| line " << line_number << " | Error: " << message << "\n  |\n";
    std::cerr << " >| ";
    for (char ch : line) {
        std::cerr << ch;
        if (ch == '\n') {
            std::cerr << " >| ";
        }
    }
    std::cerr << '\n';
}