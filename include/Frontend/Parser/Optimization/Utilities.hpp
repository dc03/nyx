#pragma once

/* Copyright (C) 2020-2022  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef UTILITIES_HPP
#define UTILITIES_HPP

#include <utility>

template <typename T, typename... Args>
T first_not_null(T &&value, Args &&...args) {
    if (value) {
        return std::forward<T>(value);
    } else {
        return first_not_null(std::forward<Args>(args)...);
    }
}

template <typename T>
T first_not_null(T &&value) {
    if (value) {
        return std::forward<T>(value);
    } else {
        return nullptr;
    }
}

#endif