/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "Natives.hpp"

#include "../Common.hpp"
#include "Module.hpp"

#include <iostream>
#include <string>

// clang-format off
std::vector<NativeFn> native_functions{
    {native_print,    "print",    Type::NULL_,  {{Type::INT, Type::FLOAT, Type::STRING, Type::BOOL, Type::FUNCTION, Type::NULL_, Type::LIST}}, 1},
    {native_int,      "int",      Type::INT,    {{Type::INT, Type::FLOAT, Type::STRING, Type::BOOL}}, 1},
    {native_float,    "float",    Type::FLOAT,  {{Type::INT, Type::FLOAT, Type::STRING, Type::BOOL}}, 1},
    {native_string,   "string",   Type::STRING, {{Type::INT, Type::FLOAT, Type::STRING, Type::BOOL, Type::LIST}}, 1},
    {native_readline, "readline", Type::STRING, {{Type::STRING}}, 1},
    {native_size,     "size",     Type::INT,    {{Type::LIST, Type::STRING}}, 1}
};
// clang-format on

Value native_print(Value *args) {
    Value &arg = args[0];
    if (arg.tag == Value::Tag::INT) {
        std::cout << arg.w_int;
    } else if (arg.tag == Value::Tag::FLOAT) {
        std::cout << arg.w_float;
    } else if (arg.tag == Value::Tag::BOOL) {
        std::cout << (arg.w_bool ? "true" : "false");
    } else if (arg.tag == Value::Tag::STRING) {
        std::cout << arg.w_str;
    } else if (arg.tag == Value::Tag::REF) {
        native_print(arg.w_ref);
    } else if (arg.tag == Value::Tag::INVALID) {
        std::cout << "<invalid!>";
    }
    return Value{nullptr};
}

Value native_int(Value *args) {
    Value &arg = args[0];
    if (arg.tag == Value::Tag::INT) {
        return arg;
    } else if (arg.tag == Value::Tag::FLOAT) {
        return Value{static_cast<int>(arg.w_float)};
    } else if (arg.tag == Value::Tag::STRING) {
        return Value{std::stoi(arg.w_str)};
    } else if (arg.tag == Value::Tag::BOOL) {
        return Value{static_cast<int>(arg.w_bool)};
    } else if (arg.tag == Value::Tag::REF) {
        return native_int(arg.w_ref);
    } else if (arg.tag == Value::Tag::INVALID) {
        return Value{0};
    }
    unreachable();
}

Value native_float(Value *args) {
    Value &arg = args[0];
    if (arg.tag == Value::Tag::INT) {
        return Value{static_cast<float>(arg.w_int)};
    } else if (arg.tag == Value::Tag::FLOAT) {
        return arg;
    } else if (arg.tag == Value::Tag::STRING) {
        return Value{std::stod(arg.w_str)};
    } else if (arg.tag == Value::Tag::BOOL) {
        return Value{static_cast<float>(arg.w_bool)};
    } else if (arg.tag == Value::Tag::REF) {
        return native_int(arg.w_ref);
    }
    unreachable();
}

Value native_string(Value *args) {
    Value &arg = args[0];
    //    if (arg.is_int()) {
    //        return Value{std::to_string(arg.to_int())};
    //    } else if (arg.is_double()) {
    //        return Value{std::to_string(arg.to_double())};
    //    } else if (arg.is_string()) {
    //        return Value{arg.to_string()};
    //    } else if (arg.is_bool()) {
    //        using namespace std::string_literals;
    //        return Value{arg.to_bool() ? "true"s : "false"s};
    //    } else if (arg.is_ref()) {
    //        return native_string(arg.to_referred());
    //    } else if (arg.is_list()) {
    //        return Value{arg.repr()};
    //    }
    unreachable();
}

Value native_readline(Value *args) {
    Value &prompt = args[0];
    if (prompt.tag == Value::Tag::STRING) {
        std::cout << prompt.w_ref->w_str;
    } else {
        std::cout << prompt.w_str;
    }
    std::string result{};
    std::getline(std::cin, result);
    // return Value{result};
    unreachable();
}

Value native_size(Value *args) {
    Value &arg = args[0];
    //    if (arg.is_list()) {
    //        return Value{static_cast<int>(arg.to_list().size())};
    //    } else if (arg.is_string()) {
    //        return Value{static_cast<int>(arg.to_string().size())};
    //    } else if (arg.is_ref()) {
    //        return native_size(arg.to_referred());
    //    }
    unreachable();
}