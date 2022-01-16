#pragma once

/* Copyright (C) 2020-2022  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef CONSTANT_TERNARY_EXPR_FOLDING_HPP
#define CONSTANT_TERNARY_EXPR_FOLDING_HPP

#include "AST/AST.hpp"
#include "Utilities.hpp"

#define TERNARY_PARAMETERS LiteralExpr &cond, LiteralExpr &middle, LiteralExpr &right
#define TERNARY_ARGS       cond, middle, right

bool check_literal(LiteralValue &value) {
    if (value.is_int()) {
        return value.to_int() != 0;
    } else if (value.is_double()) {
        return value.to_double() != 0.0;
    } else if (value.is_string()) {
        return not value.to_string().empty();
    } else if (value.is_bool()) {
        return value.to_bool();
    } else if (value.is_null()) {
        return false;
    }
    return false;
}

template <typename F1>
ExprNode generic_conditional_operation(TERNARY_PARAMETERS, F1 check_middle, F1 check_right, Type computed_type) {
    if (std::invoke(check_middle, middle.value) && std::invoke(check_right, right.value)) {
        auto value = check_literal(cond.value) ? middle.value : right.value;
        return ExprNode{allocate_node(
            LiteralExpr, std::move(value), TypeNode{allocate_node(PrimitiveType, computed_type, true, false)})};
    }

    return nullptr;
}

template <typename F1>
ExprNode generic_conditional_operation_same_pmf(TERNARY_PARAMETERS, F1 check, Type computed_type) {
    return generic_conditional_operation(TERNARY_ARGS, check, check, computed_type);
}

ExprNode int_conditional_operation(TERNARY_PARAMETERS) {
    return generic_conditional_operation_same_pmf(TERNARY_ARGS, &LiteralValue::is_int, Type::INT);
}

ExprNode float_conditional_operation(TERNARY_PARAMETERS) {
    return generic_conditional_operation_same_pmf(TERNARY_ARGS, &LiteralValue::is_double, Type::FLOAT);
}

ExprNode string_conditional_oepration(TERNARY_PARAMETERS) {
    return generic_conditional_operation_same_pmf(TERNARY_ARGS, &LiteralValue::is_string, Type::STRING);
}

ExprNode bool_conditional_operation(TERNARY_PARAMETERS) {
    return generic_conditional_operation_same_pmf(TERNARY_ARGS, &LiteralValue::is_bool, Type::BOOL);
}

ExprNode null_conditional_operation(TERNARY_PARAMETERS) {
    return generic_conditional_operation_same_pmf(TERNARY_ARGS, &LiteralValue::is_null, Type::NULL_);
}

ExprNode conditional_operation(TERNARY_PARAMETERS) {
    return first_not_null(int_conditional_operation(TERNARY_ARGS), float_conditional_operation(TERNARY_ARGS),
        string_conditional_oepration(TERNARY_ARGS), bool_conditional_operation(TERNARY_ARGS),
        null_conditional_operation(TERNARY_ARGS));
}

#endif