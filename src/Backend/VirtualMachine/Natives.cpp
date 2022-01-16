/* Copyright (C) 2020-2022  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "Backend/VirtualMachine/Natives.hpp"

#include "Backend/RuntimeModule.hpp"
#include "Backend/VirtualMachine/VirtualMachine.hpp"
#include "Common.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <string>

Value native_print(VirtualMachine &vm, Value *args);
Value native_int(VirtualMachine &vm, Value *args);
Value native_float(VirtualMachine &vm, Value *args);
Value native_string(VirtualMachine &vm, Value *args);
Value native_readline(VirtualMachine &vm, Value *args);
Value native_size(VirtualMachine &vm, Value *args);
Value native_fill_trivial(VirtualMachine &vm, Value *args);
Value native_resize_list_trivial(VirtualMachine &vm, Value *args);

NativeWrappers native_wrappers{};

template <typename T, typename... Args>
bool is_in(T value, Args &&...args) {
    std::array arr{args...};
    return std::find(arr.begin(), arr.end(), value) != arr.end();
}

NativeWrapper::NativeWrapper(NativeFunctionType native, std::string name, TypeNode return_type, std::size_t arity,
    ArgumentVerifierType argument_verifier)
    : native{native},
      name{std::move(name)},
      return_type{std::move(return_type)},
      arity{arity},
      argument_verifier{argument_verifier} {
    native_wrappers.add_native(*this);
}

Native NativeWrapper::get_native() const noexcept {
    return {native, get_name(), get_arity()};
}

const std::string &NativeWrapper::get_name() const noexcept {
    return name;
}

std::size_t NativeWrapper::get_arity() const noexcept {
    return arity;
}

bool NativeWrapper::check_arity(std::size_t num_args) const noexcept {
    return num_args == arity;
}

std::pair<bool, std::string_view> NativeWrapper::check_arguments(
    std::vector<CallExpr::ArgumentType> &arguments) const noexcept {
    return argument_verifier(arguments);
}

const TypeNode &NativeWrapper::get_return_type() const noexcept {
    return return_type;
}

void NativeWrappers::add_native(NativeWrapper &native) {
    native_functions[native.get_name()] = &native;
}

bool NativeWrappers::is_native(std::string_view function) const noexcept {
    return native_functions.find(function) != native_functions.end();
}

const NativeWrapper *NativeWrappers::get_native(std::string_view function) const noexcept {
    if (is_native(function)) {
        return native_functions.find(function)->second;
    } else {
        return nullptr;
    }
}

const NativeWrappers::NativeCollectionType &NativeWrappers::get_all_natives() const noexcept {
    return native_functions;
}

// clang-format off
NativeWrapper print{
    native_print,
    "print",
    TypeNode{allocate_node(PrimitiveType, Type::NULL_, false, false)},
    1,
    NATIVE_ARGUMENT_CHECKER_DEFINITION {
        if (arguments.size() != 1) {
            return {false, "arity incorrect, should be 1"};
        }

        if (not is_in(NATIVE_ARGN_PRIMITIVE(0), Type::INT, Type::FLOAT,
                Type::STRING, Type::BOOL, Type::FUNCTION, Type::NULL_, Type::LIST, Type::TUPLE)) {
            return {false, "incorrect argument type"};
        }

        return {true, ""};
    }
};

NativeWrapper int_{
    native_int,
    "int",
    TypeNode{allocate_node(PrimitiveType, Type::INT, false, false)},
    1,
    NATIVE_ARGUMENT_CHECKER_DEFINITION {
        if (arguments.size() != 1) {
            return {false, "arity incorrect, should be 1"};
        }

        if (not is_in(NATIVE_ARGN_PRIMITIVE(0), Type::INT, Type::FLOAT,
            Type::STRING, Type::BOOL)) {
            return {false, "incorrect argument type"};
        }

        return {true, ""};
    }
};

NativeWrapper float_{
    native_float,
    "float",
    TypeNode{allocate_node(PrimitiveType, Type::FLOAT, false, false)},
    1,
    NATIVE_ARGUMENT_CHECKER_DEFINITION {
        if (arguments.size() != 1) {
            return {false, "arity incorrect, should be 1"};
        }

        if (not is_in(NATIVE_ARGN_PRIMITIVE(0), Type::INT, Type::FLOAT,
            Type::STRING, Type::BOOL)) {
            return {false, "incorrect argument type"};
        }

        return {true, ""};
    }
};

NativeWrapper string_{
    native_string,
    "string",
    TypeNode{allocate_node(PrimitiveType, Type::STRING, false, false)},
    1,
    NATIVE_ARGUMENT_CHECKER_DEFINITION {
        if (arguments.size() != 1) {
            return {false, "arity incorrect, should be 1"};
        }

        if (not is_in(NATIVE_ARGN_PRIMITIVE(0), Type::INT, Type::FLOAT,
            Type::STRING, Type::BOOL, Type::LIST)) {
            return {false, "incorrect argument type"};
        }

        return {true, ""};
    }
};

NativeWrapper readline{
    native_readline,
    "readline",
    TypeNode{allocate_node(PrimitiveType, Type::STRING, false, false)},
    1,
    NATIVE_ARGUMENT_CHECKER_DEFINITION {
       if (arguments.size() != 1) {
            return {false, "arity incorrect, should be 1"};
        }

        if (not is_in(NATIVE_ARGN_PRIMITIVE(0), Type::STRING)) {
            return {false, "incorrect argument type, can only pass string as prompt"};
        }

        return {true, ""};
    }
};

NativeWrapper size_{
    native_size,
    "size",
    TypeNode{allocate_node(PrimitiveType, Type::INT, false, false)},
    1,
    NATIVE_ARGUMENT_CHECKER_DEFINITION {
        if (arguments.size() != 1) {
            return {false, "arity incorrect, should be 1"};
        }

        if (not is_in(NATIVE_ARGN_PRIMITIVE(0), Type::LIST, Type::STRING, Type::TUPLE)) {
            return {false, "incorrect argument type, can only be list, string or tuple"};
        }

        return {true, ""};
    }
};

NativeWrapper fill_trivial {
    native_fill_trivial,
    "fill_trivial",
    TypeNode{allocate_node(PrimitiveType, Type::NULL_, false, false)},
    2,
    NATIVE_ARGUMENT_CHECKER_DEFINITION {
        if (arguments.size() != 2) {
            return {false, "arity incorrect, should be 2"};
        }

        auto *list = NATIVE_ARGN_TYPE(0);
        auto *value = NATIVE_ARGN_TYPE(1);

        if (list->primitive != Type::LIST) {
            return {false, "type of the first argument has to be a list type"};
        }

        auto *list_type = dynamic_cast<ListType*>(list);
        if (list_type->contained->is_ref) {
            return {false, "cannot fill list of references"};
        } else if (is_nontrivial_type(list_type->contained->primitive) || is_nontrivial_type(value->primitive)) {
            return {false, "cannot call function with arguments having non-trivial types"};
        } else if (list_type->contained->primitive != value->primitive) {
            return {false, "type of value must match contained type of list"};
        }

        return {true, ""};
    }
};

NativeWrapper resize_list_trivial {
    native_resize_list_trivial,
    "%resize_list_trivial",
    TypeNode{allocate_node(PrimitiveType, Type::NULL_, false, false)},
    2,
    NATIVE_ARGUMENT_CHECKER_DEFINITION {
        // Note that we don't need pretty type checking here because this function is not user-callable yet
        assert(NATIVE_ARGN_PRIMITIVE(0) == Type::LIST && "Expect list type");
        assert(NATIVE_ARGN_PRIMITIVE(1) == Type::INT && "Expect int type");
        return {true, ""};
    }
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

Value native_fill_trivial(VirtualMachine &vm, Value *args) {
    Value &list = args[0];
    Value *value = &args[1];

    if (value->tag == Value::Tag::REF) {
        value = value->w_ref;
    }

    if (value->tag == Value::Tag::STRING) {
        std::for_each(list.w_list->begin(), list.w_list->end(), [&vm](const Value &v) { vm.remove_string(v.w_str); });
        for (auto &e : *list.w_list) {
            e = Value{&vm.store_string(value->w_str->str)};
        }
    } else {
        std::fill(list.w_list->begin(), list.w_list->end(), *value);
    }
    return Value{nullptr};
}

Value native_resize_list_trivial(VirtualMachine &vm, Value *args) {
    Value &list = args[0];
    Value *size = &args[1];

    if (size->tag == Value::Tag::REF) {
        size = size->w_ref;
    }

    if (not list.w_list->empty() && (*list.w_list)[0].tag == Value::Tag::STRING) {
        for (auto i = static_cast<std::size_t>(size->w_int); i < list.w_list->size(); i++) {
            vm.remove_string((*list.w_list)[i].w_str);
        }
    }
    list.w_list->resize(size->w_int);
    return Value{nullptr};
}
