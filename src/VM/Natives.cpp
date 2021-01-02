/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "Natives.hpp"

#include "../Common.hpp"
#include "../Module.hpp"

#include <iostream>
#include <string>

Value native_print(Value *args) {
    Value &arg = args[0];
    if (arg.is_int()) {
        std::cout << arg.to_int();
    } else if (arg.is_double()) {
        std::cout << arg.to_double();
    } else if (arg.is_bool()) {
        std::cout << (arg.to_bool() ? "true" : "false");
    } else if (arg.is_string()) {
        std::cout << arg.to_string();
    } else if (arg.is_function()) {
        std::cout << "<function '" << arg.to_function()->name << "' at " << arg.to_function();
    } else if (arg.is_ref()) {
        native_print(arg.to_referred());
    } else if (arg.is_null()) {
        std::cout << "null";
    }
    return Value{nullptr};
}

Value native_int(Value *args) {
    Value &arg = args[0];
    if (arg.is_numeric()) {
        return Value{static_cast<int>(args[0].to_numeric())};
    } else if (arg.is_string()) {
        return Value{std::stoi(arg.to_string())};
    } else if (arg.is_bool()) {
        return Value{static_cast<int>(arg.to_bool())};
    } else if (arg.is_ref()) {
        return native_int(arg.to_referred());
    }
    unreachable();
}

Value native_float(Value *args) {
    Value &arg = args[0];
    if (arg.is_numeric()) {
        return Value{args[0].to_numeric()};
    } else if (arg.is_string()) {
        return Value{std::stod(arg.to_string())};
    } else if (arg.is_bool()) {
        return Value{static_cast<double>(arg.to_bool())};
    } else if (arg.is_ref()) {
        return native_float(arg.to_referred());
    }
    unreachable();
}

Value native_string(Value *args) {
    Value &arg = args[0];
    if (arg.is_int()) {
        return Value{std::to_string(arg.to_int()).c_str()};
    } else if (arg.is_double()) {
        return Value{std::to_string(arg.to_double()).c_str()};
    } else if (arg.is_string()) {
        return Value{arg.to_string()};
    } else if (arg.is_bool()) {
        using namespace std::string_literals;
        return Value{arg.to_bool() ? "true" : "false"};
    } else if (arg.is_ref()) {
        return native_string(arg.to_referred());
    }
    unreachable();
}

Value native_readline(Value *args) {
    std::cout << args[0].to_string();
    std::string result{};
    std::getline(std::cin, result);
    return Value{result.c_str()};
}