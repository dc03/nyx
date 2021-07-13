#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef STRING_CACHER_HPP
#define STRING_CACHER_HPP

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

struct HashedString {
    std::string str{};
    std::size_t hash{};

    HashedString() noexcept = default;
    explicit HashedString(std::string str) : str{std::move(str)}, hash{std::hash<std::string>{}(this->str)} {}

    [[nodiscard]] bool operator==(const HashedString &other) const noexcept {
        return hash == other.hash && str == other.str;
    }
    [[nodiscard]] bool operator>(const HashedString &other) const noexcept { return str > other.str; }
    [[nodiscard]] bool operator<(const HashedString &other) const noexcept { return str < other.str; }
    [[nodiscard]] bool operator==(const std::string &other) const noexcept { return str == other; }
};

namespace std {
template <>
struct hash<HashedString> {
    std::size_t operator()(const HashedString &string) const noexcept { return string.hash; }
};
} // namespace std

class StringCacher {
    std::unordered_map<HashedString, std::size_t> strings{};

  public:
    StringCacher() noexcept = default;

    [[nodiscard]] const HashedString &concat(const HashedString &first, const HashedString &second);
    [[nodiscard]] const HashedString &insert(std::string &&value);
    [[nodiscard]] const HashedString &insert(const std::string &value);
    [[nodiscard]] const HashedString &insert(HashedString &&value);
    [[nodiscard]] const HashedString &insert(const HashedString &value);
    void remove(const HashedString &value);
};

#endif