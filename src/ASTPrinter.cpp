/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "ASTPrinter.hpp"

#include <algorithm>
#include <iostream>

void print_tabs(std::size_t num) {
    for (std::size_t i = 0; i < num; i++) {
        std::cout << "|  ";
    }
}

std::ostream &print_ident_type(IdentifierType type) {
    if (type == IdentifierType::LOCAL) {
        std::cout << "local";
    } else if (type == IdentifierType::GLOBAL) {
        std::cout << "global";
    } else if (type == IdentifierType::FUNCTION) {
        std::cout << "function";
    } else if (type == IdentifierType::CLASS) {
        std::cout << "class";
    }
    return std::cout;
}

std::ostream &print_conversion_type(NumericConversionType type) {
    if (type == NumericConversionType::INT_TO_FLOAT) {
        std::cout << "int->float";
    } else if (type == NumericConversionType::FLOAT_TO_INT) {
        std::cout << "float->int";
    } else if (type == NumericConversionType::NONE) {
        std::cout << "none";
    }
    return std::cout;
}

std::ostream &print_type(Type type) {
    switch (type) {
        case Type::BOOL: std::cout << "bool"; break;
        case Type::INT: std::cout << "int"; break;
        case Type::FLOAT: std::cout << "float"; break;
        case Type::STRING: std::cout << "string"; break;
        case Type::CLASS: std::cout << "class"; break;
        case Type::LIST: std::cout << "list"; break;
        case Type::TYPEOF: break;
        case Type::NULL_: std::cout << "null"; break;
        case Type::FUNCTION: std::cout << "function"; break;
        case Type::MODULE: std::cout << "module"; break;
        case Type::TUPLE: std::cout << "tuple"; break;
    }
    return std::cout;
}

std::ostream &print_access_specifier(VisibilityType visibility) {
    switch (visibility) {
        case VisibilityType::PRIVATE: std::cout << "private"; break;
        case VisibilityType::PROTECTED: std::cout << "protected"; break;
        case VisibilityType::PUBLIC: std::cout << "public"; break;
    }
    return std::cout;
}

std::string escape(const std::string &string_value) {
    std::string result{};
    auto is_escape = [](char ch) {
        switch (ch) {
            case '\b':
            case '\n':
            case '\r':
            case '\t':
            case '\'':
            case '\"':
            case '\\': return true;
            default: return false;
        }
    };
    auto repr_escape = [](char ch) {
        switch (ch) {
            case '\b': return "\\b";
            case '\n': return "\\n";
            case '\r': return "\\r";
            case '\t': return "\\t";
            case '\'': return "\\\'";
            case '\"': return "\\\"";
            case '\\': return "\\\\";
            default: return "";
        }
    };
    result.reserve(string_value.size() + std::count_if(string_value.begin(), string_value.end(), is_escape));
    for (char ch : string_value) {
        if (is_escape(ch)) {
            result += repr_escape(ch);
        } else {
            result += ch;
        }
    }
    return result;
}

std::ostream &print_token(const Token &token) {
    return std::cout << "\"" << escape(token.lexeme) << "\" Line:" << token.line << "::Bytes:" << token.start << ".."
                     << token.end;
}

void ASTPrinter::print_stmt(StmtNode &stmt) {
    print(stmt.get());
}

void ASTPrinter::print_stmts(std::vector<StmtNode> &stmts) {
    for (auto &stmt : stmts) {
        if (stmt != nullptr) {
            print_stmt(stmt);
            std::cout << '\n';
        }
    }
}

ExprVisitorType ASTPrinter::print(Expr *expr) {
    print_tabs(current_depth);
    std::cout << expr->string_tag() << '\n';
    return expr->accept(*this);
}

StmtVisitorType ASTPrinter::print(Stmt *stmt) {
    print_tabs(current_depth);
    std::cout << stmt->string_tag() << '\n';
    return stmt->accept(*this);
}

