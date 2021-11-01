#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef ERROR_LOGGER_HPP
#define ERROR_LOGGER_HPP

#include "AST/Token.hpp"

#include <string>
#include <string_view>
#include <vector>

class Module;

class ErrorLogger {
    using TermColorModifier = std::ostream &(*)(std::ostream &out);

    bool error_occurred{false};
    bool runtime_error_occurred{false};

    void print_message(Module *module, const std::vector<std::string> &message, const Token &where,
        std::string_view prefix, TermColorModifier color);

  public:
    void warning(Module *module, const std::vector<std::string> &message, const Token &where);
    void error(Module *module, const std::vector<std::string> &message, const Token &where);
    void runtime_error(std::string_view message, std::size_t line_number);
    void note(Module *module, const std::vector<std::string> &message);
    void fatal_error(std::vector<std::string> message);

    [[nodiscard]] bool had_error() const noexcept;
    [[nodiscard]] bool had_runtime_error() const noexcept;
};

#endif