/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "VM.hpp"

#include "../Common.hpp"
#include "../ErrorLogger/ErrorLogger.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <utility>

#ifndef NDEBUG
#define PRINT_STACK
#endif

VM::VM() {
    frames = new CallFrame[max_stack_frames];
    frame_top = frames;
    stack = new Value[max_stack_frames * max_locals_per_frame];
    stack_top = stack;
    for (const auto &native : native_functions) {
        define_native(native.name, native);
    }
}

VM::~VM() {
    delete[] frames;
    delete[] stack;
}

void VM::push(const Value &value) {
    //*stack_top = Value{}; // Without this, for reasons I do not know, a segfault occurs with strings
    // if (value.is_string()) {
    //    stack_top->as.string = std::string{};
    //}
    *(stack_top++) = value;
}

void VM::push(Value &&value) {
    *(stack_top++) = std::move(value);
}

void VM::pop() {
    if (top_from(1).is_list()) {
        Value destroyed = std::move(*--stack_top);
    } else {
        --stack_top;
    }
    //    Value destroyed = std::move(*--stack_top);
}

void VM::define_native(const std::string &name, NativeFn code) {
    natives[name] = code;
}

Value &VM::top_from(std::size_t distance) const {
    return *(stack_top - distance);
}

Chunk::byte VM::read_byte() {
    return *(ip++);
}

std::size_t VM::read_three_bytes() {
    std::size_t bytes = read_byte();
    bytes = (bytes << 8) | read_byte();
    bytes = (bytes << 8) | read_byte();
    return bytes;
}

bool is_truthy(Value &value) {
    return bool(value);
}

void VM::recursively_size_list(List &list, Value *size, std::size_t depth) {
    if (depth > 1) {
        std::vector<std::unique_ptr<List>> &stored_list = list.to_list_list();
        stored_list.resize(size->to_int());
        if (depth > 2) {
            std::generate(stored_list.begin(), stored_list.end(),
                []() { return std::make_unique<List>(std::vector<std::unique_ptr<List>>{}); });
        } else {
            auto &assigned = (size - 2)->to_list();
            std::generate(stored_list.begin(), stored_list.end(), [&assigned]() {
                if (assigned.is_int_list()) {
                    return std::make_unique<List>(assigned.to_int_list());
                } else if (assigned.is_float_list()) {
                    return std::make_unique<List>(assigned.to_float_list());
                } else if (assigned.is_string_list()) {
                    return std::make_unique<List>(assigned.to_string_list());
                } else if (assigned.is_ref_list()) {
                    return std::make_unique<List>(assigned.to_ref_list());
                } else if (assigned.is_bool_list()) {
                    return std::make_unique<List>(assigned.to_bool_list());
                }
                unreachable();
            });
        }
        for (auto &contained : stored_list) {
            recursively_size_list(*contained, size - 1, depth - 1);
        }
    } else {
        list.resize(size->to_int());
    }
}

List VM::make_list(List::tag type) {
    switch (type) {
        case List::tag::INT_LIST: return List{std::vector<int>{}};
        case List::tag::FLOAT_LIST: return List{std::vector<double>{}};
        case List::tag::STRING_LIST: return List{std::vector<std::string>{}};
        case List::tag::BOOL_LIST: return List{std::vector<char>{}};
        case List::tag::REF_LIST: return List{std::vector<Value *>{}};
        case List::tag::LIST_LIST: return List{std::vector<std::unique_ptr<List>>{}};
        default: break;
    }
    unreachable();
}

