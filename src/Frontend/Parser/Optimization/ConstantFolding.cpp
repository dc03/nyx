/* Copyright (C) 2020-2022  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "Common.hpp"
#include "Frontend/Parser/Optimization/ConstantBinaryExprFolding.hpp"
#include "Frontend/Parser/Optimization/ConstantTernaryExprFolding.hpp"
#include "Frontend/Parser/Optimization/ConstantUnaryExprFolding.hpp"
#include "Frontend/Parser/Parser.hpp"

struct right_shift {
    template <typename T, typename U>
    constexpr auto operator()(T &&left, U &&right) const -> decltype(std::forward<T>(left) >> std::forward<U>(right)) {
        return left >> right;
    }
};

struct left_shift {
    template <typename T, typename U>
    constexpr auto operator()(T &&left, U &&right) const -> decltype(std::forward<T>(left) << std::forward<U>(right)) {
        return left << right;
    }
};

struct string_logical_not {
    bool operator()(const std::string &v) { return not v.empty(); }
};

ExprNode Parser::compute_literal_binary_expr(LiteralExpr &left, const Token &oper, LiteralExpr &right) {
    switch (oper.type) {
        case TokenType::BIT_OR: return int_binary_operation<std::bit_or<>>(left, right);
        case TokenType::BIT_XOR: return int_binary_operation<std::bit_xor<>>(left, right);
        case TokenType::BIT_AND: return int_binary_operation<std::bit_and<>>(left, right);
        case TokenType::NOT_EQUAL:
            return first_not_null(comparison_operation<std::not_equal_to<>>(left, right),
                string_binary_operation<std::not_equal_to<>>(left, right, Type::BOOL),
                null_binary_operation<std::not_equal_to<>>(left, right, Type::BOOL));
        case TokenType::EQUAL_EQUAL:
            return first_not_null(comparison_operation<std::equal_to<>>(left, right),
                string_binary_operation<std::equal_to<>>(left, right, Type::BOOL),
                null_binary_operation<std::equal_to<>>(left, right, Type::BOOL));
        case TokenType::GREATER: return comparison_operation<std::greater<>>(left, right);
        case TokenType::GREATER_EQUAL: return comparison_operation<std::greater_equal<>>(left, right);
        case TokenType::LESS: return comparison_operation<std::less<>>(left, right);
        case TokenType::LESS_EQUAL: return comparison_operation<std::less_equal<>>(left, right);
        case TokenType::RIGHT_SHIFT: return int_binary_operation<right_shift>(left, right);
        case TokenType::LEFT_SHIFT: return int_binary_operation<left_shift>(left, right);
        case TokenType::MINUS:
            return first_not_null(
                int_binary_operation<std::minus<>>(left, right), numeric_binary_operation<std::minus<>>(left, right));
        case TokenType::PLUS:
            return first_not_null(int_binary_operation<std::plus<>>(left, right),
                numeric_binary_operation<std::plus<>>(left, right), string_binary_operation<std::plus<>>(left, right));
        case TokenType::MODULO:
            if (right.value.is_int() && right.value.to_int() <= 0) {
                error({"Modulo using negative or zero value"}, right.synthesized_attrs.token);
                return nullptr;
            } else {
                return int_binary_operation<std::modulus<>>(left, right);
            }
        case TokenType::SLASH:
            if (right.value.is_numeric() && right.value.to_numeric() == 0.0) {
                error({"Division by zero"}, right.synthesized_attrs.token);
                return nullptr;
            } else {
                return first_not_null(int_binary_operation<std::divides<>>(left, right),
                    numeric_binary_operation<std::divides<>>(left, right));
            }
        case TokenType::STAR:
            return first_not_null(int_binary_operation<std::multiplies<>>(left, right),
                numeric_binary_operation<std::multiplies<>>(left, right));
        case TokenType::DOT_DOT:
        case TokenType::DOT_DOT_EQUAL: return nullptr;
        default: unreachable();
    }
}

ExprNode Parser::compute_literal_ternary_expr(
    LiteralExpr &cond, LiteralExpr &middle, LiteralExpr &right, const Token &oper) {
    if (oper.type == TokenType::QUESTION) {
        return conditional_operation(cond, middle, right);
    } else {
        unreachable();
    }
}

ExprNode Parser::compute_literal_unary_expr(LiteralExpr &value, const Token &oper) {
    switch (oper.type) {
        case TokenType::MINUS:
            return first_not_null(
                int_unary_operation<std::negate<>>(value), float_unary_operation<std::negate<>>(value));
        case TokenType::PLUS:
            if (value.value.is_numeric()) {
                return ExprNode{allocate_node(LiteralExpr, value.value, std::move(value.type))};
            } else {
                return nullptr;
            }
        case TokenType::NOT:
            return first_not_null(int_unary_operation<std::logical_not<>>(value, Type::BOOL),
                float_unary_operation<std::logical_not<>>(value, Type::BOOL),
                string_unary_operation<string_logical_not>(value, Type::BOOL),
                boolean_unary_operation<std::logical_not<>>(value, Type::BOOL));
        case TokenType::BIT_NOT: return int_unary_operation<std::bit_not<>>(value);
        case TokenType::PLUS_PLUS:
        case TokenType::MINUS_MINUS: return nullptr;
        default: unreachable();
    }
}