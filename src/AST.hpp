#pragma once

/* See LICENSE at project root for license details */

#ifndef AST_HPP
#  define AST_HPP

#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "Token.hpp"

template <typename T>
struct Expr;
template <typename T>
struct Stmt;
template <typename T>
struct Type;

template <typename T>
using expr_node_t = std::unique_ptr<Expr<T>>;
template <typename T>
using stmt_node_t = std::unique_ptr<Stmt<T>>;
template <typename T>
using type_node_t = std::unique_ptr<Type<T>>;

// Expression nodes

template <typename T>
struct AssignExpr;
template <typename T>
struct BinaryExpr;
template <typename T>
struct CallExpr;
template <typename T>
struct CommaExpr;
template <typename T>
struct GetExpr;
template <typename T>
struct GroupingExpr;
template <typename T>
struct IndexExpr;
template <typename T>
struct LiteralExpr;
template <typename T>
struct LogicalExpr;
template <typename T>
struct SetExpr;
template <typename T>
struct SuperExpr;
template <typename T>
struct TernaryExpr;
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

// Type nodes

template <typename T>
struct PrimitiveType;
template <typename T>
struct UserDefinedType;
template <typename T>
struct ListType;

template <typename T>
struct Visitor {
    virtual T visit(AssignExpr<T>& expr) = 0;
    virtual T visit(BinaryExpr<T>& expr) = 0;
    virtual T visit(CallExpr<T>& expr) = 0;
    virtual T visit(CommaExpr<T>& expr) = 0;
    virtual T visit(GetExpr<T>& expr) = 0;
    virtual T visit(GroupingExpr<T>& expr) = 0;
    virtual T visit(IndexExpr<T>& expr) = 0;
    virtual T visit(LiteralExpr<T>& expr) = 0;
    virtual T visit(LogicalExpr<T>& expr) = 0;
    virtual T visit(SetExpr<T>& expr) = 0;
    virtual T visit(SuperExpr<T>& expr) = 0;
    virtual T visit(TernaryExpr<T>& expr) = 0;
    virtual T visit(ThisExpr<T>& expr) = 0;
    virtual T visit(UnaryExpr<T>& expr) = 0;
    virtual T visit(VariableExpr<T>& expr) = 0;

    virtual T visit(BlockStmt<T>& stmt) = 0;
    virtual T visit(BreakStmt<T>& stmt) = 0;
    virtual T visit(ClassStmt<T>& stmt) = 0;
    virtual T visit(ContinueStmt<T>& stmt) = 0;
    virtual T visit(ExpressionStmt<T>& stmt) = 0;
    virtual T visit(FunctionStmt<T>& stmt) = 0;
    virtual T visit(IfStmt<T>& stmt) = 0;
    virtual T visit(ImportStmt<T>& stmt) = 0;
    virtual T visit(ReturnStmt<T>& stmt) = 0;
    virtual T visit(SwitchStmt<T>& stmt) = 0;
    virtual T visit(TypeStmt<T>& stmt) = 0;
    virtual T visit(VarStmt<T>& stmt) = 0;
    virtual T visit(WhileStmt<T>& stmt) = 0;

    virtual T visit(PrimitiveType<T>& stmt) = 0;
    virtual T visit(UserDefinedType<T>& stmt) = 0;
    virtual T visit(ListType<T>& stmt) = 0;
};

template <typename T>
struct Expr {
    virtual std::string_view to_string() = 0;
    virtual T accept(Visitor<T>& visitor) = 0;
    virtual ~Expr() = default;
};

template <typename T>
struct Stmt {
    virtual std::string_view to_string() = 0;
    virtual T accept(Visitor<T>& visitor) = 0;
    virtual ~Stmt() = default;
};

enum class TypeType {
    INT,
    FLOAT,
    STRING,
    CLASS,
    LIST
};

template <typename T>
struct Type {
    TypeType type;
    bool is_const;
    bool is_ref;

    virtual T accept(Visitor<T>& visitor) = 0;
    virtual ~Type() = default;
};

// Type node definitions

template <typename T>
struct PrimitiveType final: public Type<T> {
    std::string_view to_string() override final {
        return "PrimitiveType";
    }

    PrimitiveType()
         {}

    T accept(Visitor<T>& visitor) override final {
        return visitor.visit(*this);
    }
};

template <typename T>
struct UserDefinedType final: public Type<T> {
    Token name;

    std::string_view to_string() override final {
        return "UserDefinedType";
    }

    UserDefinedType(Token name):
        name{name} {}

    T accept(Visitor<T>& visitor) override final {
        return visitor.visit(*this);
    }
};

template <typename T>
struct ListType final: public Type<T> {
    type_node_t<T> contained;

    std::string_view to_string() override final {
        return "ListType";
    }

    ListType(type_node_t<T> contained):
        contained{std::move(contained)} {}

