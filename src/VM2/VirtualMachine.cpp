/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "VirtualMachine.hpp"

#include "../ErrorLogger/ErrorLogger.hpp"
#include "Disassembler.hpp"
#include "Instructions.hpp"

#include <cmath>
#include <iostream>

#define is (Chunk::byte)

VirtualMachine::VirtualMachine(bool trace_stack, bool trace_insn)
    : stack{std::make_unique<Value[]>(VirtualMachine::stack_size)},
      frames{std::make_unique<CallFrame[]>(VirtualMachine::frame_size)},
      trace_stack{trace_stack},
      trace_insn{trace_insn} {
    for (const auto &native : native_functions) {
        natives[native.name] = native;
    }
    frames[0] = CallFrame{&stack[0], {}};
}

Chunk::byte VirtualMachine::read_byte() {
    return *(ip++);
}

std::size_t VirtualMachine::read_three_bytes() {
    std::size_t bytes = read_byte();
    bytes = (bytes << 8) | read_byte();
    bytes = (bytes << 8) | read_byte();
    return bytes;
}

void VirtualMachine::push(Value value) noexcept {
    stack[stack_top++] = value;
}

void VirtualMachine::pop() noexcept {
    stack_top--;
}

std::size_t VirtualMachine::get_current_line() const noexcept {
    return current_chunk->get_line_number(ip - &current_chunk->bytes[0] - 1);
}

void VirtualMachine::run(RuntimeModule &module) {
    current_module = &module;
    current_chunk = &module.top_level_code;
    ip = &current_chunk->bytes[0];
    while (true) {
        if (step() == ExecutionState::FINISHED) {
            break;
        } else {
            step();
        }
    }
}

#define arith_binary_op(op, type, member)                                                                              \
    {                                                                                                                  \
        Value::type val2 = stack[--stack_top].member;                                                                  \
        Value::type val1 = stack[stack_top - 1].member;                                                                \
        stack[stack_top - 1].member = val1 op val2;                                                                    \
    }                                                                                                                  \
    break

#define comp_binary_op(op)                                                                                             \
    {                                                                                                                  \
        Value val2 = stack[--stack_top];                                                                               \
        Value val1 = stack[stack_top - 1];                                                                             \
        stack[stack_top - 1].w_bool = (val1 op val2);                                                                  \
        stack[stack_top - 1].tag = Value::Tag::BOOL;                                                                   \
    }                                                                                                                  \
    break

