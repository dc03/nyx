#pragma once

/* See LICENSE at project root for license details */

#ifndef AST_HPP
#  define AST_HPP

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "Token.hpp"

using T = std::variant<int, double, std::string, bool, std::nullptr_t>;

struct Expr;
struct Stmt;
struct Type;

using expr_node_t = std::unique_ptr<Expr>;
using stmt_node_t = std::unique_ptr<Stmt>;
using type_node_t = std::unique_ptr<Type>;

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
struct ImportStmt;
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
    virtual T visit(AssignExpr& expr) = 0;
    virtual T visit(BinaryExpr& expr) = 0;
    virtual T visit(CallExpr& expr) = 0;
    virtual T visit(CommaExpr& expr) = 0;
    virtual T visit(GetExpr& expr) = 0;
    virtual T visit(GroupingExpr& expr) = 0;
    virtual T visit(IndexExpr& expr) = 0;
    virtual T visit(LiteralExpr& expr) = 0;
    virtual T visit(LogicalExpr& expr) = 0;
    virtual T visit(SetExpr& expr) = 0;
    virtual T visit(SuperExpr& expr) = 0;
    virtual T visit(TernaryExpr& expr) = 0;
    virtual T visit(ThisExpr& expr) = 0;
    virtual T visit(UnaryExpr& expr) = 0;
    virtual T visit(VariableExpr& expr) = 0;

    virtual T visit(BlockStmt& stmt) = 0;
    virtual T visit(BreakStmt& stmt) = 0;
    virtual T visit(ClassStmt& stmt) = 0;
    virtual T visit(ContinueStmt& stmt) = 0;
    virtual T visit(ExpressionStmt& stmt) = 0;
    virtual T visit(FunctionStmt& stmt) = 0;
    virtual T visit(IfStmt& stmt) = 0;
    virtual T visit(ImportStmt& stmt) = 0;
    virtual T visit(ReturnStmt& stmt) = 0;
    virtual T visit(SwitchStmt& stmt) = 0;
    virtual T visit(TypeStmt& stmt) = 0;
    virtual T visit(VarStmt& stmt) = 0;
    virtual T visit(WhileStmt& stmt) = 0;

    virtual T visit(PrimitiveType& type) = 0;
    virtual T visit(UserDefinedType& type) = 0;
    virtual T visit(ListType& type) = 0;
    virtual T visit(TypeofType& type) = 0;
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
    ListType,
    TypeofType
};

struct Expr {
    virtual std::string_view string_tag() = 0;
    virtual NodeType type_tag() = 0;
    virtual T accept(Visitor& visitor) = 0;
    virtual ~Expr() = default;
};

struct Stmt {
    virtual std::string_view string_tag() = 0;
    virtual NodeType type_tag() = 0;
    virtual T accept(Visitor& visitor) = 0;
    virtual ~Stmt() = default;
};

enum class TypeType {
    BOOL,
    INT,
    FLOAT,
    STRING,
    CLASS,
    LIST,
    TYPEOF,
    NULL_
};

struct Type {
    virtual std::string_view string_tag() = 0;
    virtual NodeType type_tag() = 0;
    virtual T accept(Visitor& visitor) = 0;
    virtual ~Type() = default;
};

struct SharedData {
    TypeType type;
    bool is_const;
    bool is_ref;

};

// Type node definitions

struct PrimitiveType final: public Type {
    SharedData data;

    std::string_view string_tag() override final {
        return "PrimitiveType";
    }

    NodeType type_tag() override final {
        return NodeType::PrimitiveType;
    }

    PrimitiveType(SharedData data):
        data{data} {}

    T accept(Visitor& visitor) override final {
        return visitor.visit(*this);
    }
};

struct UserDefinedType final: public Type {
    SharedData data;
    Token name;

    std::string_view string_tag() override final {
        return "UserDefinedType";
    }

    NodeType type_tag() override final {
        return NodeType::UserDefinedType;
    }

    UserDefinedType(SharedData data, Token name):
        data{data}, name{name} {}

    T accept(Visitor& visitor) override final {
        return visitor.visit(*this);
    }
};

struct ListType final: public Type {
    SharedData data;
    type_node_t contained;
    expr_node_t size;

    std::string_view string_tag() override final {
        return "ListType";
    }

    NodeType type_tag() override final {
        return NodeType::ListType;
    }

    ListType(SharedData data, type_node_t contained, expr_node_t size):
        data{data}, contained{std::move(contained)}, size{std::move(size)} {}

    T accept(Visitor& visitor) override final {
        return visitor.visit(*this);
    }
};

struct TypeofType final: public Type {
    expr_node_t expr;

