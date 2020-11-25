#pragma once

/* Copyright (C) 2020  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef VM_HPP
#define VM_HPP

#include "../Module.hpp"
#include "Chunk.hpp"

constexpr std::size_t stack_size = 1000;

struct VM {
    Value stack[stack_size];
    Value *stack_top{};
    Chunk::byte *ip{};
    Chunk *chunk{};
    void run(RuntimeModule &main_module);
    void push(const Value &value);
    void pop();
    [[nodiscard]] Value &top_from(std::size_t distance) const;

    Chunk::byte read_byte();

    VM() { stack_top = stack; }
};

#endif