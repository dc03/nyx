#pragma once

#ifndef TRIE_HPP
#  define TRIE_HPP

#include <algorithm>
#include <cassert>
#include <cctype>
#include <memory>
#include <string_view>

#include "../TokenTypes.hpp"

static inline constexpr std::size_t num_alphabets{52};

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

    Node(char value): Node() {
        this->value = value;
    }
};

class Trie {
    Node::ptr_t head{nullptr};

    [[nodiscard]] constexpr std::size_t get_index(char ch) const noexcept;

  public:
    void insert(const std::string_view key, TokenType type);
    [[nodiscard]] TokenType search(const std::string_view key) const noexcept;
};

#endif