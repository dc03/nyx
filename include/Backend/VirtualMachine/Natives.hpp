#pragma once

/* Copyright (C) 2020-2022  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef NATIVES_HPP
#define NATIVES_HPP

#include "AST/VisitorTypes.hpp"
#include "Value.hpp"

#include <string>
#include <vector>

class VirtualMachine;

using NativeFunctionType = Value (*)(VirtualMachine &vm, Value *args);

struct Native {
    NativeFunctionType code;
    std::string name{};
    std::size_t arity;
};

#define NATIVE_ARGUMENT_CHECKER_DEFINITION                                                                             \
    [](std::vector<CallExpr::ArgumentType> & arguments) -> std::pair<bool, std::string_view>
#define NATIVE_ARGN_PRIMITIVE(n) std::get<ExprNode>(arguments[n])->synthesized_attrs.info->primitive
#define NATIVE_ARGN_TYPE(n)      std::get<ExprNode>(arguments[n])->synthesized_attrs.info

class NativeWrapper {
  public:
    using ArgumentVerifierType = std::pair<bool, std::string_view> (*)(std::vector<CallExpr::ArgumentType> &arguments);

  private:
    NativeFunctionType native{};
    std::string name{};
    TypeNode return_type{};
    std::size_t arity{};
    ArgumentVerifierType argument_verifier{};

  public:
    NativeWrapper(NativeFunctionType native, std::string name, TypeNode return_type, std::size_t arity,
        ArgumentVerifierType argument_verifier);

    [[nodiscard]] Native get_native() const noexcept;
    [[nodiscard]] const std::string &get_name() const noexcept;
    [[nodiscard]] std::size_t get_arity() const noexcept;
    [[nodiscard]] bool check_arity(std::size_t num_args) const noexcept;
    [[nodiscard]] std::pair<bool, std::string_view> check_arguments(
        std::vector<CallExpr::ArgumentType> &arguments) const noexcept;
    [[nodiscard]] const TypeNode &get_return_type() const noexcept;
};

class NativeWrappers {
  public:
    using NativeCollectionType = std::unordered_map<std::string_view, NativeWrapper *>;

  private:
    NativeCollectionType native_functions{};

  public:
    void add_native(NativeWrapper &native);
    [[nodiscard]] bool is_native(std::string_view function) const noexcept;
    [[nodiscard]] const NativeWrapper *get_native(std::string_view function) const noexcept;
    [[nodiscard]] const NativeCollectionType &get_all_natives() const noexcept;
};

extern NativeWrappers native_wrappers;

#endif