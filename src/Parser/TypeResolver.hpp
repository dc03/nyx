#pragma once

/* See LICENSE at project root for license details */

#ifndef TYPE_RESOLVER_HPP
#  define TYPE_RESOLVER_HPP

#include "../AST.hpp"

class TypeResolver final: Visitor {
    const std::vector<ClassStmt*> &classes;
    const std::vector<FunctionStmt*> &functions;
    const std::vector<VarStmt*> &globals;

public:
    TypeResolver(const std::vector<ClassStmt*> &classes, const std::vector<FunctionStmt*> &functions,
                 const std::vector<VarStmt*> &globals);

    T visit(AssignExpr& expr) override final;
    T visit(BinaryExpr& expr) override final;
    T visit(CallExpr& expr) override final;
    T visit(CommaExpr& expr) override final;
    T visit(GetExpr& expr) override final;
    T visit(GroupingExpr& expr) override final;
    T visit(IndexExpr& expr) override final;
    T visit(LiteralExpr& expr) override final;
    T visit(LogicalExpr& expr) override final;
    T visit(SetExpr& expr) override final;
    T visit(SuperExpr& expr) override final;
    T visit(TernaryExpr& expr) override final;
    T visit(ThisExpr& expr) override final;
    T visit(UnaryExpr& expr) override final;
    T visit(VariableExpr& expr) override final;

    T visit(BlockStmt& stmt) override final;
    T visit(BreakStmt& stmt) override final;
    T visit(ClassStmt& stmt) override final;
    T visit(ContinueStmt& stmt) override final;
    T visit(ExpressionStmt& stmt) override final;
    T visit(FunctionStmt& stmt) override final;
    T visit(IfStmt& stmt) override final;
    T visit(ImportStmt& stmt) override final;
    T visit(ReturnStmt& stmt) override final;
    T visit(SwitchStmt& stmt) override final;
    T visit(TypeStmt& stmt) override final;
    T visit(VarStmt& stmt) override final;
    T visit(WhileStmt& stmt) override final;

    T visit(PrimitiveType& type) override final;
    T visit(UserDefinedType& type) override final;
    T visit(ListType& type) override final;
    T visit(TypeofType& type) override final;
};

#endif