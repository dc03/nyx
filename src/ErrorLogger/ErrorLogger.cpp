/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "ErrorLogger/ErrorLogger.hpp"

#include "Common.hpp"
#include "Frontend/Module.hpp"

#include <iostream>

// Disable min/max macros on windows
#if defined(_WIN32) || defined(_WIN64)
#define NOMINMAX
#endif

#include <termcolor/termcolor.hpp>

ColoredPrintHelper ErrorLogger::pcife(ColoredPrintHelper::StreamColorModifier colorizer) {
    return ColoredPrintHelper{colors_enabled, colorizer};
}

void ErrorLogger::print_message(Module *module, const std::vector<std::string> &message, const Token &where,
    std::string_view prefix, ColoredPrintHelper::StreamColorModifier color) {
    std::cerr << pcife(termcolor::reset) << "\n!-| ";
    std::cerr << pcife(termcolor::blue) << pcife(termcolor::bold) << module->full_path.c_str() << ":" << where.line
              << pcife(termcolor::reset) << ":";
    std::cerr << "\n  | " << pcife(termcolor::bold) << pcife(color) << prefix << pcife(termcolor::reset) << pcife(color)
              << ": ";
    for (const std::string &str : message) {
        std::cerr << str;
    }
    std::cerr << pcife(termcolor::reset) << '\n';

    std::size_t line_start = where.start;
    std::size_t line_end = where.end;
    while (line_start > 0 && module->source[line_start] != '\n') {
        line_start--;
    }
    while (line_end < module->source.size() && module->source[line_end] != '\n') {
        line_end++;
    }

    std::cerr << " >| ";
    for (std::size_t i{line_start}; i < line_end; i++) {
        std::cerr << module->source[i];
        if (module->source[i] == '\n') {
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

void ErrorLogger::warning(Module *module, const std::vector<std::string> &message, const Token &where) {
    print_message(module, message, where, "Warning", termcolor::yellow);
}

void ErrorLogger::error(Module *module, const std::vector<std::string> &message, const Token &where) {
    error_occurred = true;
    print_message(module, message, where, "Error", termcolor::red);
}

void ErrorLogger::runtime_error(const std::string_view message, std::size_t line_number) {
    runtime_error_occurred = true;
    std::cerr << "\n!-| line " << line_number << " | " << pcife(termcolor::red) << "Error: " << message
              << pcife(termcolor::reset) << '\n';
    //    std::size_t line_count = 1;
    //    std::size_t i = 0;
    //    for (; line_count < line_number; i++) {
    //        if (module->source[i] == '\n') {
    //            line_count++;
    //        }
    //    }
    //    std::cerr << " >| \n >| ";
    //    for (; module->source[i] != '\n' && module->source[i] != '\0'; i++) {
    //        std::cerr << module->source[i];
    //    }
    std::cerr << '\n';
}

void ErrorLogger::note(Module *module, const std::vector<std::string> &message) {
    std::cerr << "->| " << pcife(termcolor::bold) << pcife(termcolor::green) << "note: " << pcife(termcolor::reset)
              << pcife(termcolor::green);
    for (const std::string &str : message) {
        std::cerr << str;
    }
    std::cerr << pcife(termcolor::reset) << '\n';
}

void ErrorLogger::fatal_error(std::vector<std::string> message) {
    std::cerr << "\n!-| " << pcife(termcolor::red) << pcife(termcolor::bold)
              << "Compile error: " << pcife(termcolor::reset) << pcife(termcolor::red);
    for (const std::string &str : message) {
        std::cerr << str;
    }
    std::cerr << pcife(termcolor::reset) << '\n';
}

bool ErrorLogger::had_error() const noexcept {
    return error_occurred;
}

bool ErrorLogger::had_runtime_error() const noexcept {
    return runtime_error_occurred;
}

void ErrorLogger::set_color(bool value) noexcept {
    colors_enabled = value;
}