    std::string_view string_tag() override final {
        return "TypeofType";
    }

    NodeType type_tag() override final {
        return NodeType::TypeofType;
    }

    TypeofType(expr_node_t expr):
        expr{std::move(expr)} {}

    T accept(Visitor& visitor) override final {
        return visitor.visit(*this);
    }
};

// End of type node definitions

// Expression node definitions

struct AssignExpr final: public Expr {
    Token target;
    expr_node_t value;

    std::string_view string_tag() override final {
        return "AssignExpr";
    }

    NodeType type_tag() override final {
        return NodeType::AssignExpr;
    }

    AssignExpr(Token target, expr_node_t value):
        target{target}, value{std::move(value)} {}

    T accept(Visitor& visitor) override final {
        return visitor.visit(*this);
    }
};

struct BinaryExpr final: public Expr {
    expr_node_t left;
    Token oper;
    expr_node_t right;

    std::string_view string_tag() override final {
        return "BinaryExpr";
    }

    NodeType type_tag() override final {
        return NodeType::BinaryExpr;
    }

    BinaryExpr(expr_node_t left, Token oper, expr_node_t right):
        left{std::move(left)}, oper{oper}, right{std::move(right)} {}

    T accept(Visitor& visitor) override final {
        return visitor.visit(*this);
    }
};

struct CallExpr final: public Expr {
    expr_node_t function;
    Token paren;
    std::vector<expr_node_t> args;

    std::string_view string_tag() override final {
        return "CallExpr";
    }

    NodeType type_tag() override final {
        return NodeType::CallExpr;
    }

    CallExpr(expr_node_t function, Token paren, std::vector<expr_node_t> args):
        function{std::move(function)}, paren{paren}, args{std::move(args)} {}

    T accept(Visitor& visitor) override final {
        return visitor.visit(*this);
    }
};

struct CommaExpr final: public Expr {
    std::vector<expr_node_t> exprs;

    std::string_view string_tag() override final {
        return "CommaExpr";
    }

    NodeType type_tag() override final {
        return NodeType::CommaExpr;
    }

    CommaExpr(std::vector<expr_node_t> exprs):
        exprs{std::move(exprs)} {}

    T accept(Visitor& visitor) override final {
        return visitor.visit(*this);
    }
};

struct GetExpr final: public Expr {
    expr_node_t object;
    Token name;

    std::string_view string_tag() override final {
        return "GetExpr";
    }

    NodeType type_tag() override final {
        return NodeType::GetExpr;
    }

    GetExpr(expr_node_t object, Token name):
        object{std::move(object)}, name{name} {}

    T accept(Visitor& visitor) override final {
        return visitor.visit(*this);
    }
};

struct GroupingExpr final: public Expr {
    expr_node_t expr;

    std::string_view string_tag() override final {
        return "GroupingExpr";
    }

    NodeType type_tag() override final {
        return NodeType::GroupingExpr;
    }

    GroupingExpr(expr_node_t expr):
        expr{std::move(expr)} {}

    T accept(Visitor& visitor) override final {
        return visitor.visit(*this);
    }
};

struct IndexExpr final: public Expr {
    expr_node_t object;
    Token oper;
    expr_node_t index;

    std::string_view string_tag() override final {
        return "IndexExpr";
    }

    NodeType type_tag() override final {
        return NodeType::IndexExpr;
    }

    IndexExpr(expr_node_t object, Token oper, expr_node_t index):
        object{std::move(object)}, oper{oper}, index{std::move(index)} {}

    T accept(Visitor& visitor) override final {
        return visitor.visit(*this);
    }
};

struct LiteralExpr final: public Expr {
    T value;

    std::string_view string_tag() override final {
        return "LiteralExpr";
    }

    NodeType type_tag() override final {
        return NodeType::LiteralExpr;
    }

    LiteralExpr(T value):
        value{std::move(value)} {}

    T accept(Visitor& visitor) override final {
        return visitor.visit(*this);
    }
};

struct LogicalExpr final: public Expr {
    expr_node_t left;
    Token oper;
    expr_node_t right;

    std::string_view string_tag() override final {
        return "LogicalExpr";
    }

    NodeType type_tag() override final {
        return NodeType::LogicalExpr;
    }

    LogicalExpr(expr_node_t left, Token oper, expr_node_t right):
        left{std::move(left)}, oper{oper}, right{std::move(right)} {}

    T accept(Visitor& visitor) override final {
        return visitor.visit(*this);
    }
};

struct SetExpr final: public Expr {
    expr_node_t object;
    Token name;
    expr_node_t value;

