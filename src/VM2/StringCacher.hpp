#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef VM2_STRING_CACHER_HPP
#define VM2_STRING_CACHER_HPP

#include "HashTable.hpp"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

struct HashedString {
    std::string str{};
    std::size_t hash{};

    HashedString() noexcept = default;
    explicit HashedString(std::string str) : str{std::move(str)}, hash{std::hash<std::string>{}(this->str)} {}

    [[nodiscard]] bool operator==(const HashedString &other) const noexcept { return str == other.str; }
    [[nodiscard]] bool operator>(const HashedString &other) const noexcept { return str > other.str; }
    [[nodiscard]] bool operator<(const HashedString &other) const noexcept { return str < other.str; }
    [[nodiscard]] bool operator==(const std::string &other) const noexcept { return str == other; }
};

using HashPair = std::pair<std::size_t, std::size_t>;

// From the Boost libraries
template <typename T>
constexpr static inline void hash_combine(std::size_t &seed, const T &v) {
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

namespace std {
template <>
struct hash<HashPair> {
    std::size_t operator()(const HashPair &pair) const noexcept {
        std::size_t seed = 0;
        hash_combine(seed, pair.first);
        hash_combine(seed, pair.second);
        return seed;
    }
};

template <>
struct hash<HashedString> {
    std::size_t operator()(const HashedString &string) const noexcept { return string.hash; }
};
} // namespace std

class StringCacher {
    std::unordered_set<HashedString> strings;
    std::unordered_map<HashPair, const HashedString *> table{};

  public:
    StringCacher() noexcept = default;

    [[nodiscard]] const HashedString &concat(const HashedString &first, const HashedString &second);
    [[nodiscard]] const HashedString &insert(std::string value);
};

#endif