BaseTypeVisitorType ASTPrinter::print(BaseType *type) {
    print_tabs(current_depth);
    std::cout << type->string_tag() << '\n';
    print_tabs(current_depth);
    print_type(type->primitive) << std::boolalpha << "::Const:" << type->is_const << "::Ref:" << type->is_ref << '\n';
    return type->accept(*this);
}

ExprVisitorType ASTPrinter::visit(AssignExpr &expr) {
    print_tabs(current_depth);
    print_token(expr.resolved.token) << std::boolalpha << "::Copy:" << expr.requires_copy << "::Conv:";
    print_conversion_type(expr.conversion_type);
    std::cout << '\n';
    current_depth++;
    print_tabs(current_depth);
    std::cout << "Target: ";
    print_token(expr.target) << '\n';
    print_tabs(current_depth);
    std::cout << "^^^ assigned value vvv\n";
    print(expr.value.get());
    current_depth--;
    return {};
}

ExprVisitorType ASTPrinter::visit(BinaryExpr &expr) {
    print_tabs(current_depth);
    print_token(expr.resolved.token) << '\n';
    current_depth++;
    print(expr.left.get());
    print(expr.right.get());
    current_depth--;
    return {};
}

ExprVisitorType ASTPrinter::visit(CallExpr &expr) {
    print_tabs(current_depth);
    print_token(expr.resolved.token) << std::boolalpha << "::Native:" << expr.is_native_call << std::noboolalpha
                                     << '\n';
    current_depth++;
    if (not expr.is_native_call) {
        print(expr.function.get());
    }
    for (std::size_t i = 0; i < expr.args.size(); i++) {
        current_depth++;
        print_tabs(current_depth);
        std::cout << "Arg:(" << i + 1 << ")::Conv:";
        print_conversion_type(std::get<NumericConversionType>(expr.args[i]))
            << "::Copy:" << std::boolalpha << std::get<RequiresCopy>(expr.args[i]) << std::noboolalpha << '\n';
        print(std::get<ExprNode>(expr.args[i]).get());
        current_depth--;
    }
    current_depth--;
    return {};
}

ExprVisitorType ASTPrinter::visit(CommaExpr &expr) {
    current_depth++;
    for (auto &elem : expr.exprs) {
        print(elem.get());
    }
    current_depth--;
    return {};
}

ExprVisitorType ASTPrinter::visit(GetExpr &expr) {
    current_depth++;
    print(expr.object.get());
    print_tabs(current_depth);
    print_token(expr.name) << '\n';
    current_depth--;
    return {};
}

ExprVisitorType ASTPrinter::visit(GroupingExpr &expr) {
    current_depth++;
    print(expr.expr.get());
    current_depth--;
    return {};
}

ExprVisitorType ASTPrinter::visit(IndexExpr &expr) {
    print_tabs(current_depth);
    print_token(expr.resolved.token) << '\n';
    current_depth++;
    print(expr.object.get());
    print_tabs(current_depth);
    std::cout << "^^^ indexed by vvv\n";
    print(expr.index.get());
    current_depth--;
    return {};
}

ExprVisitorType ASTPrinter::visit(ListExpr &expr) {
    print_tabs(current_depth);
    print_token(expr.bracket) << '\n';
    current_depth++;
    for (std::size_t i = 0; i < expr.elements.size(); i++) {
        current_depth++;
        print_tabs(current_depth);
        std::cout << "Arg:(" << i + 1 << ")::Conv:";
        print_conversion_type(std::get<NumericConversionType>(expr.elements[i]))
            << "::Copy:" << std::boolalpha << std::get<RequiresCopy>(expr.elements[i]) << std::noboolalpha << '\n';
        print(std::get<ExprNode>(expr.elements[i]).get());
        current_depth--;
    }
    current_depth--;
    return {};
}

ExprVisitorType ASTPrinter::visit(ListAssignExpr &expr) {
    print_tabs(current_depth);
    print_token(expr.resolved.token) << std::boolalpha << "::Copy:" << expr.requires_copy << std::noboolalpha
                                     << "::Conv:";
    print_conversion_type(expr.conversion_type) << '\n';
    current_depth++;
    print(&expr.list);
    print_tabs(current_depth);
    std::cout << "^^^ assigned value vvv\n";
    print(expr.value.get());
    current_depth--;
    return {};
}

