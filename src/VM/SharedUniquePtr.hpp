#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef SHARED_UNIQUE_PTR_HPP
#define SHARED_UNIQUE_PTR_HPP

#include <memory>
#include <utility>

template <typename T>
class SharedUniquePtr {
    T *object{nullptr};
    bool is_owner{false};

    explicit SharedUniquePtr(T *object) : object{object}, is_owner{true} {}

  public:
    SharedUniquePtr(const SharedUniquePtr &other) : object{other.object}, is_owner{false} {}
    explicit SharedUniquePtr(const std::unique_ptr<T> &other) : object{other.get()}, is_owner{false} {}
    SharedUniquePtr &operator=(const SharedUniquePtr &other) {
        // TODO: Fix for self assignment
        if (object && is_owner) {
            delete object;
        }
        object = other.object;
        is_owner = false;
        return *this;
    }

    SharedUniquePtr(SharedUniquePtr &&other) noexcept { *this = std::move(other); }
    SharedUniquePtr &operator=(SharedUniquePtr &&other) noexcept {
        using std::swap;
        swap(object, other.object);
        swap(is_owner, other.is_owner);
        return *this;
    }

    ~SharedUniquePtr() {
        if (is_owner) {
            delete object;
        }
    }

    T *get() const noexcept { return object; }
    T &operator*() const noexcept { return *object; }
    T *operator->() const noexcept { return object; }

    SharedUniquePtr copy() { return SharedUniquePtr{new T{*object}}; }

    template <typename... Args>
    static SharedUniquePtr create(Args &&...args) {
        return SharedUniquePtr{new T{std::forward<Args>(args)...}};
    }
};

#endif