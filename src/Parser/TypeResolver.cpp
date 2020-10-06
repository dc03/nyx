/* See LICENSE at project root for license details */
#include "TypeResolver.hpp"

TypeResolver::TypeResolver(const std::vector<ClassStmt *> &classes, const std::vector<FunctionStmt *> &functions,
                           const std::vector<VarStmt *> &globals) : classes{classes}, functions{functions},
                           globals{globals} {}

T TypeResolver::visit(AssignExpr& expr) {

}

T TypeResolver::visit(BinaryExpr& expr) {

}

T TypeResolver::visit(CallExpr& expr) {

}

T TypeResolver::visit(CommaExpr& expr) {

}

T TypeResolver::visit(GetExpr& expr) {

}

T TypeResolver::visit(GroupingExpr& expr) {

}

T TypeResolver::visit(IndexExpr& expr) {

}

T TypeResolver::visit(LiteralExpr& expr) {

}

T TypeResolver::visit(LogicalExpr& expr) {

}

T TypeResolver::visit(SetExpr& expr) {

}

T TypeResolver::visit(SuperExpr& expr) {

}

T TypeResolver::visit(TernaryExpr& expr) {

}

T TypeResolver::visit(ThisExpr& expr) {

}

T TypeResolver::visit(UnaryExpr& expr) {

}

T TypeResolver::visit(VariableExpr& expr) {

}


T TypeResolver::visit(BlockStmt& stmt) {

}

T TypeResolver::visit(BreakStmt& stmt) {

}

T TypeResolver::visit(ClassStmt& stmt) {

}

T TypeResolver::visit(ContinueStmt& stmt) {

}

T TypeResolver::visit(ExpressionStmt& stmt) {

}

T TypeResolver::visit(FunctionStmt& stmt) {

}

T TypeResolver::visit(IfStmt& stmt) {

}

T TypeResolver::visit(ImportStmt& stmt) {

}

T TypeResolver::visit(ReturnStmt& stmt) {

}

T TypeResolver::visit(SwitchStmt& stmt) {

}

T TypeResolver::visit(TypeStmt& stmt) {

}

T TypeResolver::visit(VarStmt& stmt) {

}

T TypeResolver::visit(WhileStmt& stmt) {

}


T TypeResolver::visit(PrimitiveType& type) {

}

T TypeResolver::visit(UserDefinedType& type) {

}

T TypeResolver::visit(ListType& type) {

}

T TypeResolver::visit(TypeofType& type) {

}