ExprVisitorType ASTPrinter::visit(LiteralExpr &expr) {
    print_tabs(current_depth);
    print_token(expr.resolved.token) << "::Idx:" << expr.value.index() << '\n';
    return {};
}

ExprVisitorType ASTPrinter::visit(LogicalExpr &expr) {
    print_tabs(current_depth);
    print_token(expr.resolved.token) << '\n';
    current_depth++;
    print(expr.left.get());
    print(expr.right.get());
    current_depth--;
    return {};
}

ExprVisitorType ASTPrinter::visit(MoveExpr &expr) {
    print_tabs(current_depth);
    print_token(expr.resolved.token) << '\n';
    current_depth++;
    print(expr.expr.get());
    current_depth--;
    return {};
}

ExprVisitorType ASTPrinter::visit(ScopeAccessExpr &expr) {
    current_depth++;
    print(expr.scope.get());
    print_tabs(current_depth);
    std::cout << "^^^ accessing vvv\n";
    print_tabs(current_depth);
    print_token(expr.name) << '\n';
    current_depth--;
    return {};
}

ExprVisitorType ASTPrinter::visit(ScopeNameExpr &expr) {
    print_tabs(current_depth);
    print_token(expr.name) << '\n';
    return {};
}

ExprVisitorType ASTPrinter::visit(SetExpr &expr) {
    current_depth++;
    print(expr.object.get());
    print_tabs(current_depth);
    std::cout << "Accessing:\n";
    print_tabs(current_depth);
    print_token(expr.name) << '\n';
    print_tabs(current_depth);
    std::cout << "^^^ assigning value vvv\n";
    print(expr.value.get());
    current_depth--;
    return {};
}

ExprVisitorType ASTPrinter::visit(SuperExpr &expr) {
    print_tabs(current_depth);
    print_token(expr.keyword) << '\n';
    print_tabs(current_depth);
    print_token(expr.name) << '\n';
    return {};
}

ExprVisitorType ASTPrinter::visit(TernaryExpr &expr) {
    print_tabs(current_depth);
    print_token(expr.resolved.token) << '\n';
    current_depth++;
    print(expr.left.get());
    print_tabs(current_depth);
    std::cout << "--> ?\n";
    print(expr.middle.get());
    print_tabs(current_depth);
    std::cout << "--> :\n";
    print(expr.right.get());
    current_depth--;
    return {};
}

ExprVisitorType ASTPrinter::visit(ThisExpr &expr) {
    print_tabs(current_depth);
    print_token(expr.keyword) << '\n';
    return {};
}

ExprVisitorType ASTPrinter::visit(TupleExpr &expr) {
    print_tabs(current_depth);
    print_token(expr.brace) << '\n';
    current_depth++;
    current_depth++;
    for (std::size_t i = 0; i < expr.elements.size(); i++) {
        print_tabs(current_depth);
        std::cout << "Element:(" << i + 1 << ")::Conv:";
        print_conversion_type(std::get<NumericConversionType>(expr.elements[i]))
            << "::Copy:" << std::boolalpha << std::get<RequiresCopy>(expr.elements[i]) << std::noboolalpha << '\n';
        print(std::get<ExprNode>(expr.elements[i]).get());
    }
    current_depth--;
    print(expr.type.get());
    current_depth--;
    return {};
}

ExprVisitorType ASTPrinter::visit(UnaryExpr &expr) {
    print_tabs(current_depth);
    print_token(expr.oper) << '\n';
    current_depth++;
    print(expr.right.get());
    current_depth--;
    return {};
}

ExprVisitorType ASTPrinter::visit(VariableExpr &expr) {
    print_tabs(current_depth);
    print_token(expr.name) << "::Type:";
    print_ident_type(expr.type) << '\n';
    return {};
}

