/* See LICENSE at project root for license details */
#include "CodeGen.hpp"

RuntimeModule Generator::compile_module(Module &module, Chunk &chunk) {
    RuntimeModule compiled{};
    current_chunk = &chunk;
    current_module = &module;
    return compiled;
}

ExprVisitorType Generator::compile(Expr *expr) {
    return expr->accept(*this);
}

StmtVisitorType Generator::compile(Stmt *stmt) {
    stmt->accept(*this);
}

BaseTypeVisitorType Generator::compile(BaseType *type) {
    return type->accept(*this);
}

ExprVisitorType Generator::visit(AccessExpr &expr) {
    return {};
}

ExprVisitorType Generator::visit(AssignExpr &expr) {
    return {};
}

ExprVisitorType Generator::visit(BinaryExpr &expr) {
    return {};
}

ExprVisitorType Generator::visit(CallExpr &expr) {
    return {};
}

ExprVisitorType Generator::visit(CommaExpr &expr) {
    return {};
}

ExprVisitorType Generator::visit(GetExpr &expr) {
    return {};
}

ExprVisitorType Generator::visit(GroupingExpr &expr) {
    return {};
}

ExprVisitorType Generator::visit(IndexExpr &expr) {
    return {};
}

ExprVisitorType Generator::visit(LiteralExpr &expr) {
    current_chunk->emit_constant({expr.value.value});
    return {};
}

ExprVisitorType Generator::visit(LogicalExpr &expr) {
    return {};
}

ExprVisitorType Generator::visit(SetExpr &expr) {
    return {};
}

ExprVisitorType Generator::visit(SuperExpr &expr) {
    return {};
}

ExprVisitorType Generator::visit(TernaryExpr &expr) {
    return {};
}

ExprVisitorType Generator::visit(ThisExpr &expr) {
    return {};
}

ExprVisitorType Generator::visit(UnaryExpr &expr) {
    return {};
}

ExprVisitorType Generator::visit(VariableExpr &expr) {
    return {};
}


StmtVisitorType Generator::visit(BlockStmt &stmt) {

}

StmtVisitorType Generator::visit(BreakStmt &stmt) {

}

StmtVisitorType Generator::visit(ClassStmt &stmt) {

}

StmtVisitorType Generator::visit(ContinueStmt &stmt) {

}

StmtVisitorType Generator::visit(ExpressionStmt &stmt) {

}

StmtVisitorType Generator::visit(FunctionStmt &stmt) {

}

StmtVisitorType Generator::visit(IfStmt &stmt) {

}

StmtVisitorType Generator::visit(ReturnStmt &stmt) {

}

StmtVisitorType Generator::visit(SwitchStmt &stmt) {

}

StmtVisitorType Generator::visit(TypeStmt &stmt) {

}

StmtVisitorType Generator::visit(VarStmt &stmt) {

}

StmtVisitorType Generator::visit(WhileStmt &stmt) {

}


BaseTypeVisitorType Generator::visit(PrimitiveType &type) {
    return {};
}

BaseTypeVisitorType Generator::visit(UserDefinedType &type) {
    return {};
}

BaseTypeVisitorType Generator::visit(ListType &type) {
    return {};
}

BaseTypeVisitorType Generator::visit(TypeofType &type) {
    return {};
}
