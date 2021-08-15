#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef CODE_GEN_HPP
#define CODE_GEN_HPP

#include "../AST.hpp"
#include "../VirtualMachine/Chunk.hpp"
#include "../VirtualMachine/Module.hpp"
#include "../VirtualMachine/Natives.hpp"

#include <stack>
#include <string_view>

class Generator final : Visitor {
    Chunk *current_chunk{nullptr};
    Module *current_module{nullptr};
    RuntimeModule *current_compiled{nullptr};
    std::stack<std::vector<const BaseType *>> scopes{};
    std::stack<std::vector<std::size_t>> break_stmts{};
    // Push a new vector for every loop or switch statement encountered within a loop or switch statement, with the
    // vector tracking the indexes of the breaks
    std::stack<std::vector<std::size_t>> continue_stmts{};
    // Similar thing as for break statements
    std::unordered_map<std::string_view, NativeFn> natives{};

    void begin_scope();
    void end_scope();
    void patch_jump(std::size_t jump_idx, std::size_t jump_amount);
    void emit_conversion(NumericConversionType conversion_type, std::size_t line_number);
    void emit_three_bytes_of(std::size_t value);

    std::size_t recursively_compile_size(ListType *list);

    void add_vartuple_to_scope(IdentifierTuple::TupleType &tuple);
    std::size_t compile_vartuple(IdentifierTuple::TupleType &tuple);

    ExprVisitorType compile(Expr *expr);
    StmtVisitorType compile(Stmt *stmt);
    BaseTypeVisitorType compile(BaseType *type);

  public:
    static std::vector<RuntimeModule> compiled_modules;

    Generator();
    RuntimeModule compile(Module &module);

    ExprVisitorType visit(AssignExpr &expr) override final;
    ExprVisitorType visit(BinaryExpr &expr) override final;
    ExprVisitorType visit(CallExpr &expr) override final;
    ExprVisitorType visit(CommaExpr &expr) override final;
    ExprVisitorType visit(GetExpr &expr) override final;
    ExprVisitorType visit(GroupingExpr &expr) override final;
    ExprVisitorType visit(IndexExpr &expr) override final;
    ExprVisitorType visit(ListExpr &expr) override final;
    ExprVisitorType visit(ListAssignExpr &expr) override final;
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
    StmtVisitorType visit(FunctionStmt &stmt) override final;
    StmtVisitorType visit(IfStmt &stmt) override final;
    StmtVisitorType visit(ReturnStmt &stmt) override final;
    StmtVisitorType visit(SwitchStmt &stmt) override final;
    StmtVisitorType visit(TypeStmt &stmt) override final;
    StmtVisitorType visit(VarStmt &stmt) override final;
    StmtVisitorType visit(VarTupleStmt &stmt) override final;
    StmtVisitorType visit(WhileStmt &stmt) override final;

    BaseTypeVisitorType visit(PrimitiveType &type) override final;
    BaseTypeVisitorType visit(UserDefinedType &type) override final;
    BaseTypeVisitorType visit(ListType &type) override final;
    BaseTypeVisitorType visit(TupleType &type) override final;
    BaseTypeVisitorType visit(TypeofType &type) override final;
};

#endif