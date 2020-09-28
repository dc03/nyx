#pragma once

#ifndef AST_HPP
#  define AST_HPP

#include <memory>
#include "Token.hpp"

template <typename T>
struct Expr;
template <typename T>
struct Stmt;

template <typename T>
using expr_node_t = std::unique_ptr<Expr<T>>;
template <typename T>
using stmt_node_t = std::unique_ptr<Stmt<T>>;

#endif
