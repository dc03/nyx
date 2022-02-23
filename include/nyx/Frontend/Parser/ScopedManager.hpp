#pragma once

/* Copyright (C) 2020-2022  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef SCOPED_MANAGER_HPP
#define SCOPED_MANAGER_HPP

#include <utility>

template <typename T>
class ScopedManager {
    T &managed;
    T previous_value{};

  public:
    ScopedManager(T &managed_, T &&new_value) : managed{managed_}, previous_value{std::exchange(managed_, new_value)} {}
    ~ScopedManager() { managed = previous_value; }
};

#endif