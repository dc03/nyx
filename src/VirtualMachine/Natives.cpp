/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "Natives.hpp"

#include "../Common.hpp"
#include "Module.hpp"
#include "VirtualMachine.hpp"

#include <iostream>
#include <string>

// clang-format off
std::vector<NativeFn> native_functions{
    {native_print,    "print",    Type::NULL_,  {{Type::INT, Type::FLOAT, Type::STRING, Type::BOOL, Type::FUNCTION, Type::NULL_, Type::LIST, Type::TUPLE}}, 1},
    {native_int,      "int",      Type::INT,    {{Type::INT, Type::FLOAT, Type::STRING, Type::BOOL}}, 1},
    {native_float,    "float",    Type::FLOAT,  {{Type::INT, Type::FLOAT, Type::STRING, Type::BOOL}}, 1},
    {native_string,   "string",   Type::STRING, {{Type::INT, Type::FLOAT, Type::STRING, Type::BOOL, Type::LIST}}, 1},
    {native_readline, "readline", Type::STRING, {{Type::STRING}}, 1},
    {native_size,     "size",     Type::INT,    {{Type::LIST, Type::STRING, Type::TUPLE}}, 1}
};
// clang-format on

Value native_print(VirtualMachine &vm, Value *args) {
    Value &arg = args[0];
    if (arg.tag == Value::Tag::INT) {
        std::cout << arg.w_int;
    } else if (arg.tag == Value::Tag::FLOAT) {
        std::cout << arg.w_float;
    } else if (arg.tag == Value::Tag::BOOL) {
        std::cout << (arg.w_bool ? "true" : "false");
    } else if (arg.tag == Value::Tag::STRING) {
        std::cout << arg.w_str->str;
    } else if (arg.tag == Value::Tag::REF) {
        native_print(vm, arg.w_ref);
    } else if (arg.tag == Value::Tag::LIST || arg.tag == Value::Tag::LIST_REF) {
        if (arg.w_list == nullptr || arg.w_list->empty()) {
            std::cout << "[]";
        } else {
            std::cout << "[";
            auto begin = arg.w_list->begin();
            for (; begin != arg.w_list->end() - 1; begin++) {
                native_print(vm, &*begin);
                std::cout << ", ";
            }
            native_print(vm, &*begin);
            std::cout << "]";
        }
    } else if (arg.tag == Value::Tag::INVALID) {
        std::cout << "<invalid!>";
    }
    return Value{nullptr};
}

Value native_int(VirtualMachine &vm, Value *args) {
    Value &arg = args[0];
    if (arg.tag == Value::Tag::INT) {
        return arg;
    } else if (arg.tag == Value::Tag::FLOAT) {
        return Value{static_cast<int>(arg.w_float)};
    } else if (arg.tag == Value::Tag::STRING) {
        return Value{std::stoi(arg.w_str->str)};
    } else if (arg.tag == Value::Tag::BOOL) {
        return Value{static_cast<int>(arg.w_bool)};
    } else if (arg.tag == Value::Tag::REF) {
        return native_int(vm, arg.w_ref);
    } else if (arg.tag == Value::Tag::INVALID) {
        return Value{0};
    }
    unreachable();
}

Value native_float(VirtualMachine &vm, Value *args) {
    Value &arg = args[0];
    if (arg.tag == Value::Tag::INT) {
        return Value{static_cast<float>(arg.w_int)};
    } else if (arg.tag == Value::Tag::FLOAT) {
        return arg;
    } else if (arg.tag == Value::Tag::STRING) {
        return Value{std::stod(arg.w_str->str)};
    } else if (arg.tag == Value::Tag::BOOL) {
        return Value{static_cast<float>(arg.w_bool)};
    } else if (arg.tag == Value::Tag::REF) {
        return native_int(vm, arg.w_ref);
    }
    unreachable();
}

Value native_string(VirtualMachine &vm, Value *args) {
    Value &arg = args[0];
    if (arg.tag == Value::Tag::INT) {
        return Value{&vm.store_string(std::to_string(arg.w_int))};
    } else if (arg.tag == Value::Tag::FLOAT) {
        return Value{&vm.store_string(std::to_string(arg.w_float))};
    } else if (arg.tag == Value::Tag::STRING) {
        return arg;
    } else if (arg.tag == Value::Tag::BOOL) {
        return Value{&vm.store_string(arg.w_bool ? "true" : "false")};
    } else if (arg.tag == Value::Tag::REF) {
        return native_string(vm, arg.w_ref);
    } else if (arg.tag == Value::Tag::LIST || arg.tag == Value::Tag::LIST_REF) {
        return Value{&vm.store_string(arg.repr())};
    } else if (arg.tag == Value::Tag::INVALID) {
        return Value{&vm.store_string("invalid")};
    }
    unreachable();
}

Value native_readline(VirtualMachine &vm, Value *args) {
    Value &prompt = args[0];
    if (prompt.tag == Value::Tag::REF) {
        std::cout << prompt.w_ref->w_str->str;
    } else {
        std::cout << prompt.w_str->str;
    }
    std::string result{};
    std::getline(std::cin, result);
    return Value{&vm.store_string(std::move(result))};
    unreachable();
}

Value native_size(VirtualMachine &vm, Value *args) {
    Value &arg = args[0];
    if (arg.tag == Value::Tag::STRING) {
        return Value{static_cast<Value::IntType>(arg.w_str->str.length())};
    } else if (arg.tag == Value::Tag::LIST || arg.tag == Value::Tag::LIST_REF) {
        return Value{static_cast<Value::IntType>(arg.w_list->size())};
    } else if (arg.tag == Value::Tag::REF) {
        return native_string(vm, arg.w_ref);
    }
    unreachable();
}