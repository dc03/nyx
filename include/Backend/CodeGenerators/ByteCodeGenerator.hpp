#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef BYTE_CODE_GENERATOR_HPP
#define BYTE_CODE_GENERATOR_HPP

#include "AST/AST.hpp"
#include "Backend/RuntimeContext.hpp"
#include "Backend/RuntimeModule.hpp"
#include "Backend/VirtualMachine/Chunk.hpp"
#include "Backend/VirtualMachine/Natives.hpp"
#include "Frontend/CompileContext.hpp"
#include "Frontend/Module.hpp"

#include <stack>
#include <string_view>

class ByteCodeGenerator final : Visitor {
    CompileContext *compile_ctx{};
    RuntimeContext *runtime_ctx{};

    Chunk *current_chunk{nullptr};
    Module *current_module{nullptr};
    RuntimeModule *current_compiled{nullptr};

    std::size_t current_scope_depth{};
    std::vector<std::pair<const BaseType *, std::size_t>> scopes{};

    // Push a new vector for every loop or switch statement encountered within a loop or switch statement, with the
    // vector tracking the indexes of the breaks
    std::stack<std::vector<std::size_t>> continue_stmts{};
    // Similar thing as for break statements
    std::stack<std::vector<std::size_t>> break_stmts{};

    std::unordered_map<std::string_view, Native> natives{};

    bool variable_tracking_suppressed{};

    void begin_scope();
    void remove_topmost_scope();
    void end_scope();
    void destroy_locals(std::size_t until_scope);
    void add_to_scope(const BaseType *type);
    void patch_jump(std::size_t jump_idx, std::size_t jump_amount);
    void emit_conversion(NumericConversionType conversion_type, std::size_t line_number);
    void emit_operand(std::size_t value);
    void emit_stack_slot(std::size_t value);
    void emit_destructor_call(ClassStmt *class_, std::size_t line);

    bool requires_copy(ExprNode &what, TypeNode &type);

    void add_vartuple_to_scope(IdentifierTuple::TupleType &tuple);
    std::size_t compile_vartuple(IdentifierTuple::TupleType &tuple, TupleType &type);

    bool is_ctor_call(ExprNode &node);
    ClassStmt *get_class(ExprNode &node);

    bool suppress_variable_tracking();
    void restore_variable_tracking(bool previous);
    void make_instance(ClassStmt *class_);
    Value::IntType get_member_index(ClassStmt *stmt, const std::string &name);

    std::string mangle_function(FunctionStmt &stmt);
    std::string mangle_scope_access(ScopeAccessExpr &expr);
    std::string mangle_member_access(ClassStmt *class_, std::string &name);

    ExprVisitorType compile(Expr *expr);
    StmtVisitorType compile(Stmt *stmt);
    BaseTypeVisitorType compile(BaseType *type);

  public:
    ByteCodeGenerator();

    void set_compile_ctx(CompileContext *compile_ctx_);
    void set_runtime_ctx(RuntimeContext *runtime_ctx_);

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