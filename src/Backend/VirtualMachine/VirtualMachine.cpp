/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "Backend/VirtualMachine/VirtualMachine.hpp"

#include "Backend/VirtualMachine/Disassembler.hpp"
#include "Backend/VirtualMachine/Instructions.hpp"
#include "Backend/VirtualMachine/StringCacher.hpp"
#include "CLIConfigParser.hpp"
#include "ErrorLogger/ErrorLogger.hpp"

#include <cmath>
#include <iostream>
#include <termcolor/termcolor.hpp>

#define is (Chunk::InstructionSizeType)

VirtualMachine::VirtualMachine()
    : stack{std::make_unique<Value[]>(VirtualMachine::stack_size)},
      frames{std::make_unique<CallFrame[]>(VirtualMachine::frame_size)},
      modules{std::make_unique<ModuleFrame[]>(VirtualMachine::module_size)} {
    for (const auto &[name, wrapper] : native_wrappers.get_all_natives()) {
        natives[name] = wrapper->get_native();
    }
}

void VirtualMachine::set_runtime_ctx(RuntimeContext *ctx_) {
    ctx = ctx_;
}

void VirtualMachine::set_function_module_info(RuntimeModule *module, std::size_t index) {
    for (auto &[name, function] : module->functions) {
        function.module = module;
        function.module_index = index;
    }
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

void VirtualMachine::initialize_modules() {
    std::size_t i = 0;
    for (auto &module : ctx->compiled_modules) {
        modules[module_top++] = {&stack[stack_top], module.name};
        current_module = &module;
        current_chunk = &module.top_level_code;
        ip = &current_chunk->bytes[0];

        frames[frame_top++] =
            CallFrame{&stack[stack_top], nullptr, nullptr, current_module, i++, "<" + current_module->name + ":tlc>"};

        while (step() != ExecutionState::FINISHED)
            ;
    }
}

void VirtualMachine::teardown_modules() {
    while (module_top > 0) {
        current_module = &ctx->compiled_modules[module_top - 1];
        current_chunk = &current_module->teardown_code;
        ip = &current_chunk->bytes[0];

        while (step() != ExecutionState::FINISHED)
            ;

        frame_top--;
        module_top--;
    }
}

ColoredPrintHelper VirtualMachine::pcife(ColoredPrintHelper::StreamColorModifier colorizer) {
    return ColoredPrintHelper{colors_enabled, colorizer};
}

void VirtualMachine::run_function(RuntimeFunction &function) {
    std::size_t function_frame = frame_top;

    push(Value{nullptr});

    frames[frame_top++] = CallFrame{&stack[stack_top - (function.arity + 1)], current_chunk, ip, function.module,
        function.module_index, function.name};
    current_chunk = &function.code;
    ip = &function.code.bytes[0];

    while (frame_top > function_frame) {
        step();
    }

    pop();
}

void VirtualMachine::run(RuntimeModule &module) {
    initialize_modules();

    modules[module_top++] = {&stack[stack_top], module.name};
    current_module = &module;
    current_chunk = &module.top_level_code;
    ip = &current_chunk->bytes[0];

    frames[frame_top++] = CallFrame{
        &stack[stack_top], nullptr, nullptr, current_module, ctx->compiled_modules.size(), "<" + module.name + ":tlc>"};

    while (step() != ExecutionState::FINISHED)
        ;

    current_module = &module;
    current_chunk = &current_module->teardown_code;
    ip = &current_chunk->bytes[0];

    while (step() != ExecutionState::FINISHED)
        ;

    module_top--;
    teardown_modules();
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
    if (debug_print_stack) {
        std::cout << pcife(termcolor::green) << "Stack   : ";
        for (Value *begin{&stack[0]}; begin < &stack[stack_top]; begin++) {
            std::cout << pcife(termcolor::blue) << "[ " << pcife(termcolor::cyan) << begin->repr()
                      << pcife(termcolor::blue) << " ] ";
        }
    }
    if (debug_print_frames) {
        std::cout << pcife(termcolor::green) << "\nFrames  : ";
        for (CallFrame *begin{&frames[0]}; begin < &frames[frame_top]; begin++) {
            std::cout << pcife(termcolor::blue) << "[ " << pcife(termcolor::red) << begin->name
                      << pcife(termcolor::reset) << " : " << pcife(termcolor::cyan) << begin->stack
                      << pcife(termcolor::blue) << " ] ";
        }
    }
    if (debug_print_modules) {
        std::cout << pcife(termcolor::green) << "\nModules : ";
        for (ModuleFrame *begin{&modules[0]}; begin < &modules[module_top]; begin++) {
            std::cout << pcife(termcolor::blue) << "[ " << pcife(termcolor::red) << begin->name
                      << pcife(termcolor::reset) << " : " << pcife(termcolor::cyan) << begin->stack
                      << pcife(termcolor::blue) << " ] ";
        }
    }
    if (debug_print_stack || debug_print_frames || debug_print_modules) {
        std::cout << pcife(termcolor::reset) << '\n';
    }

    if (debug_print_instructions) {
        disassemble_instruction(
            *current_chunk, static_cast<Instruction>(*ip >> 24), (ip - &current_chunk->bytes[0]), colors_enabled);
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
                ctx->logger.runtime_error("Cannot modulo by zero", get_current_line());
                return ExecutionState::FINISHED;
            }
            arith_binary_op(%, IntType, w_int);
        }
        case is Instruction::IDIV: {
            if (stack[stack_top - 1].w_int == 0) {
                ctx->logger.runtime_error("Cannot divide by zero", get_current_line());
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
                ctx->logger.runtime_error("Cannot modulo by zero", get_current_line());
                return ExecutionState::FINISHED;
            }
            Value::FloatType val2 = stack[--stack_top].w_float;
            Value::FloatType val1 = stack[stack_top - 1].w_float;
            stack[stack_top - 1].w_float = std::fmod(val1, val2);
            break;
        }
        case is Instruction::FDIV: {
            if (stack[stack_top - 1].w_float == 0.0) {
                ctx->logger.runtime_error("Cannot divide by zero", get_current_line());
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
                ctx->logger.runtime_error("Cannot bitshift with value less than zero", get_current_line());
            }
            arith_binary_op(<<, IntType, w_int);
        }
        case is Instruction::SHIFT_RIGHT: {
            if (stack[stack_top - 1].w_int < 0) {
                ctx->logger.runtime_error("Cannot bitshift with value less than zero", get_current_line());
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
            Value *assigned = &frames[frame_top - 1].stack[operand];
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
            push(frames[frame_top - 1].stack[operand]);
            if (stack[stack_top - 1].tag == Value::Tag::STRING) {
                (void)cache.insert(*stack[stack_top - 1].w_str);
            }
            break;
        }
        case is Instruction::MAKE_REF_TO_LOCAL: {
            Value &value = frames[frame_top - 1].stack[operand];
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
            Value *assigned = &modules[frames[frame_top - 1].module_index].stack[operand];
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
            push(Value{modules[frames[frame_top - 1].module_index].stack[operand]});
            if (stack[stack_top - 1].tag == Value::Tag::STRING) {
                (void)cache.insert(*stack[stack_top - 1].w_str);
            }
            break;
        }
        case is Instruction::MAKE_REF_TO_GLOBAL: {
            Value &value = modules[frames[frame_top - 1].module_index].stack[operand];
            if (value.tag == Value::Tag::LIST) {
                push(Value{value.w_list});
                stack[stack_top - 1].tag = Value::Tag::LIST_REF;
            } else {
                push(Value{&value});
            }
            break;
        }
        /* Function calls */
        case is Instruction::LOAD_FUNCTION_SAME_MODULE: {
            RuntimeFunction *function = &frames[frame_top - 1].module->functions[stack[stack_top - 1].w_str->str];
            stack[stack_top - 1].w_fun = function;
            stack[stack_top - 1].tag = Value::Tag::FUNCTION;
            break;
        }
        case is Instruction::LOAD_FUNCTION_MODULE_INDEX: {
            RuntimeFunction *function = &ctx->compiled_modules[operand].functions[stack[stack_top - 1].w_str->str];
            stack[stack_top - 1].w_fun = function;
            stack[stack_top - 1].tag = Value::Tag::FUNCTION;
            break;
        }
        case is Instruction::LOAD_FUNCTION_MODULE_PATH: {
            Value::StringType path = stack[--stack_top].w_str;
            RuntimeFunction *function = &ctx->get_module_string(path->str)->functions[stack[stack_top - 1].w_str->str];
            cache.remove(*path);
            stack[stack_top - 1].w_fun = function;
            stack[stack_top - 1].tag = Value::Tag::FUNCTION;
            break;
        }
        case is Instruction::CALL_FUNCTION: {
            RuntimeFunction *called = stack[--stack_top].w_fun;
            frames[frame_top++] = CallFrame{&stack[stack_top - (called->arity + 1)], current_chunk, ip, called->module,
                called->module_index, called->name};
            current_chunk = &called->code;
            ip = &called->code.bytes[0];
            break;
        }
        case is Instruction::CALL_NATIVE: {
            Native called = natives[stack[--stack_top].w_str->str];
            cache.remove(*stack[stack_top].w_str);
            Value result = called.code(*this, &stack[stack_top] - called.arity);
            stack[stack_top - called.arity - 1] = result;
            break;
        }
        case is Instruction::RETURN: {
            ip = frames[frame_top - 1].return_ip;
            current_chunk = frames[--frame_top].return_chunk;
            break;
        }
        case is Instruction::TRAP_RETURN: {
            ctx->logger.runtime_error("Reached end of non-null function", get_current_line());
            return ExecutionState::FINISHED;
        }
        /* String instructions */
        case is Instruction::CONSTANT_STRING: {
            Value::StringType string = current_chunk->constants[operand].w_str;
            push(Value{&cache.insert(*string)});
            break;
        }
        case is Instruction::INDEX_STRING: {
            Value &index = stack[--stack_top];
            Value *string = &stack[stack_top - 1];
            if (string->tag == Value::Tag::REF) {
                string = string->w_ref;
            }
            Value temp = stack[stack_top - 1];
            stack[stack_top - 1] = Value{&cache.insert({(string->w_str->str)[index.w_int]})};
            if (temp.tag == Value::Tag::STRING) {
                cache.remove(*temp.w_str);
            }
            break;
        }
        case is Instruction::CHECK_STRING_INDEX: {
            Value &index = stack[stack_top - 1];
            Value *string = &stack[stack_top - 2];
            if (string->tag == Value::Tag::REF) {
                string = string->w_ref;
            }
            if (index.w_int > static_cast<int>(string->w_str->str.size())) {
                ctx->logger.runtime_error("String index out of range", get_current_line());
                return ExecutionState::FINISHED;
            }
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
            push(Value{make_new_list()});
            if (operand != 0) {
                stack[stack_top - 1].w_list->resize(operand);
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
            Value &list = stack[stack_top - 1];
            list.w_list->push_back(appended);
            break;
        }
        case is Instruction::POP_FROM_LIST: {
            Value &how_many = stack[--stack_top];
            Value &list = stack[stack_top - 1];
            if (static_cast<Value::IntType>(list.w_list->size()) < how_many.w_int) {
                ctx->logger.runtime_error("Trying to pop from empty list", get_current_line());
                return ExecutionState::FINISHED;
            }
            for (Value::IntType i = 0; i < how_many.w_int; i++) {
                if (list.w_list->back().tag == Value::Tag::LIST) {
                    destroy_list(list.w_list->back().w_list);
                } else if (list.w_list->back().tag == Value::Tag::STRING) {
                    cache.remove(*list.w_list->back().w_str);
                }
                list.w_list->pop_back();
            }
            break;
        }
        case is Instruction::ASSIGN_LIST: {
            Value &assigned = stack[--stack_top];
            Value &index = stack[--stack_top];
            Value &list = stack[stack_top - 1];
            Value::Tag tag = (*list.w_list)[index.w_int].tag;
            if (tag == Value::Tag::LIST) {
                destroy_list((*list.w_list)[index.w_int].w_list);
            } else if (tag == Value::Tag::STRING) {
                cache.remove(*(*list.w_list)[index.w_int].w_str);
                (void)cache.insert(*assigned.w_str);
            }

            if (tag == Value::Tag::REF) {
                *(*list.w_list)[index.w_int].w_ref = assigned;
            } else {
                (*list.w_list)[index.w_int] = assigned;
            }
            stack[stack_top - 1] = (*list.w_list)[index.w_int];
            if (tag == Value::Tag::LIST) {
                stack[stack_top - 1].tag = Value::Tag::LIST_REF;
            }
            break;
        }
        case is Instruction::INDEX_LIST: {
            Value &index = stack[--stack_top];
            Value &list = stack[stack_top - 1];
            stack[stack_top - 1] = (*list.w_list)[index.w_int];
            if (stack[stack_top - 1].tag == Value::Tag::STRING) {
                (void)cache.insert(*stack[stack_top - 1].w_str);
            } else if (stack[stack_top - 1].tag == Value::Tag::LIST) {
                stack[stack_top - 1].tag = Value::Tag::LIST_REF;
            }
            break;
        }
        case is Instruction::MAKE_REF_TO_INDEX: {
            Value &index = stack[--stack_top];
            Value &list = stack[stack_top - 1];
            if ((*list.w_list)[index.w_int].tag == Value::Tag::LIST) {
                stack[stack_top - 1] = (*list.w_list)[index.w_int];
                stack[stack_top - 1].tag = Value::Tag::LIST_REF;
            } else {
                stack[stack_top - 1] = Value{&(*list.w_list)[index.w_int]};
            }
            break;
        }
        case is Instruction::CHECK_LIST_INDEX: {
            Value &index = stack[stack_top - 1];
            Value &list = stack[stack_top - 2];
            if (index.w_int > static_cast<int>(list.w_list->size())) {
                ctx->logger.runtime_error("List index out of range", get_current_line());
                return ExecutionState::FINISHED;
            }
            break;
        }
        case is Instruction::ACCESS_LOCAL_LIST: {
            push(frames[frame_top - 1].stack[operand]);
            stack[stack_top - 1].tag = Value::Tag::LIST_REF;
            break;
        }
        case is Instruction::ACCESS_GLOBAL_LIST: {
            push(modules[frames[frame_top - 1].module_index].stack[operand]);
            stack[stack_top - 1].tag = Value::Tag::LIST_REF;
            break;
        }
        case is Instruction::ASSIGN_LOCAL_LIST: {
            Value &assigned = frames[frame_top - 1].stack[operand];
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
            Value &assigned = modules[frames[frame_top - 1].module_index].stack[operand];
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
            } else if (stack[stack_top - 1].tag == Value::Tag::LIST_REF ||
                       stack[stack_top - 1].tag == Value::Tag::NULL_) {
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
        /* Move instructions */
        case is Instruction::MOVE_LOCAL: {
            Value &moved = frames[frame_top - 1].stack[operand];
            stack[stack_top] = moved;
            stack[stack_top++].tag = Value::Tag::LIST;
            moved = Value{Value::NullType{}};
            break;
        }
        case is Instruction::MOVE_GLOBAL: {
            Value &moved = modules[frames[frame_top - 1].module_index].stack[operand];
            stack[stack_top++] = moved;
            moved = Value{Value::NullType{}};
            break;
        }
        case is Instruction::MOVE_INDEX: {
            Value::IntType index = stack[--stack_top].w_int;
            Value::ListType &list = *stack[stack_top - 1].w_list;
            stack[stack_top - 1] = list[index];
            list[index] = Value{Value::NullType{}};
            break;
        }
        /* Swap instructions */
        case is Instruction::SWAP: {
            Value first = stack[stack_top - operand];
            Value second = stack[stack_top - operand - 1];
            stack[stack_top - operand - 1] = first;
            stack[stack_top - operand] = second;
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