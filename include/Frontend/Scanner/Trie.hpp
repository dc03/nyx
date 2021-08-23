#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef TRIE_HPP
#define TRIE_HPP

#include "AST/TokenTypes.hpp"

#include <memory>
#include <string_view>

static inline constexpr std::size_t num_alphabets{52 + 1 + 10};

struct Node {
    using ptr_t = std::unique_ptr<Node>;
    static constexpr auto &&make_unique = std::make_unique<Node>;

    char value{};
    bool is_last{false};
    TokenType type{TokenType::NONE};
    ptr_t nodes[num_alphabets]{};

    Node() {
        for (std::size_t i{0}; i < num_alphabets; i++) {
            nodes[i] = nullptr;
        }
    }

    Node(char value) : Node() { this->value = value; }
};

class Trie {
    Node::ptr_t head{nullptr};

    [[nodiscard]] static std::size_t get_index(char ch) noexcept;

  public:
    void insert(std::string_view key, TokenType type);
    [[nodiscard]] TokenType search(std::string_view key) const noexcept;
};

#endif