void VM::run(RuntimeModule &main_module) {
    ip = &main_module.top_level_code.bytes[0];
    chunk = &main_module.top_level_code;
    current_module = &main_module;

#define is (unsigned char)

#define pop_twice_push(result)                                                                                         \
    do {                                                                                                               \
        pop();                                                                                                         \
        pop();                                                                                                         \
        push(result);                                                                                                  \
    } while (0)

// clang-format off
#define binary_arithmetic_instruction(oper)                                                                            \
    {                                                                                                                  \
        if (top_from(2).is_int() && top_from(1).is_int()) {                                                            \
            Value result{top_from(2).to_int() oper top_from(1).to_int()};                                              \
            pop_twice_push(result);                                                                                    \
        } else {                                                                                                       \
            Value result{top_from(2).to_numeric() oper top_from(1).to_numeric()};                                      \
            pop_twice_push(result);                                                                                    \
        }                                                                                                              \
    }

    // clang-format on

#define binary_logical_instruction(oper)                                                                               \
    {                                                                                                                  \
        Value result{top_from(2).to_int() oper top_from(1).to_int()};                                                  \
        pop_twice_push(result);                                                                                        \
    }

#define binary_boolean_instruction(oper)                                                                               \
    if (top_from(2).is_bool()) {                                                                                       \
        Value result{top_from(2).to_bool() oper top_from(1).to_bool()};                                                \
        pop_twice_push(result);                                                                                        \
    } else if (top_from(2).is_string()) {                                                                              \
        Value result{top_from(2).to_string() oper top_from(1).to_string()};                                            \
        pop_twice_push(result);                                                                                        \
    } else {                                                                                                           \
        Value result{top_from(2).to_numeric() oper top_from(1).to_numeric()};                                          \
        pop_twice_push(result);                                                                                        \
    }

#define binary_compound_assignment(oper, what)                                                                         \
    {                                                                                                                  \
        Value *incremented = &what[read_three_bytes()];                                                                \
        if (incremented->is_ref()) {                                                                                   \
            incremented = incremented->to_referred();                                                                  \
        }                                                                                                              \
        double added = top_from(1).to_numeric();                                                                       \
        if (incremented->is_int()) {                                                                                   \
            incremented->to_int() oper int(added);                                                                     \
        } else {                                                                                                       \
            incremented->to_double() oper added;                                                                       \
        }                                                                                                              \
        pop();                                                                                                         \
        push(*incremented);                                                                                            \
    }
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    for (;;) {
#ifdef PRINT_STACK
        for (Value *begin{stack}; begin < stack_top; begin++) {
            std::cout << "[ ";
            std::cout << begin->repr();
            std::cout << " ] ";
        }
        // std::cout << ' ' << ip - &chunk->bytes[0] << '\n';
        std::cout << '\n';
#endif
        switch (read_byte()) {
            case is Instruction::CONST_SHORT: push(chunk->constants[read_byte()]); break;
            case is Instruction::CONST_LONG: {
                push(chunk->constants[read_three_bytes()]);
                break;
            }

            case is Instruction::ADD: binary_arithmetic_instruction(+); break;
            case is Instruction::SUB: binary_arithmetic_instruction(-); break;
            case is Instruction::MUL: binary_arithmetic_instruction(*); break;
            case is Instruction::DIV:
                if (top_from(1).is_int() && top_from(1).to_int() == 0) {
                    runtime_error("Division by zero", chunk->get_line_number(ip - &chunk->bytes[0] - 1));
                    return;
                } else if (top_from(1).to_double() == 0.0f) {
                    runtime_error("Division by zero", chunk->get_line_number(ip - &chunk->bytes[0] - 1));
                    return;
                }
                binary_arithmetic_instruction(/);
                break;

            case is Instruction::CONCAT: {
                Value result{top_from(2).to_string() + top_from(1).to_string()};
                pop_twice_push(result);
                break;
            }

            case is Instruction::SHIFT_LEFT: {
                if (top_from(2).to_int() < 0 || top_from(1).to_int() < 0) {
                    runtime_error("Bitwise shifting with negative numbers is not allowed",
                        chunk->get_line_number(ip - &chunk->bytes[0] - 1));
                    return;
                }
                Value result{static_cast<int>(static_cast<unsigned int>(top_from(2).to_int())
                                              << static_cast<unsigned int>(top_from(1).to_int()))};
                pop_twice_push(result);
                break;
            }
            case is Instruction::SHIFT_RIGHT: {
                if (top_from(2).to_int() < 0 || top_from(1).to_int() < 0) {
                    runtime_error("Bitwise shifting with negative numbers is not allowed",
                        chunk->get_line_number(ip - &chunk->bytes[0] - 1));
                    return;
                }
                Value result{static_cast<int>(static_cast<unsigned int>(top_from(2).to_int()) >>
                                              static_cast<unsigned int>(top_from(1).to_int()))};
                pop_twice_push(result);
                break;
            }

            case is Instruction::BIT_AND: binary_logical_instruction(&); break;
            case is Instruction::BIT_OR: binary_logical_instruction(|); break;
            case is Instruction::BIT_XOR: binary_logical_instruction(^); break;

            case is Instruction::MOD: {
                if (top_from(1).to_int() <= 0) {
                    runtime_error("Cannot modulo a number with a value lesser than or equal to zero",
                        chunk->get_line_number(ip - &chunk->bytes[0] - 1));
                    return;
                }
                Value result{top_from(2).to_int() % top_from(1).to_int()};
                pop_twice_push(result);
                break;
            }

            case is Instruction::GREATER: binary_boolean_instruction(>); break;
            case is Instruction::LESSER: binary_boolean_instruction(<); break;
            case is Instruction::EQUAL: {
                Value result{top_from(2) == top_from(1)};
                pop_twice_push(result);
                break;
            }

            case is Instruction::NOT: {
                bool truthiness = !is_truthy(top_from(1));
                pop();
                push(Value{truthiness});
                break;
            }

            case is Instruction::BIT_NOT: {
                int result = ~top_from(1).to_int();
                pop();
                push(Value{result});
                break;
            }
            case is Instruction::NEGATE: {
                if (top_from(1).is_int()) {
                    Value result{-top_from(1).to_int()};
                    pop();
                    push(result);
                } else {
                    Value result{-top_from(1).to_double()};
                    pop();
                    push(result);
                }
                break;
            }

            case is Instruction::TRUE: push(Value{true}); break;
            case is Instruction::FALSE: push(Value{false}); break;
            case is Instruction::NULL_: push(Value{nullptr}); break;

            case is Instruction::ACCESS_LOCAL_SHORT: push(frame_top->stack_slots[read_byte()]); break;
            case is Instruction::ACCESS_LOCAL_LONG: push(frame_top->stack_slots[read_three_bytes()]); break;
            case is Instruction::ACCESS_GLOBAL_SHORT: push(stack[read_byte()]); break;
            case is Instruction::ACCESS_GLOBAL_LONG: push(stack[read_three_bytes()]); break;

            case is Instruction::JUMP_FORWARD: {
                ip += read_three_bytes();
                break;
            }
            case is Instruction::JUMP_BACKWARD: {
                ip -= read_three_bytes();
                break;
            }

            case is Instruction::JUMP_IF_FALSE: {
                if (!is_truthy(top_from(1))) {
                    ip += read_three_bytes();
                } else {
                    ip += 3;
                }
                break;
            }

            case is Instruction::JUMP_IF_TRUE: {
                if (is_truthy(top_from(1))) {
                    ip += read_three_bytes();
                } else {
                    ip += 3;
                }
                break;
            }

            case is Instruction::POP_JUMP_IF_EQUAL: {
                if (top_from(2) == top_from(1)) {
                    ip += read_three_bytes();
                    pop();
                } else {
                    ip += 3;
                }
                pop();
                break;
            }

            case is Instruction::POP_JUMP_IF_FALSE: {
                if (!is_truthy(top_from(1))) {
                    ip += read_three_bytes();
                } else {
                    ip += 3;
                }
                pop();
                break;
            }

            case is Instruction::POP_JUMP_BACK_IF_TRUE: {
                if (is_truthy(top_from(1))) {
                    ip -= read_three_bytes();
                } else {
                    ip += 3;
                }
                pop();
                break;
            }

            case is Instruction::ASSIGN_LOCAL: {
                std::size_t slot = read_three_bytes();
                Value *assigned = &frame_top->stack_slots[slot];
                if (assigned->is_ref()) {
                    assigned = assigned->to_referred();
                }
                *assigned = top_from(1);
                pop();
                push(*assigned);
                break;
            }

            case is Instruction::MAKE_REF_TO_LOCAL: {
                std::size_t slot = read_three_bytes();
                push(Value{&frame_top->stack_slots[slot]});
                break;
            }

            case is Instruction::DEREF: {
                Value referred = *top_from(1).to_referred();
                pop();
                push(referred);
                break;
            }

            case is Instruction::INCR_LOCAL: binary_compound_assignment(+=, frame_top->stack_slots); break;
            case is Instruction::DECR_LOCAL: binary_compound_assignment(-=, frame_top->stack_slots); break;
            case is Instruction::MUL_LOCAL: binary_compound_assignment(*=, frame_top->stack_slots); break;
            case is Instruction::DIV_LOCAL: {
                if (top_from(1).to_numeric() == 0) {
                    runtime_error("Division by zero", chunk->get_line_number(ip - &chunk->bytes[0] - 1));
                    return;
                }
                binary_compound_assignment(/=, frame_top->stack_slots);
                break;
            }

            case is Instruction::ASSIGN_GLOBAL: {
                std::size_t slot = read_three_bytes();
                Value *assigned = &stack[slot];
                if (assigned->is_ref()) {
                    assigned = assigned->to_referred();
                }
                *assigned = top_from(1);
                pop();
                push(*assigned);
                break;
            }

            case is Instruction::MAKE_REF_TO_GLOBAL: {
                std::size_t slot = read_three_bytes();
                push(Value{&stack[slot]});
                break;
            }

            case is Instruction::INCR_GLOBAL: binary_compound_assignment(+=, stack); break;
            case is Instruction::DECR_GLOBAL: binary_compound_assignment(-=, stack); break;
            case is Instruction::MUL_GLOBAL: binary_compound_assignment(*=, stack); break;
            case is Instruction::DIV_GLOBAL: {
                if (top_from(1).to_numeric() == 0) {
                    runtime_error("Division by zero", chunk->get_line_number(ip - &chunk->bytes[0] - 1));
                    return;
                }
                binary_compound_assignment(/=, stack);
                break;
            }

            case is Instruction::LOAD_FUNCTION: {
                RuntimeFunction *function = &current_module->functions[top_from(1).to_string()];
                pop();
                push(Value{function});
                break;
            }

            case is Instruction::CALL_FUNCTION: {
                RuntimeFunction *called = top_from(1).to_function();
                *(++frame_top) = CallFrame{stack_top - called->arity - 1, chunk, ip};
                chunk = &called->code;
                ip = &called->code.bytes[0];
                pop();
                break;
            }

            case is Instruction::RETURN: {
                Value result = std::move(top_from(1));
                pop();
                std::size_t pops = read_three_bytes();
                while (pops-- > 0) {
                    pop();
                }
                ip = frame_top->return_ip;
                chunk = frame_top->return_chunk;
                --frame_top;
                push(std::move(result));
                break;
            }

            case is Instruction::CALL_NATIVE: {
                NativeFn called = natives[top_from(1).to_string()];
                pop();
                Value result = called.code(stack_top - called.arity);
                for (std::size_t i = 0; i < called.arity; i++) {
                    pop();
                }
                push(result);
                break;
            }

            case is Instruction::MAKE_LIST: {
                Chunk::byte type = read_byte();
                push(Value{make_list(static_cast<List::tag>(type))});
                break;
            }

            case is Instruction::ALLOC_AT_LEAST: {
                std::size_t size = top_from(1).to_int();
                pop();
                List &list = top_from(1).to_list();
                if (list.size() >= size) {
                    break;
                }

                list.resize(size);
                break;
            }

            case is Instruction::ALLOC_NESTED_LISTS: {
                std::size_t depth = read_byte();
                List &list = top_from(depth + 2).to_list();
                recursively_size_list(list, &top_from(1), depth);
                while (depth-- > 0) {
                    pop(); // The sizes
                }
                pop(); // The innermost list
                break;
            }

            case is Instruction::INDEX_LIST: {
                List &list = top_from(2).to_list();
                std::size_t index = top_from(1).to_int();
                Value result = list.at(index);
                pop_twice_push(result);
                break;
            }

            case is Instruction::CHECK_INDEX: {
                List &list = top_from(2).to_list();
                std::size_t index = top_from(1).to_int();
                if (index >= list.size()) {
                    runtime_error("List index out of range", chunk->get_line_number(ip - &chunk->bytes[0] - 1));
                    return;
                }
                break;
            }

            case is Instruction::ASSIGN_LIST_AT: {
                List &list = top_from(3).to_list();
                std::size_t index = top_from(2).to_int();
                Value &value = top_from(1);
                Value result = list.assign_at(index, value);
                pop();
                pop_twice_push(result);
                break;
            }

            case is Instruction::COPY: {
                Value &copied = top_from(1);
                if (copied.is_list()) {
                    List &list = copied.to_list();
                    List copy = make_list(list.type());
                    copy.resize(list.size());
                    for (std::size_t i = 0; i < copy.size(); i++) {
                        Value assigned = list.at(i);
                        copy.assign_at(i, assigned);
                    }
                    pop();
                    push(Value{std::move(copy)});
                }
                break;
            }

            case is Instruction::POP: pop(); break;
            case is Instruction::HALT: return;
        }
    }
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#undef binary_arithmetic_instruction
#undef binary_logical_instruction
#undef pop_twice_push
#undef is
}
