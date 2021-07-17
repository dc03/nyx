/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "ErrorLogger.hpp"

#include "../Common.hpp"

#include <iostream>

ErrorLogger logger{};

void ErrorLogger::set_module_name(std::string_view name) {
    this->module_name = name;
}

void ErrorLogger::set_source(std::string_view file_source) {
    this->source = file_source;
}

void print_message(const std::vector<std::string> &message, const Token &where, const std::string_view prefix) {
    std::cerr << "\n  | In module '" << logger.module_name << "',";
    std::cerr << "\n!-| line " << where.line << " | " << prefix << ": ";
    for (const std::string &str : message) {
        std::cerr << str;
    }
    std::cerr << '\n';
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
    std::cerr << "\n >| ";
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

void warning(std::vector<std::string> message, const Token &where) {
    print_message(message, where, "Warning");
}

void error(std::vector<std::string> message, const Token &where) {
    logger.had_error = true;
    print_message(message, where, "Error");
}

void runtime_error(const std::string_view message, std::size_t line_number) {
    logger.had_runtime_error = true;
    std::cerr << "\n!-| line " << line_number << " | Error: " << message << '\n';
    std::size_t line_count = 1;
    std::size_t i = 0;
    for (; line_count < line_number; i++) {
        if (logger.source[i] == '\n') {
            line_count++;
        }
    }
    std::cerr << " >| \n >| ";
    for (; logger.source[i] != '\n' && logger.source[i] != '\0'; i++) {
        std::cerr << logger.source[i];
    }
    std::cerr << '\n';
}

void note(std::vector<std::string> message) {
    std::cerr << "->| note: ";
    for (const std::string &str : message) {
        std::cerr << str;
    }
    std::cerr << '\n';
}

void compile_error(std::vector<std::string> message) {
    std::cerr << "\n  | In module '" << logger.module_name << "',";
    std::cerr << "\n!-| Compile error: ";
    for (const std::string &str : message) {
        std::cerr << str;
    }
    std::cerr << '\n';
}