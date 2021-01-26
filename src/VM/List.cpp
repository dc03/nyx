/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "List.hpp"

#include "../Common.hpp"

List::List(std::vector<int> value) : as{std::move(value)} {}
List::List(std::vector<double> value) : as{std::move(value)} {}
List::List(std::vector<std::string> value) : as{std::move(value)} {}
List::List(std::vector<char> value) : as{std::move(value)} {}
List::List(std::vector<Value *> value) : as{std::move(value)} {}
List::List(std::vector<std::unique_ptr<List>> value) : as{std::move(value)} {}

bool List::operator==(const List &other) const noexcept {
    return as == other.as;
}

std::size_t List::size() noexcept {
    if (is_int_list()) {
        return to_int_list().size();
    } else if (is_float_list()) {
        return to_float_list().size();
    } else if (is_string_list()) {
        return to_string_list().size();
    } else if (is_bool_list()) {
        return to_bool_list().size();
    } else if (is_ref_list()) {
        return to_ref_list().size();
    } else if (is_list_list()) {
        return to_list_list().size();
    }
    unreachable();
}

void List::resize(std::size_t size) noexcept {
    if (is_int_list()) {
        to_int_list().resize(size);
    } else if (is_float_list()) {
        to_float_list().resize(size);
    } else if (is_string_list()) {
        to_string_list().resize(size);
    } else if (is_bool_list()) {
        to_bool_list().resize(size);
    } else if (is_ref_list()) {
        to_ref_list().resize(size);
    } else if (is_list_list()) {
        to_list_list().resize(size);
    }
}