    std::string_view string_tag() override final {
        return "SetExpr";
    }

    NodeType type_tag() override final {
        return NodeType::SetExpr;
    }

    SetExpr(expr_node_t object, Token name, expr_node_t value):
        object{std::move(object)}, name{name}, value{std::move(value)} {}

    T accept(Visitor& visitor) override final {
        return visitor.visit(*this);
    }
};

struct SuperExpr final: public Expr {
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

    T accept(Visitor& visitor) override final {
        return visitor.visit(*this);
    }
};

struct TernaryExpr final: public Expr {
    expr_node_t left;
    Token question;
    expr_node_t middle;
    expr_node_t right;

    std::string_view string_tag() override final {
        return "TernaryExpr";
    }

    NodeType type_tag() override final {
        return NodeType::TernaryExpr;
    }

    TernaryExpr(expr_node_t left, Token question, expr_node_t middle, expr_node_t right):
        left{std::move(left)}, question{question}, middle{std::move(middle)}, right{std::move(right)} {}

    T accept(Visitor& visitor) override final {
        return visitor.visit(*this);
    }
};

struct ThisExpr final: public Expr {
    Token keyword;

    std::string_view string_tag() override final {
        return "ThisExpr";
    }

    NodeType type_tag() override final {
        return NodeType::ThisExpr;
    }

    ThisExpr(Token keyword):
        keyword{keyword} {}

    T accept(Visitor& visitor) override final {
        return visitor.visit(*this);
    }
};

struct UnaryExpr final: public Expr {
    Token oper;
    expr_node_t right;

    std::string_view string_tag() override final {
        return "UnaryExpr";
    }

    NodeType type_tag() override final {
        return NodeType::UnaryExpr;
    }

    UnaryExpr(Token oper, expr_node_t right):
        oper{oper}, right{std::move(right)} {}

    T accept(Visitor& visitor) override final {
        return visitor.visit(*this);
    }
};

struct VariableExpr final: public Expr {
    Token name;

    std::string_view string_tag() override final {
        return "VariableExpr";
    }

    NodeType type_tag() override final {
        return NodeType::VariableExpr;
    }

    VariableExpr(Token name):
        name{name} {}

    T accept(Visitor& visitor) override final {
        return visitor.visit(*this);
    }
};

// End of expression node definitions

// Statement node definitions

struct BlockStmt final: public Stmt {
    std::vector<stmt_node_t> stmts;

    std::string_view string_tag() override final {
        return "BlockStmt";
    }

    NodeType type_tag() override final {
        return NodeType::BlockStmt;
    }

    BlockStmt(std::vector<stmt_node_t> stmts):
        stmts{std::move(stmts)} {}

    T accept(Visitor& visitor) override final {
        return visitor.visit(*this);
    }
};

struct BreakStmt final: public Stmt {
    Token keyword;

    std::string_view string_tag() override final {
        return "BreakStmt";
    }

    NodeType type_tag() override final {
        return NodeType::BreakStmt;
    }

    BreakStmt(Token keyword):
        keyword{keyword} {}

    T accept(Visitor& visitor) override final {
        return visitor.visit(*this);
    }
};

enum class VisibilityType {
    PRIVATE,
    PROTECTED,
    PUBLIC,
    PRIVATE_DTOR,
    PROTECTED_DTOR,
    PUBLIC_DTOR
};

struct ClassStmt final: public Stmt {
    Token name;
    bool has_ctor;
    bool has_dtor;
    std::vector<std::pair<stmt_node_t,VisibilityType>> members;
    std::vector<std::pair<stmt_node_t,VisibilityType>> methods;

    std::string_view string_tag() override final {
        return "ClassStmt";
    }

    NodeType type_tag() override final {
        return NodeType::ClassStmt;
    }

    ClassStmt(Token name, bool has_ctor, bool has_dtor, std::vector<std::pair<stmt_node_t,VisibilityType>> members, std::vector<std::pair<stmt_node_t,VisibilityType>> methods):
        name{name}, has_ctor{has_ctor}, has_dtor{has_dtor}, members{std::move(members)}, methods{std::move(methods)} {}

    T accept(Visitor& visitor) override final {
        return visitor.visit(*this);
    }
};

struct ContinueStmt final: public Stmt {
    Token keyword;

    std::string_view string_tag() override final {
        return "ContinueStmt";
    }

    NodeType type_tag() override final {
        return NodeType::ContinueStmt;
    }

    ContinueStmt(Token keyword):
        keyword{keyword} {}

    T accept(Visitor& visitor) override final {
        return visitor.visit(*this);
    }
};

struct ExpressionStmt final: public Stmt {
    expr_node_t expr;

