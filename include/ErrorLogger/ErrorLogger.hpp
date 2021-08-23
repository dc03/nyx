#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef ERROR_LOGGER_HPP
#define ERROR_LOGGER_HPP

#include "AST/Token.hpp"

#include <string>
#include <string_view>
#include <vector>

struct ErrorLogger {
    bool had_error{false};
    bool had_runtime_error{false};
    std::string_view source{};
    std::string_view module_name{};
    void set_module_name(std::string_view name);
    void set_source(std::string_view file_source);
};

extern ErrorLogger logger;

void warning(std::vector<std::string> message, const Token &where);
void error(std::vector<std::string> message, const Token &where);
void runtime_error(std::string_view message, std::size_t line_number);
void note(std::vector<std::string> message);
void compile_error(std::vector<std::string> message);

#endif