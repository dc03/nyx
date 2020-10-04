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

    virtual T visit(PrimitiveType<T>& type) = 0;
    virtual T visit(UserDefinedType<T>& type) = 0;
    virtual T visit(ListType<T>& type) = 0;
};

enum class NodeType {
    AssignExpr,
    BinaryExpr,
    CallExpr,
    CommaExpr,
    GetExpr,
    GroupingExpr,
    IndexExpr,
    LiteralExpr,
    LogicalExpr,
    SetExpr,
    SuperExpr,
    TernaryExpr,
    ThisExpr,
    UnaryExpr,
    VariableExpr,

    BlockStmt,
    BreakStmt,
    ClassStmt,
    ContinueStmt,
    ExpressionStmt,
    FunctionStmt,
    IfStmt,
    ImportStmt,
    ReturnStmt,
    SwitchStmt,
    TypeStmt,
    VarStmt,
    WhileStmt,

    PrimitiveType,
    UserDefinedType,
    ListType
};

template <typename T>
struct Expr {
    virtual std::string_view string_tag() = 0;
    virtual NodeType type_tag() = 0;
    virtual T accept(Visitor<T>& visitor) = 0;
    virtual ~Expr() = default;
};

template <typename T>
struct Stmt {
    virtual std::string_view string_tag() = 0;
    virtual NodeType type_tag() = 0;
    virtual T accept(Visitor<T>& visitor) = 0;
    virtual ~Stmt() = default;
};

enum class TypeType {
    BOOL,
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
    std::string_view string_tag() override final {
        return "PrimitiveType";
    }

    NodeType type_tag() override final {
        return NodeType::PrimitiveType;
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

    std::string_view string_tag() override final {
        return "UserDefinedType";
    }

    NodeType type_tag() override final {
        return NodeType::UserDefinedType;
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
    expr_node_t<T> size;

    std::string_view string_tag() override final {
        return "ListType";
    }

    NodeType type_tag() override final {
        return NodeType::ListType;
    }

    ListType(type_node_t<T> contained, expr_node_t<T> size):
        contained{std::move(contained)}, size{std::move(size)} {}

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

    std::string_view string_tag() override final {
        return "AssignExpr";
    }

    NodeType type_tag() override final {
        return NodeType::AssignExpr;
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

    std::string_view string_tag() override final {
        return "BinaryExpr";
    }

    NodeType type_tag() override final {
        return NodeType::BinaryExpr;
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

    std::string_view string_tag() override final {
        return "CallExpr";
    }

    NodeType type_tag() override final {
        return NodeType::CallExpr;
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

    std::string_view string_tag() override final {
        return "CommaExpr";
    }

    NodeType type_tag() override final {
        return NodeType::CommaExpr;
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

    std::string_view string_tag() override final {
        return "GetExpr";
    }

    NodeType type_tag() override final {
        return NodeType::GetExpr;
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

    std::string_view string_tag() override final {
        return "GroupingExpr";
    }

    NodeType type_tag() override final {
        return NodeType::GroupingExpr;
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

    std::string_view string_tag() override final {
        return "IndexExpr";
    }

    NodeType type_tag() override final {
        return NodeType::IndexExpr;
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

    std::string_view string_tag() override final {
        return "LiteralExpr";
    }

    NodeType type_tag() override final {
        return NodeType::LiteralExpr;
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

    std::string_view string_tag() override final {
        return "LogicalExpr";
    }

    NodeType type_tag() override final {
        return NodeType::LogicalExpr;
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

    std::string_view string_tag() override final {
        return "SetExpr";
    }

    NodeType type_tag() override final {
        return NodeType::SetExpr;
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

    std::string_view string_tag() override final {
        return "SuperExpr";
    }

    NodeType type_tag() override final {
        return NodeType::SuperExpr;
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

    std::string_view string_tag() override final {
        return "TernaryExpr";
    }

    NodeType type_tag() override final {
        return NodeType::TernaryExpr;
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

    std::string_view string_tag() override final {
        return "ThisExpr";
    }

    NodeType type_tag() override final {
        return NodeType::ThisExpr;
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

    std::string_view string_tag() override final {
        return "UnaryExpr";
    }

    NodeType type_tag() override final {
        return NodeType::UnaryExpr;
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

    std::string_view string_tag() override final {
        return "VariableExpr";
    }

    NodeType type_tag() override final {
        return NodeType::VariableExpr;
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

    std::string_view string_tag() override final {
        return "BlockStmt";
    }

    NodeType type_tag() override final {
        return NodeType::BlockStmt;
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

    std::string_view string_tag() override final {
        return "BreakStmt";
    }

    NodeType type_tag() override final {
        return NodeType::BreakStmt;
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

    std::string_view string_tag() override final {
        return "ClassStmt";
    }

    NodeType type_tag() override final {
        return NodeType::ClassStmt;
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

    std::string_view string_tag() override final {
        return "ContinueStmt";
    }

    NodeType type_tag() override final {
        return NodeType::ContinueStmt;
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

    std::string_view string_tag() override final {
        return "ExpressionStmt";
    }

    NodeType type_tag() override final {
        return NodeType::ExpressionStmt;
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

    std::string_view string_tag() override final {
        return "FunctionStmt";
    }

    NodeType type_tag() override final {
        return NodeType::FunctionStmt;
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

    std::string_view string_tag() override final {
        return "IfStmt";
    }

    NodeType type_tag() override final {
        return NodeType::IfStmt;
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

    std::string_view string_tag() override final {
        return "ImportStmt";
    }

    NodeType type_tag() override final {
        return NodeType::ImportStmt;
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

    std::string_view string_tag() override final {
        return "ReturnStmt";
    }

    NodeType type_tag() override final {
        return NodeType::ReturnStmt;
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

    std::string_view string_tag() override final {
        return "SwitchStmt";
    }

    NodeType type_tag() override final {
        return NodeType::SwitchStmt;
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

    std::string_view string_tag() override final {
        return "TypeStmt";
    }

    NodeType type_tag() override final {
        return NodeType::TypeStmt;
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

    std::string_view string_tag() override final {
        return "VarStmt";
    }

    NodeType type_tag() override final {
        return NodeType::VarStmt;
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

    std::string_view string_tag() override final {
        return "WhileStmt";
    }

    NodeType type_tag() override final {
        return NodeType::WhileStmt;
    }

    WhileStmt(expr_node_t<T> condition, stmt_node_t<T> body):
        condition{std::move(condition)}, body{std::move(body)} {}

    T accept(Visitor<T>& visitor) override final {
        return visitor.visit(*this);
    }
};

// End of statement node definitions

#endif