    std::string_view string_tag() override final {
        return "ExpressionStmt";
    }

    NodeType type_tag() override final {
        return NodeType::ExpressionStmt;
    }

    ExpressionStmt(expr_node_t expr):
        expr{std::move(expr)} {}

    T accept(Visitor& visitor) override final {
        return visitor.visit(*this);
    }
};

struct FunctionStmt final: public Stmt {
    Token name;
    type_node_t return_type;
    std::vector<std::pair<Token,type_node_t>> params;
    stmt_node_t body;

    std::string_view string_tag() override final {
        return "FunctionStmt";
    }

    NodeType type_tag() override final {
        return NodeType::FunctionStmt;
    }

    FunctionStmt(Token name, type_node_t return_type, std::vector<std::pair<Token,type_node_t>> params, stmt_node_t body):
        name{name}, return_type{std::move(return_type)}, params{std::move(params)}, body{std::move(body)} {}

    T accept(Visitor& visitor) override final {
        return visitor.visit(*this);
    }
};

struct IfStmt final: public Stmt {
    expr_node_t condition;
    stmt_node_t thenBranch;
    stmt_node_t elseBranch;

    std::string_view string_tag() override final {
        return "IfStmt";
    }

    NodeType type_tag() override final {
        return NodeType::IfStmt;
    }

    IfStmt(expr_node_t condition, stmt_node_t thenBranch, stmt_node_t elseBranch):
        condition{std::move(condition)}, thenBranch{std::move(thenBranch)},elseBranch{std::move(elseBranch)} {}

    T accept(Visitor& visitor) override final {
        return visitor.visit(*this);
    }
};

struct ImportStmt final: public Stmt {
    Token name;

    std::string_view string_tag() override final {
        return "ImportStmt";
    }

    NodeType type_tag() override final {
        return NodeType::ImportStmt;
    }

    ImportStmt(Token name):
        name{name} {}

    T accept(Visitor& visitor) override final {
        return visitor.visit(*this);
    }
};

struct ReturnStmt final: public Stmt {
    Token keyword;
    expr_node_t value;

    std::string_view string_tag() override final {
        return "ReturnStmt";
    }

    NodeType type_tag() override final {
        return NodeType::ReturnStmt;
    }

    ReturnStmt(Token keyword, expr_node_t value):
        keyword{keyword}, value{std::move(value)} {}

    T accept(Visitor& visitor) override final {
        return visitor.visit(*this);
    }
};

struct SwitchStmt final: public Stmt {
    expr_node_t condition;
    std::vector<std::pair<expr_node_t,stmt_node_t>> cases;
    std::optional<stmt_node_t> default_case;

    std::string_view string_tag() override final {
        return "SwitchStmt";
    }

    NodeType type_tag() override final {
        return NodeType::SwitchStmt;
    }

    SwitchStmt(expr_node_t condition, std::vector<std::pair<expr_node_t,stmt_node_t>> cases, std::optional<stmt_node_t> default_case):
        condition{std::move(condition)}, cases{std::move(cases)}, default_case{std::move(default_case)} {}

    T accept(Visitor& visitor) override final {
        return visitor.visit(*this);
    }
};

struct TypeStmt final: public Stmt {
    Token name;
    type_node_t type;

    std::string_view string_tag() override final {
        return "TypeStmt";
    }

    NodeType type_tag() override final {
        return NodeType::TypeStmt;
    }

    TypeStmt(Token name, type_node_t type):
        name{name}, type{std::move(type)} {}

    T accept(Visitor& visitor) override final {
        return visitor.visit(*this);
    }
};

struct VarStmt final: public Stmt {
    Token name;
    type_node_t type;
    expr_node_t initializer;

    std::string_view string_tag() override final {
        return "VarStmt";
    }

    NodeType type_tag() override final {
        return NodeType::VarStmt;
    }

    VarStmt(Token name, type_node_t type, expr_node_t initializer):
        name{name}, type{std::move(type)}, initializer{std::move(initializer)} {}

    T accept(Visitor& visitor) override final {
        return visitor.visit(*this);
    }
};

struct WhileStmt final: public Stmt {
    expr_node_t condition;
    stmt_node_t body;

    std::string_view string_tag() override final {
        return "WhileStmt";
    }

    NodeType type_tag() override final {
        return NodeType::WhileStmt;
    }

    WhileStmt(expr_node_t condition, stmt_node_t body):
        condition{std::move(condition)}, body{std::move(body)} {}

    T accept(Visitor& visitor) override final {
        return visitor.visit(*this);
    }
};

// End of statement node definitions

#endif
