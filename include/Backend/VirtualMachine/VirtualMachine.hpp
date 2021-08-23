#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef VIRTUAL_MACHINE_HPP
#define VIRTUAL_MACHINE_HPP

#include "Module.hpp"
#include "Natives.hpp"
#include "Value.hpp"

#include <memory>
#include <unordered_map>

struct CallFrame {
    Value *stack{};
    Chunk *return_chunk{};
    Chunk::InstructionSizeType *return_ip{};
};

enum class ExecutionState { RUNNING = 0, FINISHED = 1 };

class VirtualMachine {
    constexpr static std::size_t stack_size = 32768;
    constexpr static std::size_t frame_size = 1024;

    Chunk::InstructionSizeType *ip{};

    std::unique_ptr<Value[]> stack{};
    std::size_t stack_top{};

    std::unique_ptr<CallFrame[]> frames{};
    std::size_t frame_top{};

    StringCacher cache{};
    std::unordered_map<std::string_view, NativeFn> natives{};

    Chunk *current_chunk{};
    RuntimeModule *current_module{};

    Chunk::InstructionSizeType read_next();

    bool trace_stack{false};
    bool trace_insn{false};

    void push(Value value) noexcept;
    void pop() noexcept;

    std::size_t get_current_line() const noexcept;
    Value::ListType *make_new_list();
    void destroy_list(Value::ListType *list);
    Value copy(Value &value);
    void copy_into(Value::ListType *list, Value::ListType *what);

  public:
    VirtualMachine(bool trace_stack, bool trace_insn);
    ~VirtualMachine() = default;

    VirtualMachine(const VirtualMachine &) = delete;
    VirtualMachine &operator=(const VirtualMachine &) = delete;

    void run(RuntimeModule &module);
    ExecutionState step();
    [[nodiscard]] const HashedString &store_string(std::string str);
};

#endif