StmtVisitorType ASTPrinter::visit(BlockStmt &stmt) {
    current_depth++;
    for (auto &elem : stmt.stmts) {
        print_stmt(elem);
    }
    current_depth--;
}

StmtVisitorType ASTPrinter::visit(BreakStmt &stmt) {
    print_tabs(current_depth);
    print_token(stmt.keyword) << '\n';
}

StmtVisitorType ASTPrinter::visit(ClassStmt &stmt) {
    print_tabs(current_depth);
    print_token(stmt.name) << '\n';
    current_depth++;
    for (auto &member : stmt.members) {
        print_tabs(current_depth);
        print_access_specifier(member.second) << '\n';
        print(member.first.get());
    }
    for (auto &method : stmt.methods) {
        print_tabs(current_depth);
        print_access_specifier(method.second) << '\n';
        print(method.first.get());
    }
    current_depth--;
}

StmtVisitorType ASTPrinter::visit(ContinueStmt &stmt) {
    print_tabs(current_depth);
    print_token(stmt.keyword) << '\n';
}

StmtVisitorType ASTPrinter::visit(ExpressionStmt &stmt) {
    current_depth++;
    print(stmt.expr.get());
    current_depth--;
}

StmtVisitorType ASTPrinter::visit(FunctionStmt &stmt) {
    print_tabs(current_depth);
    print_token(stmt.name) << '\n';
    current_depth++;
    if (stmt.return_type != nullptr) {
        print_tabs(current_depth);
        std::cout << "Return type:\n";
        current_depth++;
        print(stmt.return_type.get());
        current_depth--;
    }
    for (std::size_t i = 0; i < stmt.params.size(); i++) {
        print_tabs(current_depth);
        std::cout << "Param:(" << i + 1 << ")\n";
        current_depth++;
        print_tabs(current_depth);
        print_token(stmt.params[i].first) << '\n';
        print(stmt.params[i].second.get());
        current_depth--;
    }
    print(stmt.body.get());
    current_depth--;
}

StmtVisitorType ASTPrinter::visit(IfStmt &stmt) {
    print_tabs(current_depth);
    print_token(stmt.keyword) << std::boolalpha << "::HasElse:" << (stmt.elseBranch != nullptr) << std::noboolalpha
                              << '\n';
    current_depth++;
    print_tabs(current_depth);
    std::cout << "Condition:\n";
    print(stmt.condition.get());
    print_tabs(current_depth);
    std::cout << "Body:\n";
    print(stmt.thenBranch.get());
    if (stmt.elseBranch != nullptr) {
        print_tabs(current_depth);
        std::cout << "Else branch:\n";
        print(stmt.elseBranch.get());
    }
    current_depth--;
}

StmtVisitorType ASTPrinter::visit(ReturnStmt &stmt) {
    print_tabs(current_depth);
    print_token(stmt.keyword) << "::Popped:" << stmt.locals_popped << '\n';
    if (stmt.value != nullptr) {
        current_depth++;
        print(stmt.value.get());
        current_depth--;
    }
}

StmtVisitorType ASTPrinter::visit(SwitchStmt &stmt) {
    current_depth++;
    print_tabs(current_depth);
    std::cout << "Condition:\n";
    print(stmt.condition.get());
    for (std::size_t i = 0; i < stmt.cases.size(); i++) {
        print_tabs(current_depth);
        std::cout << "Case:(" << i + 1 << ")\n";
        current_depth++;
        print_tabs(current_depth);
        std::cout << "Condition:\n";
        print(stmt.cases[i].first.get());
        print_tabs(current_depth);
        std::cout << "Body:\n";
        print(stmt.cases[i].second.get());
        current_depth--;
    }
    current_depth--;
}

StmtVisitorType ASTPrinter::visit(TypeStmt &stmt) {
    print_tabs(current_depth);
    print_token(stmt.name) << '\n';
    current_depth++;
    print_tabs(current_depth);
    print(stmt.type.get());
    current_depth--;
}

