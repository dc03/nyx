/* Copyright (C) 2020  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "VM.hpp"

#include "../Common.hpp"
#include "../ErrorLogger/ErrorLogger.hpp"

#include <iostream>

#define PRINT_STACK

void VM::push(const Value &value) {
    *stack_top = Value{}; // Without this, for reasons I do not know, a segfault occurs with strings
    *(stack_top++) = value;
}

void VM::pop() {
    Value destroyed = std::move(*--stack_top);
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
    if (value.is_int()) {
        return value.to_int() != 0;
    } else if (value.is_bool()) {
        return value.to_bool();
    } else if (value.is_double()) {
        return value.to_double() != 0;
    } else if (value.is_string()) {
        return value.to_string().empty();
    } else if (value.is_null()) {
        return false;
    } else {
        unreachable();
    }
}

void VM::run(RuntimeModule &main_module) {
    ip = &main_module.top_level_code.bytes[0];
    chunk = &main_module.top_level_code;

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
            std::cout << (result.is_int() ? result.to_int() : result.to_double()) << '\n';                             \
            pop_twice_push(result);                                                                                    \
        } else {                                                                                                       \
            Value result{(top_from(2).is_int() ? top_from(2).to_int() : top_from(2).to_double())                       \
                         oper                                                                                          \
                         (top_from(1).is_int() ? top_from(1).to_int() : top_from(1).to_double())};                     \
            std::cout << (result.is_int() ? result.to_int() : result.to_double()) << '\n';                             \
            pop_twice_push(result);                                                                                    \
        }                                                                                                              \
    }                                                                                                                  \
    break

    // clang-format on

#define binary_logical_instruction(oper)                                                                               \
    {                                                                                                                  \
        Value result{top_from(2).to_int() oper top_from(1).to_int()};                                                  \
        std::cout << result.to_int() << '\n';                                                                          \
        pop_twice_push(result);                                                                                        \
    }                                                                                                                  \
    break

#define binary_boolean_instruction(oper)                                                                               \
    if (top_from(2).is_int() || top_from(2).is_double()) {                                                             \
        Value result{(top_from(2).is_int() ? top_from(2).to_int() : top_from(2).to_double())oper(                      \
            top_from(1).is_int() ? top_from(1).to_int() : top_from(1).to_double())};                                   \
        std::cout << std::boolalpha << result.to_bool() << '\n';                                                       \
        pop_twice_push(result);                                                                                        \
    } else if (top_from(2).is_bool()) {                                                                                \
        Value result{top_from(2).to_bool() oper top_from(1).to_bool()};                                                \
        std::cout << std::boolalpha << result.to_bool() << '\n';                                                       \
        pop_twice_push(result);                                                                                        \
    }                                                                                                                  \
    break
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    for (;;) {
#ifdef PRINT_STACK
        for (Value *begin{stack}; begin < stack_top; begin++) {
            std::cout << "[ ";
            if (begin->is_int()) {
                std::cout << begin->to_int();
            } else if (begin->is_double()) {
                std::cout << begin->to_double();
            } else if (begin->is_bool()) {
                std::cout << std::boolalpha << begin->to_bool();
            } else if (begin->is_null()) {
                std::cout << "null";
            } else if (begin->is_string()) {
                std::cout << begin->to_string();
            }
            std::cout << " ] ";
        }
        std::cout << '\n';
#endif
        switch (read_byte()) {
            case is Instruction::CONST_SHORT: push(chunk->constants[read_byte()]); break;
            case is Instruction::CONST_LONG: {
                push(chunk->constants[read_three_bytes()]);
                break;
            }

            case is Instruction::ADD: binary_arithmetic_instruction(+);
            case is Instruction::SUB: binary_arithmetic_instruction(-);
            case is Instruction::MUL: binary_arithmetic_instruction(*);
            case is Instruction::DIV:
                if (top_from(1).is_int() && top_from(1).to_int() == 0) {
                    runtime_error("Division by zero", {});
                    break;
                } else if (top_from(1).to_double() == 0.0f) {
                    runtime_error("Division by zero", {});
                    break;
                }
                binary_arithmetic_instruction(/);

            case is Instruction::CONCAT: {
                Value result{top_from(2).to_string() + top_from(1).to_string()};
                pop_twice_push(result);
                break;
            }

            case is Instruction::SHIFT_LEFT: {
                if (top_from(2).to_int() < 0 || top_from(1).to_int() < 0) {
                    runtime_error("Bitwise shifting with negative numbers is not allowed", {});
                    break;
                }
                Value result{static_cast<int>(static_cast<unsigned int>(top_from(2).to_int())
                                              << static_cast<unsigned int>(top_from(1).to_int()))};
                std::cout << result.to_int() << '\n';
                pop_twice_push(result);
                break;
            }
            case is Instruction::SHIFT_RIGHT: {
                if (top_from(2).to_int() < 0 || top_from(1).to_int() < 0) {
                    runtime_error("Bitwise shifting with negative numbers is not allowed", {});
                    break;
                }
                Value result{static_cast<int>(static_cast<unsigned int>(top_from(2).to_int()) >>
                                              static_cast<unsigned int>(top_from(1).to_int()))};
                std::cout << result.to_int() << '\n';
                pop_twice_push(result);
                break;
            }

            case is Instruction::BIT_AND: binary_logical_instruction(&);
            case is Instruction::BIT_OR: binary_logical_instruction(|);
            case is Instruction::BIT_XOR: binary_logical_instruction(^);

            case is Instruction::MOD: {
                if (top_from(1).to_int() <= 0) {
                    runtime_error("Cannot modulo a number with a value lesser than or equal to zero", {});
                    break;
                }
                Value result{top_from(2).to_int() % top_from(1).to_int()};
                std::cout << result.to_int() << '\n';
                pop_twice_push(result);
                break;
            }

            case is Instruction::GREATER: binary_boolean_instruction(>);
            case is Instruction::LESSER: binary_boolean_instruction(<);
            case is Instruction::EQUAL: binary_boolean_instruction(==);
            case is Instruction::NOT: {
                bool truthiness = !is_truthy(top_from(1));
                std::cout << std::boolalpha << truthiness << '\n';
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
                Value result{top_from(1).is_int() ? -top_from(1).to_int() : -top_from(1).to_double()};
                pop();
                push(result);
                break;
            }

            case is Instruction::TRUE: push(Value{true}); break;
            case is Instruction::FALSE: push(Value{false}); break;
            case is Instruction::NULL_: push(Value{nullptr}); break;

            case is Instruction::ACCESS_LOCAL_SHORT: push(stack[read_byte()]); break;
            case is Instruction::ACCESS_LOCAL_LONG: {
                push(stack[read_three_bytes()]);
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

            case is Instruction::ASSIGN_LOCAL: {
                std::size_t slot = read_three_bytes();
                stack[slot] = top_from(1);
                push(stack[slot]);
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