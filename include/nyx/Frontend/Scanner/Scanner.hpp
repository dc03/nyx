#pragma once

/* Copyright (C) 2020-2022  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef SCANNER_HPP
#define SCANNER_HPP

#include "Trie.hpp"
#include "nyx/AST/Token.hpp"
#include "nyx/AST/TokenTypes.hpp"
#include "nyx/Frontend/FrontendContext.hpp"

#include <string_view>
#include <unordered_map>
#include <vector>

class Scanner {
    std::size_t line{1};
    std::size_t current_token_start{};
    std::size_t current_token_end{};

    std::size_t paren_depth{};
    Token current_token{};
    Token next_token{};
    std::string_view source{};

    struct KeywordTypePair {
        const char *lexeme;
        TokenType type;
    };

    constexpr static std::array keywords{
        // clang-format off
        KeywordTypePair{"and", TokenType::AND},
        KeywordTypePair{"bool", TokenType::BOOL},
        KeywordTypePair{"break", TokenType::BREAK},
        KeywordTypePair{"class", TokenType::CLASS},
        KeywordTypePair{"const", TokenType::CONST},
        KeywordTypePair{"continue", TokenType::CONTINUE},
        KeywordTypePair{"default", TokenType::DEFAULT},
        KeywordTypePair{"else", TokenType::ELSE},
        KeywordTypePair{"false", TokenType::FALSE},
        KeywordTypePair{"float", TokenType::FLOAT},
        KeywordTypePair{"fn", TokenType::FN},
        KeywordTypePair{"for", TokenType::FOR},
        KeywordTypePair{"if", TokenType::IF},
        KeywordTypePair{"import", TokenType::IMPORT},
        KeywordTypePair{"int", TokenType::INT},
        KeywordTypePair{"move", TokenType::MOVE},
        KeywordTypePair{"null", TokenType::NULL_},
        KeywordTypePair{"not", TokenType::NOT},
        KeywordTypePair{"or", TokenType::OR},
        KeywordTypePair{"protected", TokenType::PROTECTED},
        KeywordTypePair{"private", TokenType::PRIVATE},
        KeywordTypePair{"public", TokenType::PUBLIC},
        KeywordTypePair{"ref", TokenType::REF},
        KeywordTypePair{"return", TokenType::RETURN},
        KeywordTypePair{"string", TokenType::STRING},
        KeywordTypePair{"super", TokenType::SUPER},
        KeywordTypePair{"switch", TokenType::SWITCH},
        KeywordTypePair{"this", TokenType::THIS},
        KeywordTypePair{"true", TokenType::TRUE},
        KeywordTypePair{"type", TokenType::TYPE},
        KeywordTypePair{"typeof", TokenType::TYPEOF},
        KeywordTypePair{"var", TokenType::VAR},
        KeywordTypePair{"while", TokenType::WHILE}
        // clang-format on
    };

    Trie keyword_map{};

    FrontendContext *ctx{};
    Module *module{};

    char advance();
    [[nodiscard]] char peek() const noexcept;
    [[nodiscard]] char peek_next() const noexcept;
    bool match(char ch);

    [[nodiscard]] Token make_token(TokenType type, std::string_view lexeme) const;

    Token scan_number();
    Token scan_identifier_or_keyword();
    Token scan_string();

    Token singleline_comment();
    Token multiline_comment();

    bool is_valid_eol(const Token &token);

    std::string_view current_token_lexeme();

    Token scan_next();

  public:
    Scanner();
    Scanner(FrontendContext *ctx_, Module *module_, std::string_view source_);
    void reset();

    [[nodiscard]] bool is_at_end() const noexcept;

    Token scan_token();
    const Token &peek_token();
    std::vector<Token> scan_all();
};

#endif