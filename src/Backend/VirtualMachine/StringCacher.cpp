/* Copyright (C) 2020-2022  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "nyx/Backend/VirtualMachine/StringCacher.hpp"

#include <algorithm>

const HashedString &StringCacher::concat(const HashedString &first, const HashedString &second) {
    return insert(first.str + second.str);
}

const HashedString &StringCacher::insert(std::string &&value) {
    return insert(HashedString{std::move(value)});
}

const HashedString &StringCacher::insert(const std::string &value) {
    return insert(HashedString{value});
}

const HashedString &StringCacher::insert(HashedString &&value) {
    if (auto it = strings.find(value); it != strings.end()) {
        it->second++;
        return it->first;
    } else {
        return strings.emplace(std::move(value), 1).first->first;
    }
}

const HashedString &StringCacher::insert(const HashedString &value) {
    if (auto it = strings.find(value); it != strings.end()) {
        it->second++;
        return it->first;
    } else {
        return strings.emplace(value, 1).first->first;
    }
}

void StringCacher::remove(const HashedString &value) {
    if (auto it = strings.find(value); it != strings.end()) {
        it->second--;
        if (it->second <= 0) {
            strings.erase(it);
        }
    }
}