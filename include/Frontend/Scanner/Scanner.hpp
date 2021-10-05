#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef SCANNER_HPP
#define SCANNER_HPP

#include "AST/Token.hpp"
#include "AST/TokenTypes.hpp"
#include "Trie.hpp"

#include <string_view>
#include <unordered_map>
#include <vector>

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

class ScannerV2 {
    std::size_t line{1};
    std::size_t current_token_start{};
    std::size_t current_token_end{};

    std::size_t paren_depth{};
    std::vector<Token> tokens{};
    std::string_view source{};

    std::unordered_map<std::string, TokenType> keyword_map{};
    Trie keywords{};

    [[nodiscard]] bool is_at_end() const noexcept;

    [[nodiscard]] char advance();
    [[nodiscard]] char peek() const noexcept;
    [[nodiscard]] char peek_next() const noexcept;
    [[nodiscard]] const Token &previous() const noexcept;
    bool match(char ch);

    void scan_number();
    void scan_identifier_or_keyword();
    void scan_string();
    void skip_multiline_comment();

  public:
    ScannerV2();

    Token scan_token();
    std::vector<Token> &scan_all();
};

#endif