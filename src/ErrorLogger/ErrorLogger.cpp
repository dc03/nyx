#include <iostream>

#include "ErrorLogger.hpp"

ErrorLogger logger{};

void error(const char *message, std::size_t line) {
    logger.had_error = true;
    std::cerr << "!-| line " << line << " | Error: " << message << '\n';
}
void runtime_error(const char *message, std::size_t line) {
    logger.had_runtime_error = true;
    std::cerr << "!-| line " << line << " | Error: " << message << '\n';
}

void error(const char *message, std::size_t line_number, std::string_view line) {
    logger.had_error = true;
    std::cerr << "!-| line " << line_number << " | Error: " << message << '\n';
    std::cerr << "  | " << line << '\n';
}

void runtime_error(const char *message, std::size_t line_number, std::string_view line) {
    logger.had_runtime_error = true;
    std::cerr << "!-| line " << line_number << " | Error: " << message << '\n';
    std::cerr << "  | " << line << '\n';
}