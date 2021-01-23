/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "List.hpp"

List::List(std::vector<int> value) : as{std::move(value)} {}
List::List(std::vector<double> value) : as{std::move(value)} {}
List::List(std::vector<std::string> value) : as{std::move(value)} {}
List::List(std::vector<char> value) : as{std::move(value)} {}
List::List(std::vector<Value *> value) : as{std::move(value)} {}

bool List::operator==(const List &other) const noexcept {
    return as == other.as;
}