    T accept(Visitor<T>& visitor) override final {
        return visitor.visit(*this);
    }
};

// End of type node definitions

// Expression node definitions

template <typename T>
struct AssignExpr final: public Expr<T> {
    Token target;
    expr_node_t<T> value;

    std::string_view to_string() override final {
        return "AssignExpr";
    }

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

    std::string_view to_string() override final {
        return "BinaryExpr";
    }

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

    std::string_view to_string() override final {
        return "CallExpr";
    }

    CallExpr(expr_node_t<T> function, Token paren, std::vector<expr_node_t<T>> args):
        function{std::move(function)}, paren{paren}, args{std::move(args)} {}

    T accept(Visitor<T>& visitor) override final {
        return visitor.visit(*this);
    }
};

template <typename T>
struct CommaExpr final: public Expr<T> {
    std::vector<expr_node_t<T>> exprs;

    std::string_view to_string() override final {
        return "CommaExpr";
    }

    CommaExpr(std::vector<expr_node_t<T>> exprs):
        exprs{std::move(exprs)} {}

    T accept(Visitor<T>& visitor) override final {
        return visitor.visit(*this);
    }
};

template <typename T>
struct GetExpr final: public Expr<T> {
    expr_node_t<T> object;
    Token name;

    std::string_view to_string() override final {
        return "GetExpr";
    }

    GetExpr(expr_node_t<T> object, Token name):
        object{std::move(object)}, name{name} {}

    T accept(Visitor<T>& visitor) override final {
        return visitor.visit(*this);
    }
};

template <typename T>
struct GroupingExpr final: public Expr<T> {
    expr_node_t<T> expr;

    std::string_view to_string() override final {
        return "GroupingExpr";
    }

    GroupingExpr(expr_node_t<T> expr):
        expr{std::move(expr)} {}

    T accept(Visitor<T>& visitor) override final {
        return visitor.visit(*this);
    }
};

template <typename T>
struct IndexExpr final: public Expr<T> {
    expr_node_t<T> object;
    Token oper;
    expr_node_t<T> index;

    std::string_view to_string() override final {
        return "IndexExpr";
    }

    IndexExpr(expr_node_t<T> object, Token oper, expr_node_t<T> index):
        object{std::move(object)}, oper{oper}, index{std::move(index)} {}

    T accept(Visitor<T>& visitor) override final {
        return visitor.visit(*this);
    }
};

template <typename T>
struct LiteralExpr final: public Expr<T> {
    T value;

    std::string_view to_string() override final {
        return "LiteralExpr";
    }

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

    std::string_view to_string() override final {
        return "LogicalExpr";
    }

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

    std::string_view to_string() override final {
        return "SetExpr";
    }

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

    std::string_view to_string() override final {
        return "SuperExpr";
    }

    SuperExpr(Token keyword, Token name):
        keyword{keyword}, name{name} {}

    T accept(Visitor<T>& visitor) override final {
        return visitor.visit(*this);
    }
};

template <typename T>
struct TernaryExpr final: public Expr<T> {
    expr_node_t<T> left;
    Token question;
    expr_node_t<T> middle;
    expr_node_t<T> right;

    std::string_view to_string() override final {
        return "TernaryExpr";
    }

    TernaryExpr(expr_node_t<T> left, Token question, expr_node_t<T> middle, expr_node_t<T> right):
        left{std::move(left)}, question{question}, middle{std::move(middle)}, right{std::move(right)} {}

    T accept(Visitor<T>& visitor) override final {
        return visitor.visit(*this);
    }
};

template <typename T>
struct ThisExpr final: public Expr<T> {
    Token keyword;

    std::string_view to_string() override final {
        return "ThisExpr";
    }

    ThisExpr(Token keyword):
        keyword{keyword} {}

    T accept(Visitor<T>& visitor) override final {
        return visitor.visit(*this);
    }
};

template <typename T>
struct UnaryExpr final: public Expr<T> {
    Token oper;
    expr_node_t<T> right;

    std::string_view to_string() override final {
        return "UnaryExpr";
    }

    UnaryExpr(Token oper, expr_node_t<T> right):
        oper{oper}, right{std::move(right)} {}

    T accept(Visitor<T>& visitor) override final {
        return visitor.visit(*this);
    }
};

template <typename T>
struct VariableExpr final: public Expr<T> {
    Token name;

    std::string_view to_string() override final {
        return "VariableExpr";
    }

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

    std::string_view to_string() override final {
        return "BlockStmt";
    }

    BlockStmt(std::vector<stmt_node_t<T>> stmts):
        stmts{std::move(stmts)} {}

    T accept(Visitor<T>& visitor) override final {
        return visitor.visit(*this);
    }
};

template <typename T>
struct BreakStmt final: public Stmt<T> {
    Token keyword;

    std::string_view to_string() override final {
        return "BreakStmt";
    }

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
};