ExecutionState VirtualMachine::step() {
    if (trace_stack) {
        for (Value *begin{&stack[0]}; begin < &stack[stack_top]; begin++) {
            std::cout << "[ " << begin->repr() << " ] ";
        }
        std::cout << '\n';
    }
    if (trace_insn) {
        disassemble_instruction(*current_chunk, static_cast<Instruction>(*ip), (ip - &current_chunk->bytes[0]));
    }
    switch (read_byte()) {
        case is Instruction::HALT: return ExecutionState::FINISHED;
        case is Instruction::POP: {
            pop();
            break;
        }
        /* Push constants onto stack */
        case is Instruction::CONST_SHORT: {
            push(current_chunk->constants[read_byte()]);
            break;
        }
        case is Instruction::CONST_LONG: {
            push(current_chunk->constants[read_three_bytes()]);
            break;
        }
        /* Integer operations */
        case is Instruction::IADD: arith_binary_op(+, w_int_t, w_int);
        case is Instruction::ISUB: arith_binary_op(-, w_int_t, w_int);
        case is Instruction::IMUL: arith_binary_op(*, w_int_t, w_int);
        case is Instruction::IMOD: {
            if (stack[stack_top - 1].w_int == 0) {
                runtime_error("Cannot modulo by zero", get_current_line());
                return ExecutionState::FINISHED;
            }
            arith_binary_op(%, w_int_t, w_int);
        }
        case is Instruction::IDIV: {
            if (stack[stack_top - 1].w_int == 0) {
                runtime_error("Cannot divide by zero", get_current_line());
                return ExecutionState::FINISHED;
            }
            arith_binary_op(/, w_int_t, w_int);
        }
        case is Instruction::INEG: {
            stack[stack_top - 1].w_int = -stack[stack_top - 1].w_int;
            break;
        }
        /* Floating point operations */
        case is Instruction::FADD: arith_binary_op(+, w_float_t, w_float);
        case is Instruction::FSUB: arith_binary_op(-, w_float_t, w_float);
        case is Instruction::FMUL: arith_binary_op(*, w_float_t, w_float);
        case is Instruction::FMOD: {
            if (stack[stack_top - 1].w_float == 0.0) {
                runtime_error("Cannot modulo by zero", get_current_line());
                return ExecutionState::FINISHED;
            }
            Value::w_float_t val2 = stack[--stack_top].w_float;
            Value::w_float_t val1 = stack[stack_top - 1].w_float;
            stack[stack_top - 1].w_float = std::fmod(val1, val2);
            break;
        }
        case is Instruction::FDIV: {
            if (stack[stack_top - 1].w_float == 0.0) {
                runtime_error("Cannot divide by zero", get_current_line());
                return ExecutionState::FINISHED;
            }
            arith_binary_op(/, w_float_t, w_float);
        }
        case is Instruction::FNEG: {
            stack[stack_top - 1].w_float = -stack[stack_top - 1].w_float;
            break;
        }
        /* Floating <-> integral conversions */
        case is Instruction::FLOAT_TO_INT: {
            stack[stack_top - 1].w_int = static_cast<Value::w_int_t>(stack[stack_top - 1].w_float);
            stack[stack_top - 1].tag = Value::Tag::INT;
            break;
        }
        case is Instruction::INT_TO_FLOAT: {
            stack[stack_top - 1].w_float = static_cast<Value::w_float_t>(stack[stack_top - 1].w_int);
            stack[stack_top - 1].tag = Value::Tag::FLOAT;
            break;
        }
        /* Bitwise operations */
        case is Instruction::SHIFT_LEFT: {
            if (stack[stack_top - 1].w_int < 0) {
                runtime_error("Cannot bitshift with value less than zero", get_current_line());
            }
            arith_binary_op(<<, w_int_t, w_int);
        }
        case is Instruction::SHIFT_RIGHT: {
            if (stack[stack_top - 1].w_int < 0) {
                runtime_error("Cannot bitshift with value less than zero", get_current_line());
            }
            arith_binary_op(>>, w_int_t, w_int);
        }
        case is Instruction::BIT_AND: arith_binary_op(&, w_int_t, w_int);
        case is Instruction::BIT_OR: arith_binary_op(|, w_int_t, w_int);
        case is Instruction::BIT_NOT: stack[stack_top - 1].w_int = ~stack[stack_top - 1].w_int; break;
        case is Instruction::BIT_XOR: arith_binary_op(^, w_int_t, w_int);
        /* Logical operations */
        case is Instruction::NOT: {
            stack[stack_top - 1].w_bool = !stack[stack_top - 1];
            stack[stack_top - 1].tag = Value::Tag::BOOL;
            break;
        }
        case is Instruction::EQUAL: comp_binary_op(==);
        case is Instruction::GREATER: comp_binary_op(>);
        case is Instruction::LESSER: comp_binary_op(<);
        /* Constant operations */
        case is Instruction::PUSH_TRUE: {
            stack[stack_top].w_bool = true;
            stack[stack_top++].tag = Value::Tag::BOOL;
            break;
        }
        case is Instruction::PUSH_FALSE: {
            stack[stack_top].w_bool = false;
            stack[stack_top++].tag = Value::Tag::BOOL;
            break;
        }
        case is Instruction::PUSH_NULL: {
            stack[stack_top].w_null = nullptr;
            stack[stack_top++].tag = Value::Tag::NULL_;
            break;
        }
        /* Jump operations */
        case is Instruction::JUMP_FORWARD: {
            ip += read_three_bytes();
            break;
        }
        case is Instruction::JUMP_BACKWARD: {
            ip -= read_three_bytes();
            break;
        }
        case is Instruction::JUMP_IF_TRUE: {
            ip += stack[stack_top - 1] ? read_three_bytes() : 3;
            break;
        }
        case is Instruction::JUMP_IF_FALSE: {
            ip += stack[stack_top - 1] ? 3 : read_three_bytes();
            break;
        }
        case is Instruction::POP_JUMP_IF_EQUAL: {
            if (stack[stack_top - 2] == stack[stack_top - 1]) {
                ip += read_three_bytes();
                stack_top--;
            } else {
                ip += 3;
            }
            stack_top--;
            break;
        }
        case is Instruction::POP_JUMP_IF_FALSE: {
            ip += !stack[stack_top - 1] ? read_three_bytes() : 3;
            stack_top--;
            break;
        }
        case is Instruction::POP_JUMP_BACK_IF_TRUE: {
            ip -= stack[stack_top - 1] ? read_three_bytes() : -3;
            stack_top--;
            break;
        }
        /* Local variable operations */
        case is Instruction::ASSIGN_LOCAL: {
            Value &assigned = frames[frame_top].stack[read_three_bytes()];
            if (assigned.tag == Value::Tag::REF) {
                *assigned.w_ref = stack[stack_top - 1];
            } else {
                assigned = stack[stack_top - 1];
            }
            break;
        }
        case is Instruction::ACCESS_LOCAL_SHORT: {
            push(frames[frame_top].stack[read_byte()]);
            break;
        }
        case is Instruction::ACCESS_LOCAL_LONG: {
            push(frames[frame_top].stack[read_three_bytes()]);
            break;
        }
        case is Instruction::MAKE_REF_TO_LOCAL: {
            push(Value{&frames[frame_top].stack[read_three_bytes()]});
            break;
        }
        case is Instruction::DEREF: {
            stack[stack_top - 1] = *stack[stack_top - 1].w_ref;
            break;
        }
        /* Global variable operations */
        case is Instruction::ASSIGN_GLOBAL: {
            Value &assigned = stack[read_three_bytes()];
            if (assigned.tag == Value::Tag::REF) {
                *assigned.w_ref = stack[stack_top - 1];
            } else {
                assigned = stack[stack_top - 1];
            }
            break;
        }
        case is Instruction::ACCESS_GLOBAL_SHORT: {
            push(Value{stack[read_byte()]});
            break;
        }
        case is Instruction::ACCESS_GLOBAL_LONG: {
            push(Value{stack[read_three_bytes()]});
            break;
        }
        case is Instruction::MAKE_REF_TO_GLOBAL: {
            push(Value{&stack[read_three_bytes()]});
            break;
        }
        /* Function calls */
        case is Instruction::LOAD_FUNCTION: break;
        case is Instruction::CALL_FUNCTION: break;
        case is Instruction::CALL_NATIVE: {
            NativeFn called = natives[stack[--stack_top].w_str];
            Value result = called.code(&stack[stack_top] - called.arity);
            stack_top -= called.arity;
            stack[stack_top++] = result;
            break;
        }
        case is Instruction::RETURN: break;
        case is Instruction::TRAP_RETURN: {
            runtime_error("Reached end of non-null function", get_current_line());
            return ExecutionState::FINISHED;
        }
        /* Copying */
        case is Instruction::COPY: break;
    }
    return ExecutionState::RUNNING;
}

#undef arith_binary_op
#undef comp_binary_op