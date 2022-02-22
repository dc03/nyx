#pragma once

/* Copyright (C) 2020-2022  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef CONSTANT_BINARY_EXPR_FOLDING_HPP
#define CONSTANT_BINARY_EXPR_FOLDING_HPP

#include "Utilities.hpp"
#include "nyx/AST/AST.hpp"

#include <functional>

template <typename U, typename F1, typename F2>
ExprNode generic_binary_operation(LiteralExpr &left, LiteralExpr &right, F1 check_left, F1 check_right, F2 access_left,
    F2 access_right, Type computed_type) {
    if (std::invoke(check_left, left.value) && std::invoke(check_right, right.value)) {
        return ExprNode{allocate_node(LiteralExpr,
            LiteralValue{U{}(std::invoke(access_left, left.value), std::invoke(access_right, right.value))},
            TypeNode{allocate_node(PrimitiveType, computed_type, true, false)})};
    }

    return nullptr;
}

template <typename U, typename F1, typename F2>
ExprNode generic_binary_operation_same_pmf(
    LiteralExpr &left, LiteralExpr &right, F1 check, F2 access, Type computed_type) {
    return generic_binary_operation<U>(left, right, check, check, access, access, computed_type);
}

template <typename U>
ExprNode int_binary_operation(LiteralExpr &left, LiteralExpr &right, Type computed_type = Type::INT) {
    return generic_binary_operation_same_pmf<U>(
        left, right, &LiteralValue::is_int, &LiteralValue::to_int, computed_type);
}

template <typename U>
ExprNode numeric_binary_operation(LiteralExpr &left, LiteralExpr &right, Type computed_type = Type::FLOAT) {
    return generic_binary_operation_same_pmf<U>(
        left, right, &LiteralValue::is_numeric, &LiteralValue::to_numeric, computed_type);
}

template <typename U>
ExprNode boolean_binary_operation(LiteralExpr &left, LiteralExpr &right, Type computed_type = Type::BOOL) {
    return generic_binary_operation_same_pmf<U>(
        left, right, &LiteralValue::is_bool, &LiteralValue::to_bool, computed_type);
}

template <typename U>
ExprNode string_binary_operation(LiteralExpr &left, LiteralExpr &right, Type computed_type = Type::STRING) {
    return generic_binary_operation_same_pmf<U>(
        left, right, &LiteralValue::is_string, &LiteralValue::to_string, computed_type);
}

template <typename U>
ExprNode null_binary_operation(LiteralExpr &left, LiteralExpr &right, Type computed_type = Type::BOOL) {
    return generic_binary_operation_same_pmf<U>(
        left, right, &LiteralValue::is_null, &LiteralValue::to_null, computed_type);
}

template <typename U>
ExprNode comparison_operation(LiteralExpr &left, LiteralExpr &right) {
    return first_not_null(int_binary_operation<U>(left, right, Type::BOOL),
        numeric_binary_operation<U>(left, right, Type::BOOL), boolean_binary_operation<U>(left, right, Type::BOOL));
}

#endif