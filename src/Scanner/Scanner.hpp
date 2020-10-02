#pragma once

/* See LICENSE at project root for license details */

#ifndef SCANNER_HPP
#  define SCANNER_HPP

#include <array>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include "../Token.hpp"
#include "../TokenTypes.hpp"
#include "Trie.hpp"

class Scanner {
    std::size_t line{1};
    std::size_t start{};
    std::size_t current{};
    std::vector<Token> tokens{};
    std::string_view source{};
    Trie keywords{};

  public:
    Scanner();
    Scanner(const std::string_view source);

    bool is_at_end() const noexcept;

    char advance();
    char peek() const noexcept;
    char peek_next() const noexcept;
    bool match(const char ch);

    void number();
    void identifier();
    void string(const char delim);
    void multiline_comment();

    void add_token(const TokenType type);
    void scan_token();
    const std::vector<Token> &scan();
};

#endif