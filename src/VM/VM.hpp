#pragma once

/* Copyright (C) 2020  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef VM_HPP
#define VM_HPP

#include "../Module.hpp"
#include "Chunk.hpp"

constexpr std::size_t max_stack_frames = 1000;
constexpr std::size_t max_locals_per_frame = 100;

struct CallFrame {
    Value *stack_slots{};
    Chunk::byte *return_ip{};
};

struct VM {
    struct NativeFn {
        Value (*code)(Value *);
        std::size_t arity;
    };

    Chunk::byte *ip{};
    Value *stack{};
    Value *stack_top{};
    Chunk *chunk{};
    CallFrame *frames{};
    CallFrame *frame_top{};
    std::unordered_map<std::string, NativeFn> native_functions{};
    RuntimeModule *current_module{};

    void run(RuntimeModule &main_module);
    void push(const Value &value);
    void pop();
    void define_native(const std::string &name, NativeFn code);
    [[nodiscard]] Value &top_from(std::size_t distance) const;

    Chunk::byte read_byte();
    std::size_t read_three_bytes();

    VM() {
        frames = new CallFrame[max_stack_frames];
        frame_top = frames;
        stack = new Value[max_stack_frames * max_locals_per_frame];
        stack_top = stack;
    }

    ~VM() {
        delete[] frames;
        delete[] stack;
    }
};

#endif