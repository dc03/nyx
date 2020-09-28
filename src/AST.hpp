#pragma once

#ifndef AST_HPP
#  define AST_HPP

#include <memory>
#include <vector>
#include "Token.hpp"

template <typename T>
struct Expr;
template <typename T>
struct Stmt;

template <typename T>
using expr_node_t = std::unique_ptr<Expr<T>>;
template <typename T>
using stmt_node_t = std::unique_ptr<Stmt<T>>;

// Expression nodes

template <typename T>
struct AssignExpr;
template <typename T>
struct BinaryExpr;
template <typename T>
struct CallExpr;
template <typename T>
struct GetExpr;
template <typename T>
struct GroupingExpr;
template <typename T>
struct LiteralExpr;
template <typename T>
struct LogicalExpr;
template <typename T>
struct SetExpr;
template <typename T>
struct SuperExpr;
template <typename T>
struct ThisExpr;
template <typename T>
struct UnaryExpr;
template <typename T>
struct VariableExpr;

// Statement nodes

template <typename T>
struct BlockStmt;
template <typename T>
struct BreakStmt;
template <typename T>
struct ClassStmt;
template <typename T>
struct ContinueStmt;
template <typename T>
struct ExpressionStmt;
template <typename T>
struct ForStmt;
template <typename T>
struct FuncStmt;
template <typename T>
struct IfStmt;
template <typename T>
struct ImportStmt;
template <typename T>
struct ReturnStmt;
template <typename T>
struct SwitchStmt;
template <typename T>
struct TypeStmt;
template <typename T>
struct VarStmt;
template <typename T>
struct WhileStmt;

template <typename T>
struct Visitor {
	T visit(AssignExpr<T>& expr) = 0;
	T visit(BinaryExpr<T>& expr) = 0;
	T visit(CallExpr<T>& expr) = 0;
	T visit(GetExpr<T>& expr) = 0;
	T visit(GroupingExpr<T>& expr) = 0;
	T visit(LiteralExpr<T>& expr) = 0;
	T visit(LogicalExpr<T>& expr) = 0;
	T visit(SetExpr<T>& expr) = 0;
	T visit(SuperExpr<T>& expr) = 0;
	T visit(ThisExpr<T>& expr) = 0;
	T visit(UnaryExpr<T>& expr) = 0;
	T visit(VariableExpr<T>& expr) = 0;
	T visit(BlockStmt<T>& stmt) = 0;
	T visit(BreakStmt<T>& stmt) = 0;
	T visit(ClassStmt<T>& stmt) = 0;
	T visit(ContinueStmt<T>& stmt) = 0;
	T visit(ExpressionStmt<T>& stmt) = 0;
	T visit(ForStmt<T>& stmt) = 0;
	T visit(FuncStmt<T>& stmt) = 0;
	T visit(IfStmt<T>& stmt) = 0;
	T visit(ImportStmt<T>& stmt) = 0;
	T visit(ReturnStmt<T>& stmt) = 0;
	T visit(SwitchStmt<T>& stmt) = 0;
	T visit(TypeStmt<T>& stmt) = 0;
	T visit(VarStmt<T>& stmt) = 0;
	T visit(WhileStmt<T>& stmt) = 0;
};

template <typename T>
struct Expr {
	virtual T accept(Visitor<T>& visitor) = 0;
	virtual ~Expr() = default;
};

template <typename T>
struct Stmt {
	virtual T accept(Visitor<T>& visitor) = 0;
	virtual ~Stmt() = default;
};

template <typename T>
struct AssignExpr final: public Expr<T> {
	Token target;
	expr_node_t<T> value;

	AssignExpr(Token target, expr_node_t<T> value):
		target{target}, value{std::move(value)} {}

	T accept(Visitor<T>& visitor) override final {
		return visitor.visit(*this);
	}
};

template <typename T>
struct BinaryExpr final: public Expr<T> {
	expr_node_t<T> left;
	Token oper;
	expr_node_t<T> right;

	BinaryExpr(expr_node_t<T> left, Token oper, expr_node_t<T> right):
		left{std::move(left)}, oper{oper}, right{std::move(right)} {}

	T accept(Visitor<T>& visitor) override final {
		return visitor.visit(*this);
	}
};

template <typename T>
struct CallExpr final: public Expr<T> {
	expr_node_t<T> function;
	Token paren;
	std::vector<expr_node_t<T>> args;

	CallExpr(expr_node_t<T> function, Token paren, std::vector<expr_node_t<T>> args):
		function{std::move(function)}, paren{paren}, args{std::move(args)} {}

	T accept(Visitor<T>& visitor) override final {
		return visitor.visit(*this);
	}
};

template <typename T>
struct GetExpr final: public Expr<T> {
	expr_node_t<T> object;
	Token name;

	GetExpr(expr_node_t<T> object, Token name):
		object{std::move(object)}, name{name} {}

	T accept(Visitor<T>& visitor) override final {
		return visitor.visit(*this);
	}
};

template <typename T>
struct GroupingExpr final: public Expr<T> {
	expr_node_t<T> expr;

	GroupingExpr(expr_node_t<T> expr):
		expr{std::move(expr)} {}

	T accept(Visitor<T>& visitor) override final {
		return visitor.visit(*this);
	}
};

template <typename T>
struct LiteralExpr final: public Expr<T> {
	T value;

	LiteralExpr(T value):
		value{std::move(value)} {}

	T accept(Visitor<T>& visitor) override final {
		return visitor.visit(*this);
	}
};

template <typename T>
struct LogicalExpr final: public Expr<T> {
	expr_node_t<T> left;
	Token oper;
	expr_node_t<T> right;

	LogicalExpr(expr_node_t<T> left, Token oper, expr_node_t<T> right):
		left{std::move(left)}, oper{oper}, right{std::move(right)} {}

	T accept(Visitor<T>& visitor) override final {
		return visitor.visit(*this);
	}
};

template <typename T>
struct SetExpr final: public Expr<T> {
	expr_node_t<T> object;
	Token name;
	expr_node_t<T> value;

	SetExpr(expr_node_t<T> object, Token name, expr_node_t<T> value):
		object{std::move(object)}, name{name}, value{std::move(value)} {}

	T accept(Visitor<T>& visitor) override final {
		return visitor.visit(*this);
	}
};

template <typename T>
struct SuperExpr final: public Expr<T> {
	Token keyword;
	Token name;

	SuperExpr(Token keyword, Token name):
		keyword{keyword}, name{name} {}

	T accept(Visitor<T>& visitor) override final {
		return visitor.visit(*this);
	}
};

template <typename T>
struct ThisExpr final: public Expr<T> {
	Token keyword;

	ThisExpr(Token keyword):
		keyword{keyword} {}

	T accept(Visitor<T>& visitor) override final {
		return visitor.visit(*this);
	}
};

template <typename T>
struct UnaryExpr final: public Expr<T> {
	Token operator;
	expr_node_t<T> right;

	UnaryExpr(Token operator, expr_node_t<T> right):
		operator{operator}, right{std::move(right)} {}

	T accept(Visitor<T>& visitor) override final {
		return visitor.visit(*this);
	}
};

template <typename T>
struct VariableExpr final: public Expr<T> {
	Token name;

	VariableExpr(Token name):
		name{name} {}

	T accept(Visitor<T>& visitor) override final {
		return visitor.visit(*this);
	}
};


#endif
