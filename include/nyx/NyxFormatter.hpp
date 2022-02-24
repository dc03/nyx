#pragma once

/* Copyright (C) 2020-2022  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef NYX_FMT_HPP
#define NYX_FMT_HPP

#include "nyx/AST/AST.hpp"
#include "nyx/Frontend/FrontendContext.hpp"
#include "nyx/Frontend/Module.hpp"

#define USE_TABS                   "use-tabs"
#define TAB_SIZE                   "tab-size"
#define COLLAPSE_SINGLE_LINE_BLOCK "collapse-single-line-block"
#define BRACE_NEXT_LINE            "brace-next-line"

class NyxFormatter : public Visitor {
    std::ostream &out;
    std::size_t indent{};
    std::size_t tab_size{4};
    FrontendContext *ctx{};
    bool use_tabs{false};

    ExprVisitorType format(Expr *expr);
    StmtVisitorType format(Stmt *stmt);
    BaseTypeVisitorType format(BaseType *type);

    void print_vartuple(IdentifierTuple &tuple);

    std::ostream &print_indent(std::size_t tabs);

  public:
    explicit NyxFormatter(std::ostream &out_, FrontendContext *ctx_);

    void format(Module &module);

    ExprVisitorType visit(AssignExpr &expr) override final;
    ExprVisitorType visit(BinaryExpr &expr) override final;
    ExprVisitorType visit(CallExpr &expr) override final;
    ExprVisitorType visit(CommaExpr &expr) override final;
    ExprVisitorType visit(GetExpr &expr) override final;
    ExprVisitorType visit(GroupingExpr &expr) override final;
    ExprVisitorType visit(IndexExpr &expr) override final;
    ExprVisitorType visit(ListExpr &expr) override final;
    ExprVisitorType visit(ListAssignExpr &expr) override final;
    ExprVisitorType visit(ListRepeatExpr &expr) override final;
    ExprVisitorType visit(LiteralExpr &expr) override final;
    ExprVisitorType visit(LogicalExpr &expr) override final;
    ExprVisitorType visit(MoveExpr &expr) override final;
    ExprVisitorType visit(ScopeAccessExpr &expr) override final;
    ExprVisitorType visit(ScopeNameExpr &expr) override final;
    ExprVisitorType visit(SetExpr &expr) override final;
    ExprVisitorType visit(SuperExpr &expr) override final;
    ExprVisitorType visit(TernaryExpr &expr) override final;
    ExprVisitorType visit(ThisExpr &expr) override final;
    ExprVisitorType visit(TupleExpr &expr) override final;
    ExprVisitorType visit(UnaryExpr &expr) override final;
    ExprVisitorType visit(VariableExpr &expr) override final;

    StmtVisitorType visit(BlockStmt &stmt) override final;
    StmtVisitorType visit(BreakStmt &stmt) override final;
    StmtVisitorType visit(ClassStmt &stmt) override final;
    StmtVisitorType visit(ContinueStmt &stmt) override final;
    StmtVisitorType visit(ExpressionStmt &stmt) override final;
    StmtVisitorType visit(ForStmt &stmt) override final;
    StmtVisitorType visit(FunctionStmt &stmt) override final;
    StmtVisitorType visit(IfStmt &stmt) override final;
    StmtVisitorType visit(ReturnStmt &stmt) override final;
    StmtVisitorType visit(SwitchStmt &stmt) override final;
    StmtVisitorType visit(TypeStmt &stmt) override final;
    StmtVisitorType visit(VarStmt &stmt) override final;
    StmtVisitorType visit(VarTupleStmt &stmt) override final;
    StmtVisitorType visit(WhileStmt &stmt) override final;
    StmtVisitorType visit(SingleLineCommentStmt &stmt) override final;
    StmtVisitorType visit(MultiLineCommentStmt &stmt) override final;

    BaseTypeVisitorType visit(PrimitiveType &basetype) override final;
    BaseTypeVisitorType visit(UserDefinedType &basetype) override final;
    BaseTypeVisitorType visit(ListType &basetype) override final;
    BaseTypeVisitorType visit(TupleType &basetype) override final;
    BaseTypeVisitorType visit(TypeofType &basetype) override final;
};

#endif