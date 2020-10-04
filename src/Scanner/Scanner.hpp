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
    std::size_t paren_count{};
    std::vector<Token> tokens{};
    std::string_view source{};
    Trie keywords{};

  public:
    Scanner();
    explicit Scanner(std::string_view source);

    [[nodiscard]] bool is_at_end() const noexcept;

    char advance();
    [[nodiscard]] char peek() const noexcept;
    [[nodiscard]] char peek_next() const noexcept;
    [[nodiscard]] const Token &previous() const noexcept;
    bool match(char ch);

    void number();
    void identifier();
    void string(char delimiter);
    void multiline_comment();

    void add_token(TokenType type);
    void scan_token();
    const std::vector<Token> &scan();
};

#endif