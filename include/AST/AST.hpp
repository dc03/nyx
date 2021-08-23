#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef AST_HPP
#define AST_HPP

#include "Token.hpp"
#include "VisitorTypes.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

struct Expr;
struct Stmt;
struct BaseType;

using ExprNode = std::unique_ptr<Expr>;
using StmtNode = std::unique_ptr<Stmt>;
using TypeNode = std::unique_ptr<BaseType>;

using RequiresCopy = bool;

// Expression nodes

struct AssignExpr;
struct BinaryExpr;
struct CallExpr;
struct CommaExpr;
struct GetExpr;
struct GroupingExpr;
struct IndexExpr;
struct ListExpr;
struct ListAssignExpr;
struct LiteralExpr;
struct LogicalExpr;
struct MoveExpr;
struct ScopeAccessExpr;
struct ScopeNameExpr;
struct SetExpr;
struct SuperExpr;
struct TernaryExpr;
struct ThisExpr;
struct TupleExpr;
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
struct VarTupleStmt;
struct WhileStmt;

// Type nodes

struct PrimitiveType;
struct UserDefinedType;
struct ListType;
struct TupleType;
struct TypeofType;

struct Visitor {
    virtual ExprVisitorType visit(AssignExpr &expr) = 0;
    virtual ExprVisitorType visit(BinaryExpr &expr) = 0;
    virtual ExprVisitorType visit(CallExpr &expr) = 0;
    virtual ExprVisitorType visit(CommaExpr &expr) = 0;
    virtual ExprVisitorType visit(GetExpr &expr) = 0;
    virtual ExprVisitorType visit(GroupingExpr &expr) = 0;
    virtual ExprVisitorType visit(IndexExpr &expr) = 0;
    virtual ExprVisitorType visit(ListExpr &expr) = 0;
    virtual ExprVisitorType visit(ListAssignExpr &expr) = 0;
    virtual ExprVisitorType visit(LiteralExpr &expr) = 0;
    virtual ExprVisitorType visit(LogicalExpr &expr) = 0;
    virtual ExprVisitorType visit(MoveExpr &expr) = 0;
    virtual ExprVisitorType visit(ScopeAccessExpr &expr) = 0;
    virtual ExprVisitorType visit(ScopeNameExpr &expr) = 0;
    virtual ExprVisitorType visit(SetExpr &expr) = 0;
    virtual ExprVisitorType visit(SuperExpr &expr) = 0;
    virtual ExprVisitorType visit(TernaryExpr &expr) = 0;
    virtual ExprVisitorType visit(ThisExpr &expr) = 0;
    virtual ExprVisitorType visit(TupleExpr &expr) = 0;
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
    virtual StmtVisitorType visit(VarTupleStmt &stmt) = 0;
    virtual StmtVisitorType visit(WhileStmt &stmt) = 0;

    virtual BaseTypeVisitorType visit(PrimitiveType &basetype) = 0;
    virtual BaseTypeVisitorType visit(UserDefinedType &basetype) = 0;
    virtual BaseTypeVisitorType visit(ListType &basetype) = 0;
    virtual BaseTypeVisitorType visit(TupleType &basetype) = 0;
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
    ListExpr,
    ListAssignExpr,
    LiteralExpr,
    LogicalExpr,
    MoveExpr,
    ScopeAccessExpr,
    ScopeNameExpr,
    SetExpr,
    SuperExpr,
    TernaryExpr,
    ThisExpr,
    TupleExpr,
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
    VarTupleStmt,
    WhileStmt,

    PrimitiveType,
    UserDefinedType,
    ListType,
    TupleType,
    TypeofType
};

struct Expr {
    ExprVisitorType resolved{};

    Expr() = default;
    virtual std::string_view string_tag() = 0;
    virtual NodeType type_tag() = 0;
    virtual ExprVisitorType accept(Visitor &visitor) = 0;
    virtual ~Expr() = default;
};

struct Stmt {
    Stmt() = default;
    virtual std::string_view string_tag() = 0;
    virtual NodeType type_tag() = 0;
    virtual StmtVisitorType accept(Visitor &visitor) = 0;
    virtual ~Stmt() = default;
};

