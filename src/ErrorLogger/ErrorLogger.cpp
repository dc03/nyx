/* See LICENSE at project root for license details */
#include <iostream>

#include "ErrorLogger.hpp"

ErrorLogger logger{};

void ErrorLogger::set_source(std::string_view file_source) {
    this->source = file_source;
}

void print_message(const std::string_view message, const Token &where, const std::string_view prefix) {
    std::cerr << "\n!-| line " << where.line << " | " << prefix << ": " << message << '\n';
    std::size_t line_start = where.start;
    std::size_t line_end = where.end;
    while (line_start > 0 && logger.source[line_start] != '\n') {
        line_start--;
    }
    while (line_end < logger.source.size() && logger.source[line_end] != '\n') {
        line_end++;
    }

    std::cerr << " >| ";
    for (std::size_t i{line_start}; i < line_end; i++) {
        std::cerr << logger.source[i];
        if (logger.source[i] == '\n') {
            std::cerr << " >| ";
        }
    }
    std::cout << "\n >| ";
    for (std::size_t i{line_start + 1}; i < line_end; i++) {
        if (i == where.start || line_start == where.start) {
            std::cerr << '^';
        } else if (where.start < i && i < where.end) {
            std::cerr << '-';
        } else {
            std::cerr << ' ';
        }
    }
    std::cerr << '\n';
}

void warning(std::string_view message, const Token &where) {
    print_message(message, where, "Warning");
}

void error(const std::string_view message, const Token &where) {
    logger.had_error = true;
    print_message(message, where, "Error");
}
void runtime_error(const std::string_view message, const Token &where) {
    logger.had_runtime_error = true;
    std::cerr << "\n!-| line " << where.line << " | Error: " << message << '\n';
    std::size_t line_start = where.start;
    std::size_t line_end = where.end;
    while (line_start > 0 && logger.source[line_start] != '\n') {
        line_start--;
    }
    while (line_end < logger.source.size() && logger.source[line_end] != '\n') {
        line_end++;
    }

    std::cerr << " >| ";
    for (std::size_t i{line_start}; i < line_end; i++) {
        std::cerr << logger.source[i];
        if (logger.source[i] == '\n') {
            std::cerr << " >| ";
        }
    }
    std::cout << "\n >| ";
    for (std::size_t i{line_start + 1}; i < line_end; i++) {
        if (i == where.start || line_start == where.start) {
            std::cerr << '^';
        } else if (where.start < i && i < where.end) {
            std::cerr << '-';
        } else {
            std::cerr << ' ';
        }
    }
    std::cerr << '\n';
}

void note(const std::string_view message) {
    std::cerr << "->| note: " << message << '\n';
}