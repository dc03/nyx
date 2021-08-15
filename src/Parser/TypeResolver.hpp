#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef TYPE_RESOLVER_HPP
#define TYPE_RESOLVER_HPP

#include "../AST.hpp"
#include "../VirtualMachine/Module.hpp"

#include <string_view>
#include <tuple>
#include <unordered_map>
#include <vector>

class TypeResolver final : Visitor {
    struct Value {
        std::string_view lexeme{};
        QualifiedTypeInfo info{};
        std::size_t scope_depth{};
        ClassStmt *class_{nullptr};
        std::size_t stack_slot{};
    };

    Module &current_module;
    const std::unordered_map<std::string_view, ClassStmt *> &classes;
    const std::unordered_map<std::string_view, FunctionStmt *> &functions;
    std::vector<TypeNode> type_scratch_space{};
    std::vector<Value> values{};

    bool in_ctor{false};
    bool in_dtor{false};
    bool in_class{false};
    bool in_function{false};
    bool in_loop{false};
    bool in_switch{false};
    ClassStmt *current_class{nullptr};
    FunctionStmt *current_function{nullptr};
    std::size_t scope_depth{0};

    template <typename T, typename... Args>
    BaseType *make_new_type(Type type, bool is_const, bool is_ref, Args &&...args);
    ExprTypeInfo resolve_class_access(ExprVisitorType &object, const Token &name);
    ExprVisitorType check_inbuilt(VariableExpr *function, const Token &oper,
        std::vector<std::tuple<ExprNode, NumericConversionType, bool>> &args);
    ClassStmt *find_class(const std::string &class_name);
    FunctionStmt *find_function(const std::string &function_name);
    bool convertible_to(
        QualifiedTypeInfo to, QualifiedTypeInfo from, bool from_lvalue, const Token &where, bool in_initializer);

    void replace_if_typeof(TypeNode &type);
    void infer_list_type(ListExpr *of, ListType *from);
    void infer_tuple_type(TupleExpr *of, TupleType *from);
    bool are_equivalent_primitives(QualifiedTypeInfo first, QualifiedTypeInfo second);
    bool are_equivalent_types(QualifiedTypeInfo first, QualifiedTypeInfo second);

    bool match_ident_tuple_with_type(IdentifierTuple::TupleType &tuple, TupleType &type);
    void copy_types_into_vartuple(IdentifierTuple::TupleType &tuple, TupleType &type);
    void add_vartuple_to_stack(IdentifierTuple::TupleType &tuple);

    void begin_scope();
    void end_scope();
    friend class ScopedScopeManager;

    ExprVisitorType resolve(Expr *expr);
    StmtVisitorType resolve(Stmt *stmt);
    BaseTypeVisitorType resolve(BaseType *type);

  public:
    explicit TypeResolver(Module &module);

    void check(std::vector<StmtNode> &program);

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