void ASTPrinter::print_variable(Token &name, NumericConversionType conv, RequiresCopy copy, TypeNode &type) {
    print_tabs(current_depth);
    print_token(name) << "::Conv:";
    print_conversion_type(conv) << "::Copy:" << std::boolalpha << copy << std::noboolalpha << '\n';
    current_depth++;
    if (type != nullptr) {
        print_tabs(current_depth);
        std::cout << "^^^ type vvv\n";
        print(type.get());
    }
    current_depth--;
}

void ASTPrinter::print_ident_tuple(IdentifierTuple &tuple) {
    current_depth++;
    print_tabs(current_depth);
    std::cout << "Begin IdentifierTuple\n";
    for (auto &elem : tuple.tuple) {
        if (elem.index() == IdentifierTuple::IDENT_TUPLE) {
            print_ident_tuple(std::get<IdentifierTuple>(elem));
        } else {
            current_depth++;
            auto &var = std::get<IdentifierTuple::DeclarationDetails>(elem);
            print_variable(std::get<Token>(var), std::get<NumericConversionType>(var), std::get<RequiresCopy>(var),
                std::get<TypeNode>(var));
            current_depth--;
        }
    }
    print_tabs(current_depth);
    std::cout << "End IdentifierTuple\n";
    current_depth--;
}

StmtVisitorType ASTPrinter::visit(VarStmt &stmt) {
    print_variable(stmt.name, stmt.conversion_type, stmt.requires_copy, stmt.type);
    current_depth++;
    print_tabs(current_depth);
    std::cout << "^^^ initializer vvv\n";
    if (stmt.initializer != nullptr) {
        print(stmt.initializer.get());
    } else {
        print_tabs(current_depth);
        std::cout << "Initializer: none\n";
    }
    current_depth--;
}

StmtVisitorType ASTPrinter::visit(VarTupleStmt &stmt) {
    print_ident_tuple(stmt.names);
    current_depth++;
    if (stmt.type != nullptr) {
        print_tabs(current_depth);
        std::cout << "^^^ type vvv\n";
        print(stmt.type.get());
    }
    print_tabs(current_depth);
    std::cout << "^^^ initializer vvv\n";
    if (stmt.initializer != nullptr) {
        print(stmt.initializer.get());
    } else {
        print_tabs(current_depth);
        std::cout << "Initializer: none\n";
    }
    current_depth--;
}

StmtVisitorType ASTPrinter::visit(WhileStmt &stmt) {
    print_tabs(current_depth);
    print_token(stmt.keyword) << '\n';
    current_depth++;
    print_tabs(current_depth);
    std::cout << "Condition:\n";
    print(stmt.condition.get());
    print_tabs(current_depth);
    std::cout << "Body:\n";
    print(stmt.body.get());
    current_depth--;
}

BaseTypeVisitorType ASTPrinter::visit(PrimitiveType &type) {
    return {};
}

BaseTypeVisitorType ASTPrinter::visit(UserDefinedType &type) {
    current_depth++;
    print_tabs(current_depth);
    print_token(type.name) << '\n';
    current_depth--;
    return {};
}

BaseTypeVisitorType ASTPrinter::visit(ListType &type) {
    current_depth++;
    print_tabs(current_depth);
    std::cout << "Contained:\n";
    print(type.contained.get());
    if (type.size != nullptr) {
        print_tabs(current_depth);
        std::cout << "^^^ size vvv\n";
        print(type.size.get());
    }
    current_depth--;
    return {};
}

BaseTypeVisitorType ASTPrinter::visit(TupleType &type) {
    current_depth++;
    print_tabs(current_depth);
    std::cout << "Contained:\n";
    current_depth++;
    for (std::size_t i = 0; i < type.types.size(); i++) {
        print(type.types[i].get());
    }
    current_depth--;
    current_depth--;
    return {};
}

BaseTypeVisitorType ASTPrinter::visit(TypeofType &type) {
    current_depth++;
    print_tabs(current_depth);
    std::cout << "Expression:\n";
    print(type.expr.get());
    current_depth--;
    return {};
}