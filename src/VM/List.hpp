#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef LIST_HPP
#define LIST_HPP

#include <string>
#include <variant>
#include <vector>

struct Value;

struct List {
    enum tag { INT_LIST, FLOAT_LIST, STRING_LIST, BOOL_LIST, REF_LIST };
    std::variant<std::vector<int>, std::vector<double>, std::vector<std::string>, std::vector<char>,
        std::vector<Value *>>
        as;

    explicit List(std::vector<int> value);
    explicit List(std::vector<double> value);
    explicit List(std::vector<std::string> value);
    explicit List(std::vector<char> value);
    explicit List(std::vector<Value *> value);

    // clang-format off
    [[nodiscard]] bool is_int_list()    const noexcept { return as.index() == List::tag::INT_LIST; }
    [[nodiscard]] bool is_float_list()  const noexcept { return as.index() == List::tag::FLOAT_LIST; }
    [[nodiscard]] bool is_string_list() const noexcept { return as.index() == List::tag::STRING_LIST; }
    [[nodiscard]] bool is_bool_list()   const noexcept { return as.index() == List::tag::BOOL_LIST; }
    [[nodiscard]] bool is_ref_list()    const noexcept { return as.index() == List::tag::REF_LIST; }

    [[nodiscard]] std::vector<int> &to_int_list()            noexcept { return std::get<INT_LIST>(as); }
    [[nodiscard]] std::vector<double> &to_float_list()       noexcept { return std::get<FLOAT_LIST>(as); }
    [[nodiscard]] std::vector<std::string> &to_string_list() noexcept { return std::get<STRING_LIST>(as); }
    [[nodiscard]] std::vector<char> &to_bool_list()          noexcept { return std::get<BOOL_LIST>(as); }
    [[nodiscard]] std::vector<Value *> &to_ref_list()        noexcept { return std::get<REF_LIST>(as); }
    // clang-format on

    [[nodiscard]] bool operator==(const List &other) const noexcept;
};

#endif