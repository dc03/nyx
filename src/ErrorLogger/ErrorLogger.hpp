#pragma once

#ifndef ERROR_LOGGER_HPP
#  define ERROR_LOGGER_HPP

#include <string_view>

struct ErrorLogger {
    bool had_error{false};
    bool had_runtime_error{false};
};

extern ErrorLogger logger;

void error(const char *message, std::size_t line);
void runtime_error(const char *message, std::size_t line);

void error(const char *message, std::size_t line_number, std::string_view line);
void runtime_error(const char *message, std::size_t line_number, std::string_view line);

#endif