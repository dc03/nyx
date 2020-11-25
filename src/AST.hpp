#pragma once

/* Copyright (C) 2020  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef AST_HPP
#define AST_HPP

#include "Parser/VisitorTypes.hpp"
#include "Token.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct Expr;
struct Stmt;
struct BaseType;

using expr_node_t = std::unique_ptr<Expr>;
using stmt_node_t = std::unique_ptr<Stmt>;
using type_node_t = std::unique_ptr<BaseType>;

// Expression nodes

struct AssignExpr;
struct BinaryExpr;
struct CallExpr;
struct CommaExpr;
struct GetExpr;
struct GroupingExpr;
struct IndexExpr;
struct LiteralExpr;
struct LogicalExpr;
struct ScopeAccessExpr;
struct ScopeNameExpr;
struct SetExpr;
struct SuperExpr;
struct TernaryExpr;
struct ThisExpr;
struct UnaryExpr;
struct VariableExpr;

// Statement nodes

struct BlockStmt;
struct BreakStmt;
struct ClassStmt;
struct ContinueStmt;
struct ExpressionStmt;
struct FunctionStmt;
struct IfStmt;
struct ReturnStmt;
struct SwitchStmt;
struct TypeStmt;
struct VarStmt;
struct WhileStmt;

// Type nodes

struct PrimitiveType;
struct UserDefinedType;
struct ListType;
struct TypeofType;

struct Visitor {
    virtual ExprVisitorType visit(AssignExpr &expr) = 0;
    virtual ExprVisitorType visit(BinaryExpr &expr) = 0;
    virtual ExprVisitorType visit(CallExpr &expr) = 0;
    virtual ExprVisitorType visit(CommaExpr &expr) = 0;
    virtual ExprVisitorType visit(GetExpr &expr) = 0;
    virtual ExprVisitorType visit(GroupingExpr &expr) = 0;
    virtual ExprVisitorType visit(IndexExpr &expr) = 0;
    virtual ExprVisitorType visit(LiteralExpr &expr) = 0;
    virtual ExprVisitorType visit(LogicalExpr &expr) = 0;
    virtual ExprVisitorType visit(ScopeAccessExpr &expr) = 0;
    virtual ExprVisitorType visit(ScopeNameExpr &expr) = 0;
    virtual ExprVisitorType visit(SetExpr &expr) = 0;
    virtual ExprVisitorType visit(SuperExpr &expr) = 0;
    virtual ExprVisitorType visit(TernaryExpr &expr) = 0;
    virtual ExprVisitorType visit(ThisExpr &expr) = 0;
    virtual ExprVisitorType visit(UnaryExpr &expr) = 0;
    virtual ExprVisitorType visit(VariableExpr &expr) = 0;

    virtual StmtVisitorType visit(BlockStmt &stmt) = 0;
    virtual StmtVisitorType visit(BreakStmt &stmt) = 0;
    virtual StmtVisitorType visit(ClassStmt &stmt) = 0;
    virtual StmtVisitorType visit(ContinueStmt &stmt) = 0;
    virtual StmtVisitorType visit(ExpressionStmt &stmt) = 0;
    virtual StmtVisitorType visit(FunctionStmt &stmt) = 0;
    virtual StmtVisitorType visit(IfStmt &stmt) = 0;
    virtual StmtVisitorType visit(ReturnStmt &stmt) = 0;
    virtual StmtVisitorType visit(SwitchStmt &stmt) = 0;
    virtual StmtVisitorType visit(TypeStmt &stmt) = 0;
    virtual StmtVisitorType visit(VarStmt &stmt) = 0;
    virtual StmtVisitorType visit(WhileStmt &stmt) = 0;

    virtual BaseTypeVisitorType visit(PrimitiveType &basetype) = 0;
    virtual BaseTypeVisitorType visit(UserDefinedType &basetype) = 0;
    virtual BaseTypeVisitorType visit(ListType &basetype) = 0;
    virtual BaseTypeVisitorType visit(TypeofType &basetype) = 0;
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
    ScopeAccessExpr,
    ScopeNameExpr,
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
    ReturnStmt,
    SwitchStmt,
    TypeStmt,
    VarStmt,
    WhileStmt,

    PrimitiveType,
    UserDefinedType,
    ListType,
    TypeofType
};

struct Expr {
    virtual std::string_view string_tag() = 0;
    virtual NodeType type_tag() = 0;
    virtual ExprVisitorType accept(Visitor &visitor) = 0;
    virtual ~Expr() = default;
};

struct Stmt {
    virtual std::string_view string_tag() = 0;
    virtual NodeType type_tag() = 0;
    virtual StmtVisitorType accept(Visitor &visitor) = 0;
    virtual ~Stmt() = default;
};

struct SharedData {
    Type type;
    bool is_const;
    bool is_ref;
};

struct BaseType {
    SharedData data;

    BaseType() = default;
    BaseType(SharedData data) : data{data} {}
    virtual std::string_view string_tag() = 0;
    virtual NodeType type_tag() = 0;
    virtual BaseTypeVisitorType accept(Visitor &visitor) = 0;
    virtual ~BaseType() = default;
};

// Type node definitions

struct PrimitiveType final : public BaseType {
    std::string_view string_tag() override final { return "PrimitiveType"; }

    NodeType type_tag() override final { return NodeType::PrimitiveType; }

    PrimitiveType(SharedData data) : BaseType{data} {}

    BaseTypeVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct UserDefinedType final : public BaseType {
    Token name;

    std::string_view string_tag() override final { return "UserDefinedType"; }

    NodeType type_tag() override final { return NodeType::UserDefinedType; }

    UserDefinedType(SharedData data, Token name) : BaseType{data}, name{name} {}

    BaseTypeVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct ListType final : public BaseType {
    type_node_t contained;
    expr_node_t size;

    std::string_view string_tag() override final { return "ListType"; }

    NodeType type_tag() override final { return NodeType::ListType; }

    ListType(SharedData data, type_node_t contained, expr_node_t size)
        : BaseType{data}, contained{std::move(contained)}, size{std::move(size)} {}

    BaseTypeVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct TypeofType final : public BaseType {
    expr_node_t expr;

    std::string_view string_tag() override final { return "TypeofType"; }

    NodeType type_tag() override final { return NodeType::TypeofType; }

    TypeofType(SharedData data, expr_node_t expr) : BaseType{data}, expr{std::move(expr)} {}

    BaseTypeVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

// End of type node definitions

// Expression node definitions

struct AssignExpr final : public Expr {
    Token target;
    expr_node_t value;

    std::string_view string_tag() override final { return "AssignExpr"; }

    NodeType type_tag() override final { return NodeType::AssignExpr; }

    AssignExpr(Token target, expr_node_t value) : target{target}, value{std::move(value)} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct BinaryExpr final : public Expr {
    expr_node_t left;
    Token oper;
    expr_node_t right;
    ExprVisitorType resolved_type;

    std::string_view string_tag() override final { return "BinaryExpr"; }

    NodeType type_tag() override final { return NodeType::BinaryExpr; }

    BinaryExpr(expr_node_t left, Token oper, expr_node_t right, ExprVisitorType resolved_type)
        : left{std::move(left)}, oper{oper}, right{std::move(right)}, resolved_type{resolved_type} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct CallExpr final : public Expr {
    expr_node_t function;
    Token paren;
    std::vector<expr_node_t> args;

    std::string_view string_tag() override final { return "CallExpr"; }

    NodeType type_tag() override final { return NodeType::CallExpr; }

    CallExpr(expr_node_t function, Token paren, std::vector<expr_node_t> args)
        : function{std::move(function)}, paren{paren}, args{std::move(args)} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct CommaExpr final : public Expr {
    std::vector<expr_node_t> exprs;

    std::string_view string_tag() override final { return "CommaExpr"; }

    NodeType type_tag() override final { return NodeType::CommaExpr; }

    CommaExpr(std::vector<expr_node_t> exprs) : exprs{std::move(exprs)} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct GetExpr final : public Expr {
    expr_node_t object;
    Token name;

    std::string_view string_tag() override final { return "GetExpr"; }

    NodeType type_tag() override final { return NodeType::GetExpr; }

    GetExpr(expr_node_t object, Token name) : object{std::move(object)}, name{name} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct GroupingExpr final : public Expr {
    expr_node_t expr;

    std::string_view string_tag() override final { return "GroupingExpr"; }

    NodeType type_tag() override final { return NodeType::GroupingExpr; }

    GroupingExpr(expr_node_t expr) : expr{std::move(expr)} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct IndexExpr final : public Expr {
    expr_node_t object;
    Token oper;
    expr_node_t index;

    std::string_view string_tag() override final { return "IndexExpr"; }

    NodeType type_tag() override final { return NodeType::IndexExpr; }

    IndexExpr(expr_node_t object, Token oper, expr_node_t index)
        : object{std::move(object)}, oper{oper}, index{std::move(index)} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct LiteralExpr final : public Expr {
    LiteralValue value;
    Token lexeme;
    type_node_t type;

    std::string_view string_tag() override final { return "LiteralExpr"; }

    NodeType type_tag() override final { return NodeType::LiteralExpr; }

    LiteralExpr(LiteralValue value, Token lexeme, type_node_t type)
        : value{std::move(value)}, lexeme{std::move(lexeme)}, type{std::move(type)} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct LogicalExpr final : public Expr {
    expr_node_t left;
    Token oper;
    expr_node_t right;

    std::string_view string_tag() override final { return "LogicalExpr"; }

    NodeType type_tag() override final { return NodeType::LogicalExpr; }

    LogicalExpr(expr_node_t left, Token oper, expr_node_t right)
        : left{std::move(left)}, oper{oper}, right{std::move(right)} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct ScopeAccessExpr final : public Expr {
    expr_node_t scope;
    Token name;

    std::string_view string_tag() override final { return "ScopeAccessExpr"; }

    NodeType type_tag() override final { return NodeType::ScopeAccessExpr; }

    ScopeAccessExpr(expr_node_t scope, Token name) : scope{std::move(scope)}, name{name} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct ScopeNameExpr final : public Expr {
    Token name;

    std::string_view string_tag() override final { return "ScopeNameExpr"; }

    NodeType type_tag() override final { return NodeType::ScopeNameExpr; }

    ScopeNameExpr(Token name) : name{name} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct SetExpr final : public Expr {
    expr_node_t object;
    Token name;
    expr_node_t value;

    std::string_view string_tag() override final { return "SetExpr"; }

    NodeType type_tag() override final { return NodeType::SetExpr; }

    SetExpr(expr_node_t object, Token name, expr_node_t value)
        : object{std::move(object)}, name{name}, value{std::move(value)} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct SuperExpr final : public Expr {
    Token keyword;
    Token name;

    std::string_view string_tag() override final { return "SuperExpr"; }

    NodeType type_tag() override final { return NodeType::SuperExpr; }

    SuperExpr(Token keyword, Token name) : keyword{keyword}, name{name} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct TernaryExpr final : public Expr {
    expr_node_t left;
    Token question;
    expr_node_t middle;
    expr_node_t right;

    std::string_view string_tag() override final { return "TernaryExpr"; }

    NodeType type_tag() override final { return NodeType::TernaryExpr; }

    TernaryExpr(expr_node_t left, Token question, expr_node_t middle, expr_node_t right)
        : left{std::move(left)}, question{question}, middle{std::move(middle)}, right{std::move(right)} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct ThisExpr final : public Expr {
    Token keyword;

    std::string_view string_tag() override final { return "ThisExpr"; }

    NodeType type_tag() override final { return NodeType::ThisExpr; }

    ThisExpr(Token keyword) : keyword{keyword} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct UnaryExpr final : public Expr {
    Token oper;
    expr_node_t right;

    std::string_view string_tag() override final { return "UnaryExpr"; }

    NodeType type_tag() override final { return NodeType::UnaryExpr; }

    UnaryExpr(Token oper, expr_node_t right) : oper{oper}, right{std::move(right)} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct VariableExpr final : public Expr {
    Token name;
    std::size_t scope_depth;

    std::string_view string_tag() override final { return "VariableExpr"; }

    NodeType type_tag() override final { return NodeType::VariableExpr; }

    VariableExpr(Token name, std::size_t scope_depth) : name{name}, scope_depth{scope_depth} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

// End of expression node definitions

// Statement node definitions

struct BlockStmt final : public Stmt {
    std::vector<stmt_node_t> stmts;

    std::string_view string_tag() override final { return "BlockStmt"; }

    NodeType type_tag() override final { return NodeType::BlockStmt; }

    BlockStmt(std::vector<stmt_node_t> stmts) : stmts{std::move(stmts)} {}

    StmtVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct BreakStmt final : public Stmt {
    Token keyword;

    std::string_view string_tag() override final { return "BreakStmt"; }

    NodeType type_tag() override final { return NodeType::BreakStmt; }

    BreakStmt(Token keyword) : keyword{keyword} {}

    StmtVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

enum class VisibilityType { PRIVATE, PROTECTED, PUBLIC, PRIVATE_DTOR, PROTECTED_DTOR, PUBLIC_DTOR };

struct ClassStmt final : public Stmt {
    Token name;
    FunctionStmt *ctor;
    FunctionStmt *dtor;
    std::vector<std::pair<stmt_node_t, VisibilityType>> members;
    std::vector<std::pair<stmt_node_t, VisibilityType>> methods;

    std::string_view string_tag() override final { return "ClassStmt"; }

    NodeType type_tag() override final { return NodeType::ClassStmt; }

    ClassStmt(Token name, FunctionStmt *ctor, FunctionStmt *dtor,
        std::vector<std::pair<stmt_node_t, VisibilityType>> members,
        std::vector<std::pair<stmt_node_t, VisibilityType>> methods)
        : name{name}, ctor{ctor}, dtor{dtor}, members{std::move(members)}, methods{std::move(methods)} {}

    StmtVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct ContinueStmt final : public Stmt {
    Token keyword;

    std::string_view string_tag() override final { return "ContinueStmt"; }

    NodeType type_tag() override final { return NodeType::ContinueStmt; }

    ContinueStmt(Token keyword) : keyword{keyword} {}

    StmtVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct ExpressionStmt final : public Stmt {
    expr_node_t expr;

    std::string_view string_tag() override final { return "ExpressionStmt"; }

    NodeType type_tag() override final { return NodeType::ExpressionStmt; }

    ExpressionStmt(expr_node_t expr) : expr{std::move(expr)} {}

    StmtVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct FunctionStmt final : public Stmt {
    Token name;
    type_node_t return_type;
    std::vector<std::pair<Token, type_node_t>> params;
    stmt_node_t body;

    std::string_view string_tag() override final { return "FunctionStmt"; }

    NodeType type_tag() override final { return NodeType::FunctionStmt; }

    FunctionStmt(
        Token name, type_node_t return_type, std::vector<std::pair<Token, type_node_t>> params, stmt_node_t body)
        : name{name}, return_type{std::move(return_type)}, params{std::move(params)}, body{std::move(body)} {}

    StmtVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct IfStmt final : public Stmt {
    expr_node_t condition;
    stmt_node_t thenBranch;
    stmt_node_t elseBranch;

    std::string_view string_tag() override final { return "IfStmt"; }

    NodeType type_tag() override final { return NodeType::IfStmt; }

    IfStmt(expr_node_t condition, stmt_node_t thenBranch, stmt_node_t elseBranch)
        : condition{std::move(condition)}, thenBranch{std::move(thenBranch)}, elseBranch{std::move(elseBranch)} {}

    StmtVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct ReturnStmt final : public Stmt {
    Token keyword;
    expr_node_t value;

    std::string_view string_tag() override final { return "ReturnStmt"; }

    NodeType type_tag() override final { return NodeType::ReturnStmt; }

    ReturnStmt(Token keyword, expr_node_t value) : keyword{keyword}, value{std::move(value)} {}

    StmtVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct SwitchStmt final : public Stmt {
    expr_node_t condition;
    std::vector<std::pair<expr_node_t, stmt_node_t>> cases;
    stmt_node_t default_case;

    std::string_view string_tag() override final { return "SwitchStmt"; }

    NodeType type_tag() override final { return NodeType::SwitchStmt; }

    SwitchStmt(expr_node_t condition, std::vector<std::pair<expr_node_t, stmt_node_t>> cases, stmt_node_t default_case)
        : condition{std::move(condition)}, cases{std::move(cases)}, default_case{std::move(default_case)} {}

    StmtVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct TypeStmt final : public Stmt {
    Token name;
    type_node_t type;

    std::string_view string_tag() override final { return "TypeStmt"; }

    NodeType type_tag() override final { return NodeType::TypeStmt; }

    TypeStmt(Token name, type_node_t type) : name{name}, type{std::move(type)} {}

    StmtVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct VarStmt final : public Stmt {
    bool is_val;
    Token name;
    type_node_t type;
    expr_node_t initializer;

    std::string_view string_tag() override final { return "VarStmt"; }

    NodeType type_tag() override final { return NodeType::VarStmt; }

    VarStmt(bool is_val, Token name, type_node_t type, expr_node_t initializer)
        : is_val{is_val}, name{name}, type{std::move(type)}, initializer{std::move(initializer)} {}

    StmtVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct WhileStmt final : public Stmt {
    Token keyword;
    expr_node_t condition;
    stmt_node_t body;

    std::string_view string_tag() override final { return "WhileStmt"; }

    NodeType type_tag() override final { return NodeType::WhileStmt; }

    WhileStmt(Token keyword, expr_node_t condition, stmt_node_t body)
        : keyword{keyword}, condition{std::move(condition)}, body{std::move(body)} {}

    StmtVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

// End of statement node definitions

// Helper function to turn a given type node into a string
std::string stringify(BaseType *node);

// Helper function to copy a given type node (list size expressions are not copied however)
BaseTypeVisitorType copy_type(BaseType *node);

#endif
