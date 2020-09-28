#pragma once

#ifndef AST_HPP
#  define AST_HPP

#include <memory>
#include <optional>
#include <utility>
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
struct FunctionStmt;
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
	T visit(FunctionStmt<T>& stmt) = 0;
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

// Expression node definitions

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

// End of expression node definitions

// Statement node definitions

template <typename T>
struct BlockStmt final: public Stmt<T> {
	std::vector<stmt_node_t<T>> stmts;

	BlockStmt(std::vector<stmt_node_t<T>> stmts):
		stmts{std::move(stmts)} {}

	T accept(Visitor<T>& visitor) override final {
		return visitor.visit(*this);
	}
};

template <typename T>
struct BreakStmt final: public Stmt<T> {
	Token keyword;

	BreakStmt(Token keyword):
		keyword{keyword} {}

	T accept(Visitor<T>& visitor) override final {
		return visitor.visit(*this);
	}
};

enum class VisibilityType {
	PRIVATE,
	PROTECTED,
	PUBLIC
}

template <typename T>
struct ClassStmt final: public Stmt<T> {
	Token name;
	std::vector<std::pair<stmt_node_t<T>,VisibilityType>> members;
	std::vector<std::pair<stmt_node_t<T>,VisibilityType>> methods;

	ClassStmt(Token name, std::vector<std::pair<stmt_node_t<T>,VisibilityType>> members, std::vector<std::pair<stmt_node_t<T>,VisibilityType>> methods):
		name{name}, members{std::move(members)}, methods{std::move(methods)} {}

	T accept(Visitor<T>& visitor) override final {
		return visitor.visit(*this);
	}
};

template <typename T>
struct ContinueStmt final: public Stmt<T> {
	Token keyword;

	ContinueStmt(Token keyword):
		keyword{keyword} {}

	T accept(Visitor<T>& visitor) override final {
		return visitor.visit(*this);
	}
};

template <typename T>
struct ExpressionStmt final: public Stmt<T> {
	expr_node_t<T> expr;

	ExpressionStmt(expr_node_t<T> expr):
		expr{std::move(expr)} {}

	T accept(Visitor<T>& visitor) override final {
		return visitor.visit(*this);
	}
};

template <typename T>
struct FunctionStmt final: public Stmt<T> {
	Token name;
	std::vector<expr_node_t<T>> params;
	std::vector<stmt_node_t<T>> body;

	FunctionStmt(Token name, std::vector<expr_node_t<T>> params, std::vector<stmt_node_t<T>> body):
		name{name}, params{std::move(params)}, body{std::move(body)} {}

	T accept(Visitor<T>& visitor) override final {
		return visitor.visit(*this);
	}
};

template <typename T>
struct IfStmt final: public Stmt<T> {
	expr_node_t<T> condition;
	stmt_node_t<T> thenBranch;
	stmt_node_t<T> elseBranch;

	IfStmt(expr_node_t<T> condition, stmt_node_t<T> thenBranch, stmt_node_t<T> elseBranch):
		condition{std::move(condition)}, thenBranch{std::move(thenBranch)},elseBranch{std::move(elseBranch)} {}

	T accept(Visitor<T>& visitor) override final {
		return visitor.visit(*this);
	}
};

template <typename T>
struct ImportStmt final: public Stmt<T> {
	Token name;

	ImportStmt(Token name):
		name{name} {}

	T accept(Visitor<T>& visitor) override final {
		return visitor.visit(*this);
	}
};

template <typename T>
struct ReturnStmt final: public Stmt<T> {
	Token keyword;
	expr_node_t<T> value;

	ReturnStmt(Token keyword, expr_node_t<T> value):
		keyword{keyword}, value{std::move(value)} {}

	T accept(Visitor<T>& visitor) override final {
		return visitor.visit(*this);
	}
};

template <typename T>
struct SwitchStmt final: public Stmt<T> {
	expr_node_t<T> condition;
	std::vector<std::pair<expr_node_t<T>,stmt_node_t<T>>> cases;
	std::optional<std::pair<expr_node_t<T>,stmt_node_t<T>>> default_case;

	SwitchStmt(expr_node_t<T> condition, std::vector<std::pair<expr_node_t<T>,stmt_node_t<T>>> cases, std::optional<std::pair<expr_node_t<T>,stmt_node_t<T>>> default_case):
		condition{std::move(condition)}, cases{std::move(cases)}, default_case{std::move(default_case)} {}

	T accept(Visitor<T>& visitor) override final {
		return visitor.visit(*this);
	}
};

template <typename T>
struct TypeStmt final: public Stmt<T> {
	Token name;
	Token type;

	TypeStmt(Token name, Token type):
		name{name}, type{type} {}

	T accept(Visitor<T>& visitor) override final {
		return visitor.visit(*this);
	}
};

template <typename T>
struct VarStmt final: public Stmt<T> {
	Token name;
	expr_node_t<T> initializer;

	VarStmt(Token name, expr_node_t<T> initializer):
		name{name}, initializer{std::move(initializer)} {}

	T accept(Visitor<T>& visitor) override final {
		return visitor.visit(*this);
	}
};

template <typename T>
struct WhileStmt final: public Stmt<T> {
	expr_node_t<T> condition;
	stmt_node_t<T> body;

	WhileStmt(expr_node_t<T> condition, stmt_node_t<T> body):
		condition{std::move(condition)}, body{std::move(body)} {}

	T accept(Visitor<T>& visitor) override final {
		return visitor.visit(*this);
	}
};

// End of statement node definitions

#endif
