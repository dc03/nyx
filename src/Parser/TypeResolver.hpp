#pragma once

/* See LICENSE at project root for license details */

#ifndef TYPE_RESOLVER_HPP
#  define TYPE_RESOLVER_HPP

#include <string_view>
#include <unordered_map>
#include <vector>

#include "../AST.hpp"

class TypeResolver final: Visitor {
    struct Value {
        std::string_view lexeme{};
        QualifiedTypeInfo info{};
        std::size_t scope_depth{};
    };

    const std::vector<ClassStmt*> &classes;
    const std::vector<FunctionStmt*> &functions;
    std::vector<type_node_t> type_scratch_space{};
    std::vector<Value> values{};

    bool in_class{false};
    bool in_function{false};
    bool in_loop{false};
    bool in_switch{false};
    ClassStmt *current_class{nullptr};
    FunctionStmt *current_function{nullptr};
    std::size_t scope_depth{0};

    template <typename T, typename ... Args>
    BaseType *make_new_type(Type type, bool is_const, bool is_ref, Args&& ... args);
    ExprTypeInfo resolve_class_access(ExprVisitorType &object, const Token& name);
    ExprVisitorType check_inbuilt(VariableExpr *function, const Token &oper, std::vector<expr_node_t> &args);

public:
    TypeResolver(const std::vector<ClassStmt*> &classes, const std::vector<FunctionStmt*> &functions);

    void check(std::vector<stmt_node_t> &program);
    void begin_scope();
    void end_scope();

    ExprVisitorType resolve(Expr *expr);
    StmtVisitorType resolve(Stmt *stmt);
    BaseTypeVisitorType resolve(BaseType *type);

    ExprVisitorType visit(AssignExpr& expr) override final;
    ExprVisitorType visit(BinaryExpr& expr) override final;
    ExprVisitorType visit(CallExpr& expr) override final;
    ExprVisitorType visit(CommaExpr& expr) override final;
    ExprVisitorType visit(GetExpr& expr) override final;
    ExprVisitorType visit(GroupingExpr& expr) override final;
    ExprVisitorType visit(IndexExpr& expr) override final;
    ExprVisitorType visit(LiteralExpr& expr) override final;
    ExprVisitorType visit(LogicalExpr& expr) override final;
    ExprVisitorType visit(SetExpr& expr) override final;
    ExprVisitorType visit(SuperExpr& expr) override final;
    ExprVisitorType visit(TernaryExpr& expr) override final;
    ExprVisitorType visit(ThisExpr& expr) override final;
    ExprVisitorType visit(UnaryExpr& expr) override final;
    ExprVisitorType visit(VariableExpr& expr) override final;

    StmtVisitorType visit(BlockStmt& stmt) override final;
    StmtVisitorType visit(BreakStmt& stmt) override final;
    StmtVisitorType visit(ClassStmt& stmt) override final;
    StmtVisitorType visit(ContinueStmt& stmt) override final;
    StmtVisitorType visit(ExpressionStmt& stmt) override final;
    StmtVisitorType visit(FunctionStmt& stmt) override final;
    StmtVisitorType visit(IfStmt& stmt) override final;
    StmtVisitorType visit(ImportStmt& stmt) override final;
    StmtVisitorType visit(ReturnStmt& stmt) override final;
    StmtVisitorType visit(SwitchStmt& stmt) override final;
    StmtVisitorType visit(TypeStmt& stmt) override final;
    StmtVisitorType visit(VarStmt& stmt) override final;
    StmtVisitorType visit(WhileStmt& stmt) override final;

    BaseTypeVisitorType visit(PrimitiveType& type) override final;
    BaseTypeVisitorType visit(UserDefinedType& type) override final;
    BaseTypeVisitorType visit(ListType& type) override final;
    BaseTypeVisitorType visit(TypeofType& type) override final;
};

#endif