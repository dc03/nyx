#pragma once

/* See LICENSE at project root for license details */

#ifndef CODE_GEN_HPP
#define CODE_GEN_HPP

#include "../AST.hpp"
#include "../VM/Chunk.hpp"
#include "../Module.hpp"

class Generator : Visitor {
    Chunk *current_chunk{nullptr};
    Module *current_module{nullptr};
    RuntimeModule generated_module{};

  public:
    Generator() = default;
    RuntimeModule compile_module(Module &module, Chunk &chunk);

    ExprVisitorType compile(Expr *expr);
    StmtVisitorType compile(Stmt *stmt);
    BaseTypeVisitorType compile(BaseType *type);

    ExprVisitorType visit(AccessExpr &expr) override final;
    ExprVisitorType visit(AssignExpr &expr) override final;
    ExprVisitorType visit(BinaryExpr &expr) override final;
    ExprVisitorType visit(CallExpr &expr) override final;
    ExprVisitorType visit(CommaExpr &expr) override final;
    ExprVisitorType visit(GetExpr &expr) override final;
    ExprVisitorType visit(GroupingExpr &expr) override final;
    ExprVisitorType visit(IndexExpr &expr) override final;
    ExprVisitorType visit(LiteralExpr &expr) override final;
    ExprVisitorType visit(LogicalExpr &expr) override final;
    ExprVisitorType visit(SetExpr &expr) override final;
    ExprVisitorType visit(SuperExpr &expr) override final;
    ExprVisitorType visit(TernaryExpr &expr) override final;
    ExprVisitorType visit(ThisExpr &expr) override final;
    ExprVisitorType visit(UnaryExpr &expr) override final;
    ExprVisitorType visit(VariableExpr &expr) override final;

    StmtVisitorType visit(BlockStmt &stmt) override final;
    StmtVisitorType visit(BreakStmt &stmt) override final;
    StmtVisitorType visit(ClassStmt &stmt) override final;
    StmtVisitorType visit(ContinueStmt &stmt) override final;
    StmtVisitorType visit(ExpressionStmt &stmt) override final;
    StmtVisitorType visit(FunctionStmt &stmt) override final;
    StmtVisitorType visit(IfStmt &stmt) override final;
    StmtVisitorType visit(ReturnStmt &stmt) override final;
    StmtVisitorType visit(SwitchStmt &stmt) override final;
    StmtVisitorType visit(TypeStmt &stmt) override final;
    StmtVisitorType visit(VarStmt &stmt) override final;
    StmtVisitorType visit(WhileStmt &stmt) override final;

    BaseTypeVisitorType visit(PrimitiveType &type) override final;
    BaseTypeVisitorType visit(UserDefinedType &type) override final;
    BaseTypeVisitorType visit(ListType &type) override final;
    BaseTypeVisitorType visit(TypeofType &type) override final;
};

#endif