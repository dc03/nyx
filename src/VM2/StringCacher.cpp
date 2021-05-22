/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "StringCacher.hpp"

#include <algorithm>

const HashedString &StringCacher::concat(const HashedString &first, const HashedString &second) {
    HashPair pair = {first.hash, second.hash};
    if (auto it = table.find(pair); it != table.end()) {
        return *(it->second);
    } else {
        std::string value = first.str + second.str;
        if (auto it2 = std::find(strings.begin(), strings.end(), value); it2 != strings.end()) {
            table.emplace(pair, &(*it2));
            return *it2;
        } else {
            return *strings.emplace(std::move(value)).first;
        }
    }
}

const HashedString &StringCacher::insert(std::string value) {
    return *strings.emplace(std::move(value)).first;
}