template <typename T>
struct ClassStmt final: public Stmt<T> {
    Token name;
    std::vector<std::pair<stmt_node_t<T>,VisibilityType>> members;
    std::vector<std::pair<stmt_node_t<T>,VisibilityType>> methods;

    std::string_view to_string() override final {
        return "ClassStmt";
    }

    ClassStmt(Token name, std::vector<std::pair<stmt_node_t<T>,VisibilityType>> members, std::vector<std::pair<stmt_node_t<T>,VisibilityType>> methods):
        name{name}, members{std::move(members)}, methods{std::move(methods)} {}

    T accept(Visitor<T>& visitor) override final {
        return visitor.visit(*this);
    }
};

template <typename T>
struct ContinueStmt final: public Stmt<T> {
    Token keyword;

    std::string_view to_string() override final {
        return "ContinueStmt";
    }

    ContinueStmt(Token keyword):
        keyword{keyword} {}

    T accept(Visitor<T>& visitor) override final {
        return visitor.visit(*this);
    }
};

template <typename T>
struct ExpressionStmt final: public Stmt<T> {
    expr_node_t<T> expr;

    std::string_view to_string() override final {
        return "ExpressionStmt";
    }

    ExpressionStmt(expr_node_t<T> expr):
        expr{std::move(expr)} {}

    T accept(Visitor<T>& visitor) override final {
        return visitor.visit(*this);
    }
};

template <typename T>
struct FunctionStmt final: public Stmt<T> {
    Token name;
    type_node_t<T> return_type;
    std::vector<std::pair<Token,type_node_t<T>>> params;
    std::vector<stmt_node_t<T>> body;

    std::string_view to_string() override final {
        return "FunctionStmt";
    }

    FunctionStmt(Token name, type_node_t<T> return_type, std::vector<std::pair<Token,type_node_t<T>>> params, std::vector<stmt_node_t<T>> body):
        name{name}, return_type{std::move(return_type)}, params{std::move(params)}, body{std::move(body)} {}

    T accept(Visitor<T>& visitor) override final {
        return visitor.visit(*this);
    }
};

template <typename T>
struct IfStmt final: public Stmt<T> {
    expr_node_t<T> condition;
    stmt_node_t<T> thenBranch;
    stmt_node_t<T> elseBranch;

    std::string_view to_string() override final {
        return "IfStmt";
    }

    IfStmt(expr_node_t<T> condition, stmt_node_t<T> thenBranch, stmt_node_t<T> elseBranch):
        condition{std::move(condition)}, thenBranch{std::move(thenBranch)},elseBranch{std::move(elseBranch)} {}

    T accept(Visitor<T>& visitor) override final {
        return visitor.visit(*this);
    }
};

template <typename T>
struct ImportStmt final: public Stmt<T> {
    Token name;

    std::string_view to_string() override final {
        return "ImportStmt";
    }

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

    std::string_view to_string() override final {
        return "ReturnStmt";
    }

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

    std::string_view to_string() override final {
        return "SwitchStmt";
    }

    SwitchStmt(expr_node_t<T> condition, std::vector<std::pair<expr_node_t<T>,stmt_node_t<T>>> cases, std::optional<std::pair<expr_node_t<T>,stmt_node_t<T>>> default_case):
        condition{std::move(condition)}, cases{std::move(cases)}, default_case{std::move(default_case)} {}

    T accept(Visitor<T>& visitor) override final {
        return visitor.visit(*this);
    }
};

template <typename T>
struct TypeStmt final: public Stmt<T> {
    Token name;
    type_node_t<T> type;

    std::string_view to_string() override final {
        return "TypeStmt";
    }

    TypeStmt(Token name, type_node_t<T> type):
        name{name}, type{std::move(type)} {}

    T accept(Visitor<T>& visitor) override final {
        return visitor.visit(*this);
    }
};

template <typename T>
struct VarStmt final: public Stmt<T> {
    Token name;
    type_node_t<T> type;
    expr_node_t<T> initializer;

    std::string_view to_string() override final {
        return "VarStmt";
    }

    VarStmt(Token name, type_node_t<T> type, expr_node_t<T> initializer):
        name{name}, type{std::move(type)}, initializer{std::move(initializer)} {}

    T accept(Visitor<T>& visitor) override final {
        return visitor.visit(*this);
    }
};

template <typename T>
struct WhileStmt final: public Stmt<T> {
    expr_node_t<T> condition;
    stmt_node_t<T> body;

    std::string_view to_string() override final {
        return "WhileStmt";
    }

    WhileStmt(expr_node_t<T> condition, stmt_node_t<T> body):
        condition{std::move(condition)}, body{std::move(body)} {}

    T accept(Visitor<T>& visitor) override final {
        return visitor.visit(*this);
    }
};

// End of statement node definitions

#endif
