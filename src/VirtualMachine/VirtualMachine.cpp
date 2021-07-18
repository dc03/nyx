/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "VirtualMachine.hpp"

#include "../ErrorLogger/ErrorLogger.hpp"
#include "Disassembler.hpp"
#include "Instructions.hpp"
#include "StringCacher.hpp"

#include <cmath>
#include <iostream>

#define is (Chunk::InstructionSizeType)

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

Chunk::InstructionSizeType VirtualMachine::read_next() {
    return *(ip++);
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

void VirtualMachine::destroy_list(Value::ListType *list) {
    for (auto &elem : *list) {
        if (elem.tag == Value::Tag::STRING) {
            cache.remove(*elem.w_str);
        } else if (elem.tag == Value::Tag::LIST) {
            destroy_list(elem.w_list);
        }
    }
    delete list;
}

Value::ListType *VirtualMachine::make_new_list() {
    return new Value::ListType;
}

Value VirtualMachine::copy(Value &value) {
    if (value.tag == Value::Tag::LIST || value.tag == Value::Tag::LIST_REF) {
        Value::ListType *new_list = make_new_list();
        new_list->resize(value.w_list->size());
        copy_into(new_list, value.w_list);
        return Value{new_list};
    } else {
        return value;
    }
}

void VirtualMachine::copy_into(Value::ListType *list, Value::ListType *what) {
    for (std::size_t i = 0; i < what->size(); i++) {
        if ((*what)[i].tag == Value::Tag::LIST) {
            (*list)[i] = copy((*what)[i]);
        } else if ((*what)[i].tag == Value::Tag::STRING) {
            (*list)[i] = Value{&cache.insert(*(*what)[i].w_str)};
        } else {
            (*list)[i] = (*what)[i];
        }
    }
}

void VirtualMachine::run(RuntimeModule &module) {
    current_module = &module;
    current_chunk = &module.top_level_code;
    ip = &current_chunk->bytes[0];
    while (step() != ExecutionState::FINISHED)
        ;
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
        disassemble_instruction(*current_chunk, static_cast<Instruction>(*ip >> 24), (ip - &current_chunk->bytes[0]));
    }
    Chunk::InstructionSizeType next = read_next();
    Chunk::InstructionSizeType instruction = next & 0xff00'0000;
    Chunk::InstructionSizeType operand = next & 0x00ff'ffff;
    switch (instruction >> 24) {
        case is Instruction::HALT: return ExecutionState::FINISHED;
        case is Instruction::POP: {
            pop();
            break;
        }
        /* Push constants onto stack */
        case is Instruction::CONSTANT: {
            push(current_chunk->constants[operand]);
            break;
        }
        /* Integer operations */
        case is Instruction::IADD: arith_binary_op(+, IntType, w_int);
        case is Instruction::ISUB: arith_binary_op(-, IntType, w_int);
        case is Instruction::IMUL: arith_binary_op(*, IntType, w_int);
        case is Instruction::IMOD: {
            if (stack[stack_top - 1].w_int == 0) {
                runtime_error("Cannot modulo by zero", get_current_line());
                return ExecutionState::FINISHED;
            }
            arith_binary_op(%, IntType, w_int);
        }
        case is Instruction::IDIV: {
            if (stack[stack_top - 1].w_int == 0) {
                runtime_error("Cannot divide by zero", get_current_line());
                return ExecutionState::FINISHED;
            }
            arith_binary_op(/, IntType, w_int);
        }
        case is Instruction::INEG: {
            stack[stack_top - 1].w_int = -stack[stack_top - 1].w_int;
            break;
        }
        /* Floating point operations */
        case is Instruction::FADD: arith_binary_op(+, FloatType, w_float);
        case is Instruction::FSUB: arith_binary_op(-, FloatType, w_float);
        case is Instruction::FMUL: arith_binary_op(*, FloatType, w_float);
        case is Instruction::FMOD: {
            if (stack[stack_top - 1].w_float == 0.0) {
                runtime_error("Cannot modulo by zero", get_current_line());
                return ExecutionState::FINISHED;
            }
            Value::FloatType val2 = stack[--stack_top].w_float;
            Value::FloatType val1 = stack[stack_top - 1].w_float;
            stack[stack_top - 1].w_float = std::fmod(val1, val2);
            break;
        }
        case is Instruction::FDIV: {
            if (stack[stack_top - 1].w_float == 0.0) {
                runtime_error("Cannot divide by zero", get_current_line());
                return ExecutionState::FINISHED;
            }
            arith_binary_op(/, FloatType, w_float);
        }
        case is Instruction::FNEG: {
            stack[stack_top - 1].w_float = -stack[stack_top - 1].w_float;
            break;
        }
        /* Floating <-> integral conversions */
        case is Instruction::FLOAT_TO_INT: {
            stack[stack_top - 1].w_int = static_cast<Value::IntType>(stack[stack_top - 1].w_float);
            stack[stack_top - 1].tag = Value::Tag::INT;
            break;
        }
        case is Instruction::INT_TO_FLOAT: {
            stack[stack_top - 1].w_float = static_cast<Value::FloatType>(stack[stack_top - 1].w_int);
            stack[stack_top - 1].tag = Value::Tag::FLOAT;
            break;
        }
        /* Bitwise operations */
        case is Instruction::SHIFT_LEFT: {
            if (stack[stack_top - 1].w_int < 0) {
                runtime_error("Cannot bitshift with value less than zero", get_current_line());
            }
            arith_binary_op(<<, IntType, w_int);
        }
        case is Instruction::SHIFT_RIGHT: {
            if (stack[stack_top - 1].w_int < 0) {
                runtime_error("Cannot bitshift with value less than zero", get_current_line());
            }
            arith_binary_op(>>, IntType, w_int);
        }
        case is Instruction::BIT_AND: arith_binary_op(&, IntType, w_int);
        case is Instruction::BIT_OR: arith_binary_op(|, IntType, w_int);
        case is Instruction::BIT_NOT: stack[stack_top - 1].w_int = ~stack[stack_top - 1].w_int; break;
        case is Instruction::BIT_XOR: arith_binary_op(^, IntType, w_int);
        /* Logical operations */
        case is Instruction::NOT: {
            stack[stack_top - 1].w_bool = not stack[stack_top - 1];
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
            ip += operand;
            break;
        }
        case is Instruction::JUMP_BACKWARD: {
            ip -= operand;
            break;
        }
        case is Instruction::JUMP_IF_TRUE: {
            if (stack[stack_top - 1]) {
                ip += operand;
            }
            break;
        }
        case is Instruction::JUMP_IF_FALSE: {
            if (not stack[stack_top - 1]) {
                ip += operand;
            }
            break;
        }
        case is Instruction::POP_JUMP_IF_EQUAL: {
            if (stack[stack_top - 2] == stack[stack_top - 1]) {
                ip += operand;
                stack_top--;
            }
            stack_top--;
            break;
        }
        case is Instruction::POP_JUMP_IF_FALSE: {
            if (not stack[--stack_top]) {
                ip += operand;
            }
            break;
        }
        case is Instruction::POP_JUMP_BACK_IF_TRUE: {
            if (stack[--stack_top]) {
                ip -= operand;
            }
            break;
        }
        /* Local variable operations */
        case is Instruction::ASSIGN_LOCAL: {
            Value *assigned = &frames[frame_top].stack[operand];
            if (assigned->tag == Value::Tag::REF) {
                assigned = assigned->w_ref;
            }
            if (assigned->tag == Value::Tag::STRING) {
                cache.remove(*assigned->w_str);
                *assigned = Value{&cache.insert(*stack[stack_top - 1].w_str)};
            } else {
                *assigned = stack[stack_top - 1];
            }
            break;
        }
        case is Instruction::ACCESS_LOCAL: {
            push(frames[frame_top].stack[operand]);
            if (stack[stack_top - 1].tag == Value::Tag::STRING) {
                (void)cache.insert(*stack[stack_top - 1].w_str);
            }
            break;
        }
        case is Instruction::MAKE_REF_TO_LOCAL: {
            Value &value = frames[frame_top].stack[operand];
            if (value.tag == Value::Tag::LIST) {
                push(Value{value.w_list});
                stack[stack_top - 1].tag = Value::Tag::LIST_REF;
            } else {
                push(Value{&value});
            }
            break;
        }
        case is Instruction::DEREF: {
            stack[stack_top - 1] = *stack[stack_top - 1].w_ref;
            break;
        }
        /* Global variable operations */
        case is Instruction::ASSIGN_GLOBAL: {
            Value *assigned = &stack[operand];
            if (assigned->tag == Value::Tag::REF) {
                assigned = assigned->w_ref;
            }
            if (assigned->tag == Value::Tag::STRING) {
                cache.remove(*assigned->w_str);
                *assigned = Value{&cache.insert(*stack[stack_top - 1].w_str)};
            } else {
                *assigned = stack[stack_top - 1];
            }
            break;
        }
        case is Instruction::ACCESS_GLOBAL: {
            push(Value{stack[operand]});
            if (stack[stack_top - 1].tag == Value::Tag::STRING) {
                (void)cache.insert(*stack[stack_top - 1].w_str);
            }
            break;
        }
        case is Instruction::MAKE_REF_TO_GLOBAL: {
            Value &value = stack[operand];
            if (value.tag == Value::Tag::LIST) {
                push(Value{value.w_list});
                stack[stack_top - 1].tag = Value::Tag::LIST_REF;
            } else {
                push(Value{&value});
            }
            break;
        }
        /* Function calls */
        case is Instruction::LOAD_FUNCTION: {
            RuntimeFunction *function = &current_module->functions[stack[stack_top - 1].w_str->str];
            stack[stack_top - 1].w_fun = function;
            stack[stack_top - 1].tag = Value::Tag::FUNCTION;
            break;
        }
        case is Instruction::CALL_FUNCTION: {
            RuntimeFunction *called = stack[--stack_top].w_fun;
            frames[++frame_top] = CallFrame{&stack[stack_top - called->arity], current_chunk, ip};
            current_chunk = &called->code;
            ip = &called->code.bytes[0];
            break;
        }
        case is Instruction::CALL_NATIVE: {
            NativeFn called = natives[stack[--stack_top].w_str->str];
            cache.remove(*stack[stack_top].w_str);
            Value result = called.code(*this, &stack[stack_top] - called.arity);
            stack[stack_top - called.arity - 1] = result;
            break;
        }
        case is Instruction::RETURN: {
            Value result = stack[--stack_top];
            std::size_t locals_popped = operand;
            stack[stack_top - locals_popped - 1] = result;
            while (locals_popped-- > 0) {
                stack_top--;
                if (stack[stack_top].tag == Value::Tag::STRING) {
                    cache.remove(*stack[stack_top].w_str);
                } else if (stack[stack_top].tag == Value::Tag::LIST) {
                    destroy_list(stack[stack_top].w_list);
                }
            }
            ip = frames[frame_top].return_ip;
            current_chunk = frames[frame_top--].return_chunk;
            break;
        }
        case is Instruction::TRAP_RETURN: {
            runtime_error("Reached end of non-null function", get_current_line());
            return ExecutionState::FINISHED;
        }
        /* String instructions */
        case is Instruction::CONSTANT_STRING: {
            Value::StringType string = current_chunk->constants[operand].w_str;
            push(Value{&cache.insert(*string)});
            break;
        }
        case is Instruction::POP_STRING: {
            cache.remove(*stack[--stack_top].w_str);
            break;
        }
        case is Instruction::CONCATENATE: {
            Value::StringType val2 = stack[--stack_top].w_str;
            Value::StringType val1 = stack[stack_top - 1].w_str;
            stack[stack_top - 1].w_str = &cache.concat(*val1, *val2);
            cache.remove(*val1);
            cache.remove(*val2);
            break;
        }
        /* List instructions */
        case is Instruction::MAKE_LIST: {
            std::size_t size = stack[--stack_top].w_int;
            push(Value{make_new_list()});
            if (size != 0) {
                stack[stack_top - 1].w_list->resize(size);
            }
            break;
        }
        case is Instruction::COPY_LIST: {
            // COPY_LIST is a no-op for temporary lists, i.e those not bound to names
            if (stack[stack_top - 1].tag == Value::Tag::LIST_REF) {
                stack[stack_top - 1] = copy(stack[stack_top - 1]);
                stack[stack_top - 1].tag = Value::Tag::LIST;
            }
            break;
        }
        case is Instruction::APPEND_LIST: {
            Value &appended = stack[--stack_top];
            Value *list = &stack[stack_top - 1];
            if (list->tag == Value::Tag::REF) {
                list = list->w_ref;
            }
            list->w_list->push_back(appended);
            break;
        }
        case is Instruction::ASSIGN_LIST: {
            Value &assigned = stack[--stack_top];
            Value &index = stack[--stack_top];
            Value *list = &stack[stack_top - 1];
            if (list->tag == Value::Tag::REF) {
                list = list->w_ref;
            }
            Value::Tag tag = (*list->w_list)[index.w_int].tag;
            if (tag == Value::Tag::LIST) {
                destroy_list((*list->w_list)[index.w_int].w_list);
            } else if (tag == Value::Tag::STRING) {
                cache.remove(*(*list->w_list)[index.w_int].w_str);
                (void)cache.insert(*assigned.w_str);
            }
            (*list->w_list)[index.w_int] = assigned;
            stack[stack_top - 1] = (*list->w_list)[index.w_int];
            if (tag == Value::Tag::LIST) {
                stack[stack_top - 1].tag = Value::Tag::LIST_REF;
            }
            break;
        }
        case is Instruction::INDEX_LIST: {
            Value &index = stack[--stack_top];
            Value *list = &stack[stack_top - 1];
            if (list->tag == Value::Tag::REF) {
                list = list->w_ref;
            }
            stack[stack_top - 1] = (*list->w_list)[index.w_int];
            break;
        }
        case is Instruction::CHECK_INDEX: {
            Value &index = stack[stack_top - 1];
            Value *list = &stack[stack_top - 2];
            if (list->tag == Value::Tag::REF) {
                list = list->w_ref;
            }
            if (index.w_int > static_cast<int>(list->w_list->size())) {
                runtime_error("List index out of range", get_current_line());
                return ExecutionState::FINISHED;
            }
            break;
        }
        case is Instruction::ACCESS_LOCAL_LIST: {
            push(frames[frame_top].stack[operand]);
            stack[stack_top - 1].tag = Value::Tag::LIST_REF;
            break;
        }
        case is Instruction::ACCESS_GLOBAL_LIST: {
            push(stack[operand]);
            stack[stack_top - 1].tag = Value::Tag::LIST_REF;
            break;
        }
        case is Instruction::ASSIGN_LOCAL_LIST: {
            Value &assigned = frames[frame_top].stack[operand];
            if (assigned.w_list != nullptr) {
                destroy_list(assigned.w_list);
            }
            if (assigned.tag == Value::Tag::REF) {
                *assigned.w_ref = stack[stack_top - 1];
            } else {
                assigned = stack[stack_top - 1];
            }
            stack[stack_top - 1].tag = Value::Tag::LIST_REF;
            break;
        }
        case is Instruction::ASSIGN_GLOBAL_LIST: {
            Value &assigned = stack[operand];
            if (assigned.w_list != nullptr) {
                destroy_list(assigned.w_list);
            }
            if (assigned.tag == Value::Tag::REF) {
                *assigned.w_ref = stack[stack_top - 1];
            } else {
                assigned = stack[stack_top - 1];
            }
            stack[stack_top - 1].tag = Value::Tag::LIST_REF;
            break;
        }
        case is Instruction::POP_LIST: {
            if (stack[stack_top - 1].tag == Value::Tag::LIST) {
                destroy_list(stack[--stack_top].w_list);
            } else if (stack[stack_top - 1].tag == Value::Tag::LIST_REF) {
                stack_top--;
            }
            break;
        }
        /* Miscellaneous */
        case is Instruction::ACCESS_FROM_TOP: {
            push(stack[stack_top - operand]);
            break;
        }
        case is Instruction::ASSIGN_FROM_TOP: {
            Value *assigned = &stack[stack_top - operand];
            if (assigned->tag == Value::Tag::REF) {
                assigned = assigned->w_ref;
            }
            if (assigned->tag == Value::Tag::STRING) {
                cache.remove(*assigned->w_str);
                *assigned = Value{&cache.insert(*stack[stack_top - 1].w_str)};
            } else {
                *assigned = stack[stack_top - 1];
            }
            break;
        }
        case is Instruction::EQUAL_SL: {
            Value val2 = stack[--stack_top];
            Value val1 = stack[stack_top - 1];
            bool result = val1 == val2;
            if (val1.tag == Value::Tag::STRING) {
                cache.remove(*val2.w_str);
                cache.remove(*val1.w_str);
            }
            if (val1.tag == Value::Tag::LIST) {
                destroy_list(val1.w_list);
            }
            if (val2.tag == Value::Tag::LIST) {
                destroy_list(val2.w_list);
            }
            stack[stack_top - 1] = Value{result};
            break;
        }
    }
    return ExecutionState::RUNNING;
}

const HashedString &VirtualMachine::store_string(std::string str) {
    return cache.insert(std::move(str));
}

#undef arith_binary_op
#undef comp_binary_op