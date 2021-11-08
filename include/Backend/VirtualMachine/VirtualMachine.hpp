#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef VIRTUAL_MACHINE_HPP
#define VIRTUAL_MACHINE_HPP

#include "Backend/RuntimeContext.hpp"
#include "Backend/RuntimeModule.hpp"
#include "ColoredPrintHelper.hpp"
#include "Natives.hpp"
#include "Value.hpp"

#include <memory>
#include <unordered_map>

struct CallFrame {
    Value *stack{};
    Chunk *return_chunk{};
    Chunk::InstructionSizeType *return_ip{};
    RuntimeModule *module{};
    std::size_t module_index{};
    std::string name{};
};

struct ModuleFrame {
    Value *stack{};
    std::string name{};
};

enum class ExecutionState { RUNNING = 0, FINISHED = 1 };

class VirtualMachine {
    constexpr static std::size_t stack_size = 32768;
    constexpr static std::size_t frame_size = 1024;
    constexpr static std::size_t module_size = 1024;

    Chunk::InstructionSizeType *ip{};

    std::unique_ptr<Value[]> stack{};
    std::size_t stack_top{};

    std::unique_ptr<CallFrame[]> frames{};
    std::size_t frame_top{};

    std::unique_ptr<ModuleFrame[]> modules{};
    std::size_t module_top{};

    StringCacher cache{};
    std::unordered_map<std::string_view, Native> natives{};

    Chunk *current_chunk{};
    RuntimeModule *current_module{};

    RuntimeContext *ctx{};

    bool colors_enabled{};
    bool debug_print_stack{};
    bool debug_print_frames{};
    bool debug_print_modules{};
    bool debug_print_instructions{};

    Chunk::InstructionSizeType read_next();

    void push(Value value) noexcept;
    void pop() noexcept;

    std::size_t get_current_line() const noexcept;
    Value::ListType *make_new_list();
    void destroy_list(Value::ListType *list);
    Value copy(Value &value);
    void copy_into(Value::ListType *list, Value::ListType *what);

    void initialize_modules();
    void teardown_modules();

    // print_color_if_enabled
    ColoredPrintHelper pcife(ColoredPrintHelper::StreamColorModifier colorizer);

    friend class RuntimeManager;

  public:
    // TODO: add proper config for this
    VirtualMachine();
    ~VirtualMachine() = default;

    VirtualMachine(const VirtualMachine &) = delete;
    VirtualMachine &operator=(const VirtualMachine &) = delete;

    void set_runtime_ctx(RuntimeContext *ctx_);
    void set_function_module_info(RuntimeModule *module, std::size_t index);

    void run_function(RuntimeFunction &function);
    void run(RuntimeModule &module);
    ExecutionState step();
    [[nodiscard]] const HashedString &store_string(std::string str);
};

#endif