struct BaseType {
    Type primitive{};
    bool is_const{};
    bool is_ref{};

    BaseType() = default;
    BaseType(Type primitive, bool is_const, bool is_ref) : primitive{primitive}, is_const{is_const}, is_ref{is_ref} {}
    virtual std::string_view string_tag() = 0;
    virtual NodeType type_tag() = 0;
    virtual BaseTypeVisitorType accept(Visitor &visitor) = 0;
    virtual ~BaseType() = default;
};

// Expression node definitions

enum class NumericConversionType { FLOAT_TO_INT, INT_TO_FLOAT, NONE };

enum class IdentifierType { LOCAL, GLOBAL, FUNCTION, CLASS };

struct AssignExpr final : public Expr {
    Token target{};
    ExprNode value{};
    NumericConversionType conversion_type{};
    RequiresCopy requires_copy{};
    IdentifierType target_type{};

    std::string_view string_tag() override final { return "AssignExpr"; }

    NodeType type_tag() override final { return NodeType::AssignExpr; }

    AssignExpr() = default;
    AssignExpr(Token target, ExprNode value, NumericConversionType conversion_type, RequiresCopy requires_copy,
        IdentifierType target_type)
        : target{std::move(target)},
          value{std::move(value)},
          conversion_type{conversion_type},
          requires_copy{requires_copy},
          target_type{target_type} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct BinaryExpr final : public Expr {
    ExprNode left{};
    ExprNode right{};

    std::string_view string_tag() override final { return "BinaryExpr"; }

    NodeType type_tag() override final { return NodeType::BinaryExpr; }

    BinaryExpr() = default;
    BinaryExpr(ExprNode left, ExprNode right) : left{std::move(left)}, right{std::move(right)} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct CallExpr final : public Expr {
    ExprNode function{};
    std::vector<std::tuple<ExprNode, NumericConversionType, RequiresCopy>> args{};
    bool is_native_call{};

    std::string_view string_tag() override final { return "CallExpr"; }

    NodeType type_tag() override final { return NodeType::CallExpr; }

    CallExpr() = default;
    CallExpr(ExprNode function, std::vector<std::tuple<ExprNode, NumericConversionType, RequiresCopy>> args,
        bool is_native_call)
        : function{std::move(function)}, args{std::move(args)}, is_native_call{is_native_call} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct CommaExpr final : public Expr {
    std::vector<ExprNode> exprs{};

    std::string_view string_tag() override final { return "CommaExpr"; }

    NodeType type_tag() override final { return NodeType::CommaExpr; }

    CommaExpr() = default;
    explicit CommaExpr(std::vector<ExprNode> exprs) : exprs{std::move(exprs)} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct GetExpr final : public Expr {
    ExprNode object{};
    Token name{};

    std::string_view string_tag() override final { return "GetExpr"; }

    NodeType type_tag() override final { return NodeType::GetExpr; }

    GetExpr() = default;
    GetExpr(ExprNode object, Token name) : object{std::move(object)}, name{std::move(name)} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct GroupingExpr final : public Expr {
    ExprNode expr{};
    TypeNode type{};

    std::string_view string_tag() override final { return "GroupingExpr"; }

    NodeType type_tag() override final { return NodeType::GroupingExpr; }

    GroupingExpr() = default;
    GroupingExpr(ExprNode expr, TypeNode type) : expr{std::move(expr)}, type{std::move(type)} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct IndexExpr final : public Expr {
    ExprNode object{};
    ExprNode index{};

    std::string_view string_tag() override final { return "IndexExpr"; }

    NodeType type_tag() override final { return NodeType::IndexExpr; }

    IndexExpr() = default;
    IndexExpr(ExprNode object, ExprNode index) : object{std::move(object)}, index{std::move(index)} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct ListExpr final : public Expr {
    using ElementType = std::tuple<ExprNode, NumericConversionType, RequiresCopy>;

    Token bracket{};
    std::vector<ElementType> elements{};
    std::unique_ptr<ListType> type{};

    std::string_view string_tag() override final { return "ListExpr"; }

    NodeType type_tag() override final { return NodeType::ListExpr; }

    ListExpr() = default;
    ListExpr(Token bracket, std::vector<ElementType> elements, std::unique_ptr<ListType> type)
        : bracket{std::move(bracket)}, elements{std::move(elements)}, type{std::move(type)} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct ListAssignExpr final : public Expr {
    IndexExpr list{};
    ExprNode value{};
    NumericConversionType conversion_type{};
    RequiresCopy requires_copy{};

    std::string_view string_tag() override final { return "ListAssignExpr"; }

    NodeType type_tag() override final { return NodeType::ListAssignExpr; }

    ListAssignExpr() = default;
    ListAssignExpr(IndexExpr list, ExprNode value, NumericConversionType conversion_type, RequiresCopy requires_copy)
        : list{std::move(list)},
          value{std::move(value)},
          conversion_type{conversion_type},
          requires_copy{requires_copy} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct LiteralExpr final : public Expr {
    LiteralValue value{};
    TypeNode type{};

    std::string_view string_tag() override final { return "LiteralExpr"; }

    NodeType type_tag() override final { return NodeType::LiteralExpr; }

    LiteralExpr() = default;
    LiteralExpr(LiteralValue value, TypeNode type) : value{std::move(value)}, type{std::move(type)} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct LogicalExpr final : public Expr {
    ExprNode left{};
    ExprNode right{};

    std::string_view string_tag() override final { return "LogicalExpr"; }

    NodeType type_tag() override final { return NodeType::LogicalExpr; }

    LogicalExpr() = default;
    LogicalExpr(ExprNode left, ExprNode right) : left{std::move(left)}, right{std::move(right)} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct MoveExpr final : public Expr {
    ExprNode expr{};

    std::string_view string_tag() override final { return "MoveExpr"; }

    NodeType type_tag() override final { return NodeType::MoveExpr; }

    MoveExpr() = default;
    explicit MoveExpr(ExprNode expr) : expr{std::move(expr)} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct ScopeAccessExpr final : public Expr {
    ExprNode scope{};
    Token name{};

    std::string_view string_tag() override final { return "ScopeAccessExpr"; }

    NodeType type_tag() override final { return NodeType::ScopeAccessExpr; }

    ScopeAccessExpr() = default;
    ScopeAccessExpr(ExprNode scope, Token name) : scope{std::move(scope)}, name{std::move(name)} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct ScopeNameExpr final : public Expr {
    Token name{};

    std::string_view string_tag() override final { return "ScopeNameExpr"; }

    NodeType type_tag() override final { return NodeType::ScopeNameExpr; }

    ScopeNameExpr() = default;
    explicit ScopeNameExpr(Token name) : name{std::move(name)} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct SetExpr final : public Expr {
    ExprNode object{};
    Token name{};
    ExprNode value{};
    NumericConversionType conversion_type{};
    RequiresCopy requires_copy{};

    std::string_view string_tag() override final { return "SetExpr"; }

    NodeType type_tag() override final { return NodeType::SetExpr; }

    SetExpr() = default;
    SetExpr(
        ExprNode object, Token name, ExprNode value, NumericConversionType conversion_type, RequiresCopy requires_copy)
        : object{std::move(object)},
          name{std::move(name)},
          value{std::move(value)},
          conversion_type{conversion_type},
          requires_copy{requires_copy} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct SuperExpr final : public Expr {
    Token keyword{};
    Token name{};

    std::string_view string_tag() override final { return "SuperExpr"; }

    NodeType type_tag() override final { return NodeType::SuperExpr; }

    SuperExpr() = default;
    SuperExpr(Token keyword, Token name) : keyword{std::move(keyword)}, name{std::move(name)} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct TernaryExpr final : public Expr {
    ExprNode left{};
    ExprNode middle{};
    ExprNode right{};

    std::string_view string_tag() override final { return "TernaryExpr"; }

    NodeType type_tag() override final { return NodeType::TernaryExpr; }

    TernaryExpr() = default;
    TernaryExpr(ExprNode left, ExprNode middle, ExprNode right)
        : left{std::move(left)}, middle{std::move(middle)}, right{std::move(right)} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct ThisExpr final : public Expr {
    Token keyword{};

    std::string_view string_tag() override final { return "ThisExpr"; }

    NodeType type_tag() override final { return NodeType::ThisExpr; }

    ThisExpr() = default;
    explicit ThisExpr(Token keyword) : keyword{std::move(keyword)} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct TupleExpr final : public Expr {
    using ElementType = std::tuple<ExprNode, NumericConversionType, RequiresCopy>;

    Token brace{};
    std::vector<ElementType> elements{};
    std::unique_ptr<TupleType> type{};

    std::string_view string_tag() override final { return "TupleExpr"; }

    NodeType type_tag() override final { return NodeType::TupleExpr; }

    TupleExpr() = default;
    TupleExpr(Token brace, std::vector<ElementType> elements, std::unique_ptr<TupleType> type)
        : brace{std::move(brace)}, elements{std::move(elements)}, type{std::move(type)} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct UnaryExpr final : public Expr {
    Token oper{};
    ExprNode right{};

    std::string_view string_tag() override final { return "UnaryExpr"; }

    NodeType type_tag() override final { return NodeType::UnaryExpr; }

    UnaryExpr() = default;
    UnaryExpr(Token oper, ExprNode right) : oper{std::move(oper)}, right{std::move(right)} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct VariableExpr final : public Expr {
    Token name{};
    IdentifierType type{};

    std::string_view string_tag() override final { return "VariableExpr"; }

    NodeType type_tag() override final { return NodeType::VariableExpr; }

    VariableExpr() = default;
    VariableExpr(Token name, IdentifierType type) : name{std::move(name)}, type{type} {}

    ExprVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

// End of expression node definitions

// Statement node definitions

struct BlockStmt final : public Stmt {
    std::vector<StmtNode> stmts{};

    std::string_view string_tag() override final { return "BlockStmt"; }

    NodeType type_tag() override final { return NodeType::BlockStmt; }

    BlockStmt() = default;
    explicit BlockStmt(std::vector<StmtNode> stmts) : stmts{std::move(stmts)} {}

    StmtVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct BreakStmt final : public Stmt {
    Token keyword{};

    std::string_view string_tag() override final { return "BreakStmt"; }

    NodeType type_tag() override final { return NodeType::BreakStmt; }

    BreakStmt() = default;
    explicit BreakStmt(Token keyword) : keyword{std::move(keyword)} {}

    StmtVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

enum class VisibilityType { PRIVATE, PROTECTED, PUBLIC };

struct ClassStmt final : public Stmt {
    using MemberType = std::pair<std::unique_ptr<VarStmt>, VisibilityType>;
    using MethodType = std::pair<std::unique_ptr<FunctionStmt>, VisibilityType>;

    Token name{};
    FunctionStmt *ctor{};
    FunctionStmt *dtor{};
    std::vector<MemberType> members{};
    std::vector<MethodType> methods{};

    std::string_view string_tag() override final { return "ClassStmt"; }

    NodeType type_tag() override final { return NodeType::ClassStmt; }

    ClassStmt() = default;
    ClassStmt(Token name, FunctionStmt *ctor, FunctionStmt *dtor, std::vector<MemberType> members,
        std::vector<MethodType> methods)
        : name{std::move(name)}, ctor{ctor}, dtor{dtor}, members{std::move(members)}, methods{std::move(methods)} {}

    StmtVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct ContinueStmt final : public Stmt {
    Token keyword{};

    std::string_view string_tag() override final { return "ContinueStmt"; }

    NodeType type_tag() override final { return NodeType::ContinueStmt; }

    ContinueStmt() = default;
    explicit ContinueStmt(Token keyword) : keyword{std::move(keyword)} {}

    StmtVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct ExpressionStmt final : public Stmt {
    ExprNode expr{};

    std::string_view string_tag() override final { return "ExpressionStmt"; }

    NodeType type_tag() override final { return NodeType::ExpressionStmt; }

    ExpressionStmt() = default;
    explicit ExpressionStmt(ExprNode expr) : expr{std::move(expr)} {}

    StmtVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct IdentifierTuple {
    using DeclarationDetails = std::tuple<Token, NumericConversionType, RequiresCopy, TypeNode>;
    using TupleType = std::vector<std::variant<IdentifierTuple, DeclarationDetails>>;
    enum Contained { IDENT_TUPLE = 0, DECL_DETAILS = 1 };

    TupleType tuple;
};

struct FunctionStmt final : public Stmt {
    using ParameterType = std::pair<std::variant<IdentifierTuple, Token>, TypeNode>;
    enum Contained { IDENT_TUPLE = 0, TOKEN = 1 };

    Token name{};
    TypeNode return_type{};
    std::vector<ParameterType> params{};
    StmtNode body{};
    std::vector<ReturnStmt *> return_stmts{};
    std::size_t scope_depth{};

    std::string_view string_tag() override final { return "FunctionStmt"; }

    NodeType type_tag() override final { return NodeType::FunctionStmt; }

    FunctionStmt() = default;
    FunctionStmt(Token name, TypeNode return_type, std::vector<ParameterType> params, StmtNode body,
        std::vector<ReturnStmt *> return_stmts, std::size_t scope_depth)
        : name{std::move(name)},
          return_type{std::move(return_type)},
          params{std::move(params)},
          body{std::move(body)},
          return_stmts{std::move(return_stmts)},
          scope_depth{scope_depth} {}

    StmtVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct IfStmt final : public Stmt {
    Token keyword{};
    ExprNode condition{};
    StmtNode thenBranch{};
    StmtNode elseBranch{};

    std::string_view string_tag() override final { return "IfStmt"; }

    NodeType type_tag() override final { return NodeType::IfStmt; }

    IfStmt() = default;
    IfStmt(Token keyword, ExprNode condition, StmtNode thenBranch, StmtNode elseBranch)
        : keyword{std::move(keyword)},
          condition{std::move(condition)},
          thenBranch{std::move(thenBranch)},
          elseBranch{std::move(elseBranch)} {}

    StmtVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct ReturnStmt final : public Stmt {
    Token keyword{};
    ExprNode value{};
    std::size_t locals_popped{};
    FunctionStmt *function{};

    std::string_view string_tag() override final { return "ReturnStmt"; }

    NodeType type_tag() override final { return NodeType::ReturnStmt; }

    ReturnStmt() = default;
    ReturnStmt(Token keyword, ExprNode value, std::size_t locals_popped, FunctionStmt *function)
        : keyword{std::move(keyword)}, value{std::move(value)}, locals_popped{locals_popped}, function{function} {}

    StmtVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct SwitchStmt final : public Stmt {
    ExprNode condition{};
    std::vector<std::pair<ExprNode, StmtNode>> cases{};
    StmtNode default_case{};

    std::string_view string_tag() override final { return "SwitchStmt"; }

    NodeType type_tag() override final { return NodeType::SwitchStmt; }

    SwitchStmt() = default;
    SwitchStmt(ExprNode condition, std::vector<std::pair<ExprNode, StmtNode>> cases, StmtNode default_case)
        : condition{std::move(condition)}, cases{std::move(cases)}, default_case{std::move(default_case)} {}

    StmtVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct TypeStmt final : public Stmt {
    Token name{};
    TypeNode type{};

    std::string_view string_tag() override final { return "TypeStmt"; }

    NodeType type_tag() override final { return NodeType::TypeStmt; }

    TypeStmt() = default;
    TypeStmt(Token name, TypeNode type) : name{std::move(name)}, type{std::move(type)} {}

    StmtVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct VarStmt final : public Stmt {
    Token keyword{};
    Token name{};
    TypeNode type{};
    ExprNode initializer{};
    NumericConversionType conversion_type{};
    RequiresCopy requires_copy{};

    std::string_view string_tag() override final { return "VarStmt"; }

    NodeType type_tag() override final { return NodeType::VarStmt; }

    VarStmt() = default;
    VarStmt(Token keyword, Token name, TypeNode type, ExprNode initializer, NumericConversionType conversion_type,
        RequiresCopy requires_copy)
        : keyword{std::move(keyword)},
          name{std::move(name)},
          type{std::move(type)},
          initializer{std::move(initializer)},
          conversion_type{conversion_type},
          requires_copy{requires_copy} {}

    StmtVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct VarTupleStmt final : public Stmt {
    friend struct IdentifierTuple;

    IdentifierTuple names{};
    TypeNode type{};
    ExprNode initializer{};
    Token token{};
    Token keyword{};

    std::string_view string_tag() override final { return "VarTupleStmt"; }

    NodeType type_tag() override final { return NodeType::VarTupleStmt; }

    VarTupleStmt() = default;
    VarTupleStmt(IdentifierTuple names, TypeNode type, ExprNode initializer, Token token, Token keyword)
        : names{std::move(names)},
          type{std::move(type)},
          initializer{std::move(initializer)},
          token{std::move(token)},
          keyword{std::move(keyword)} {}

    StmtVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct WhileStmt final : public Stmt {
    Token keyword{};
    ExprNode condition{};
    StmtNode body{};
    StmtNode increment{};

    std::string_view string_tag() override final { return "WhileStmt"; }

    NodeType type_tag() override final { return NodeType::WhileStmt; }

    WhileStmt() = default;
    WhileStmt(Token keyword, ExprNode condition, StmtNode body, StmtNode increment)
        : keyword{std::move(keyword)},
          condition{std::move(condition)},
          body{std::move(body)},
          increment{std::move(increment)} {}

    StmtVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

// End of statement node definitions

// Type node definitions

struct PrimitiveType final : public BaseType {
    std::string_view string_tag() override final { return "PrimitiveType"; }

    NodeType type_tag() override final { return NodeType::PrimitiveType; }

    PrimitiveType() = default;
    explicit PrimitiveType(Type primitive, bool is_const, bool is_ref) : BaseType{primitive, is_const, is_ref} {}

    BaseTypeVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct UserDefinedType final : public BaseType {
    Token name{};

    std::string_view string_tag() override final { return "UserDefinedType"; }

    NodeType type_tag() override final { return NodeType::UserDefinedType; }

    UserDefinedType() = default;
    explicit UserDefinedType(Type primitive, bool is_const, bool is_ref, Token name)
        : BaseType{primitive, is_const, is_ref}, name{std::move(name)} {}

    BaseTypeVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct ListType final : public BaseType {
    TypeNode contained{};

    std::string_view string_tag() override final { return "ListType"; }

    NodeType type_tag() override final { return NodeType::ListType; }

    ListType() = default;
    explicit ListType(Type primitive, bool is_const, bool is_ref, TypeNode contained)
        : BaseType{primitive, is_const, is_ref}, contained{std::move(contained)} {}

    BaseTypeVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct TupleType final : public BaseType {
    std::vector<TypeNode> types{};

    std::string_view string_tag() override final { return "TupleType"; }

    NodeType type_tag() override final { return NodeType::TupleType; }

    TupleType() = default;
    explicit TupleType(Type primitive, bool is_const, bool is_ref, std::vector<TypeNode> types)
        : BaseType{primitive, is_const, is_ref}, types{std::move(types)} {}

    BaseTypeVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

struct TypeofType final : public BaseType {
    ExprNode expr{};

    std::string_view string_tag() override final { return "TypeofType"; }

    NodeType type_tag() override final { return NodeType::TypeofType; }

    TypeofType() = default;
    explicit TypeofType(Type primitive, bool is_const, bool is_ref, ExprNode expr)
        : BaseType{primitive, is_const, is_ref}, expr{std::move(expr)} {}

    BaseTypeVisitorType accept(Visitor &visitor) override final { return visitor.visit(*this); }
};

// End of type node definitions

// Helper function to turn a given type node into a string
std::string stringify(BaseType *node);

// Helper function to copy a given type node (list size expressions are not copied however)
BaseTypeVisitorType copy_type(BaseType *node);

// Helper function to get the size of a given vartuple
std::size_t vartuple_size(IdentifierTuple::TupleType &tuple);
#endif
