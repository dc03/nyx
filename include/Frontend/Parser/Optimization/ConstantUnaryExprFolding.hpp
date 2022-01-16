#pragma once

/* Copyright (C) 2020-2022  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef CONSTANT_UNARY_EXPR_FOLDING_HPP
#define CONSTANT_UNARY_EXPR_FOLDING_HPP

#include "AST/AST.hpp"
#include "Frontend/Parser/Optimization/Utilities.hpp"

#include <functional>

template <typename U, typename F1, typename F2>
ExprNode generic_unary_operation(LiteralExpr &value, F1 check_value, F2 access_value, Type computed_type) {
    if (std::invoke(check_value, value.value)) {
        return ExprNode{allocate_node(LiteralExpr, LiteralValue{U{}(std::invoke(access_value, value.value))},
            TypeNode{allocate_node(PrimitiveType, computed_type, true, false)})};
    }
    return nullptr;
}

template <typename U>
ExprNode int_unary_operation(LiteralExpr &value, Type computed_type = Type::INT) {
    return generic_unary_operation<U>(value, &LiteralValue::is_int, &LiteralValue::to_int, computed_type);
}

template <typename U>
ExprNode float_unary_operation(LiteralExpr &value, Type computed_type = Type::FLOAT) {
    return generic_unary_operation<U>(value, &LiteralValue::is_float, &LiteralValue::to_float, computed_type);
}

template <typename U>
ExprNode string_unary_operation(LiteralExpr &value, Type computed_type = Type::STRING) {
    return generic_unary_operation<U>(value, &LiteralValue::is_string, &LiteralValue::to_string, computed_type);
}

template <typename U>
ExprNode boolean_unary_operation(LiteralExpr &value, Type computed_type = Type::BOOL) {
    return generic_unary_operation<U>(value, &LiteralValue::is_bool, &LiteralValue::to_bool, computed_type);
}

template <typename U>
ExprNode null_unary_operation(LiteralExpr &value, Type computed_type = Type::NULL_) {
    return generic_unary_operation<U>(value, &LiteralValue::is_null, &LiteralValue::to_null, computed_type);
}

#endif