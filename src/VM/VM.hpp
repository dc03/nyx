#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef VM_HPP
#define VM_HPP

#include "../Module.hpp"
#include "Chunk.hpp"
#include "Natives.hpp"

constexpr std::size_t max_stack_frames = 1000;
constexpr std::size_t max_locals_per_frame = 100;

struct CallFrame {
    Value *stack_slots{};
    Chunk *return_chunk{};
    Chunk::byte *return_ip{};
};

struct VM {
    Chunk::byte *ip{};
    Value *stack{};
    Value *stack_top{};
    Chunk *chunk{};
    CallFrame *frames{};
    CallFrame *frame_top{};
    std::unordered_map<std::string_view, NativeFn> natives{};
    RuntimeModule *current_module{};

    void run(RuntimeModule &main_module);
    void push(const Value &value);
    void push(Value &&value);
    void pop();
    void define_native(const std::string &name, NativeFn code);
    [[nodiscard]] Value &top_from(std::size_t distance) const;
    [[nodiscard]] Value move_top_from(std::size_t distance);
    void recursively_size_list(List &list, Value *size, std::size_t depth);
    static List make_list(List::tag type);
    std::size_t current_line() const noexcept;

    Chunk::byte read_byte();
    std::size_t read_three_bytes();

    VM();
    ~VM();
};

#endif