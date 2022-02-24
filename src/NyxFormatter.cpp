/* Copyright (C) 2020-2022  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "nyx/NyxFormatter.hpp"

#include "nyx/CLIConfigParser.hpp"

#include <algorithm>

NyxFormatter::NyxFormatter(std::ostream &out_, FrontendContext *ctx_) : out{out_}, ctx{ctx_} {
    if (ctx->config->contains(TAB_SIZE)) {
        tab_size = std::stoull(ctx->config->get<std::string>(TAB_SIZE));
    } else if (ctx->config->contains(USE_TABS)) {
        use_tabs = true;
    }
}

std::ostream &NyxFormatter::print_indent(std::size_t tabs) {
    if (use_tabs) {
        for (std::size_t i = 0; i < tabs; i++) {
            out << '\t';
        }
    } else {
        for (std::size_t i = 0; i < tabs * tab_size; i++) {
            out << ' ';
        }
    }
    return out;
}

void NyxFormatter::print_vartuple(IdentifierTuple &tuple) {
    out << "{";
    std::size_t i = 1;
    for (auto &val : tuple.tuple) {
        if (val.index() == IdentifierTuple::IDENT_TUPLE) {
            print_vartuple(std::get<IdentifierTuple>(val));
        } else {
            out << std::get<Token>(std::get<IdentifierTuple::DeclarationDetails>(val)).lexeme;
        }
        if (i < tuple.tuple.size()) {
            out << ", ";
        }
        i++;
    }
    out << "}";
}

bool config_contains(const std::vector<std::string> &args, std::string_view val) {
    return std::find(args.begin(), args.end(), val) != args.end() ||
           std::find(args.begin(), args.end(), "all") != args.end();
}

void NyxFormatter::format(Module &module) {
    for (auto &stmt : module.statements) {
        if (stmt != nullptr) {
            format(stmt.get());
            out << '\n';
            if (stmt->type_tag() != NodeType::SingleLineCommentStmt &&
                stmt->type_tag() != NodeType::MultiLineCommentStmt) {
                out << '\n';
            }
        }
    }
}

ExprVisitorType NyxFormatter::format(Expr *expr) {
    return expr->accept(*this);
}

StmtVisitorType NyxFormatter::format(Stmt *stmt) {
    stmt->accept(*this);
}

BaseTypeVisitorType NyxFormatter::format(BaseType *type) {
    return type->accept(*this);
}

ExprVisitorType NyxFormatter::visit(AssignExpr &expr) {
    out << expr.target.lexeme << " = ";
    format(expr.value.get());
    return {};
}

ExprVisitorType NyxFormatter::visit(BinaryExpr &expr) {
    format(expr.left.get());
    out << ' ' << expr.synthesized_attrs.token.lexeme << ' ';
    format(expr.right.get());
    return {};
}

ExprVisitorType NyxFormatter::visit(CallExpr &expr) {
    format(expr.function.get());
    out << '(';
    if (not expr.args.empty()) {
        for (std::size_t i = 0; i < expr.args.size() - 1; i++) {
            format(std::get<ExprNode>(expr.args[i]).get());
            out << ", ";
        }
        format(std::get<ExprNode>(expr.args.back()).get());
    }
    out << ')';
    return {};
}

ExprVisitorType NyxFormatter::visit(CommaExpr &expr) {
    for (std::size_t i = 0; i < expr.exprs.size() - 1; i++) {
        format(expr.exprs[i].get());
        out << ", ";
    }
    format(expr.exprs.back().get());
    return {};
}

ExprVisitorType NyxFormatter::visit(GetExpr &expr) {
    format(expr.object.get());
    out << "." << expr.name.lexeme;
    return {};
}

ExprVisitorType NyxFormatter::visit(GroupingExpr &expr) {
    out << '(';
    format(expr.expr.get());
    out << ')';
    return {};
}

ExprVisitorType NyxFormatter::visit(IndexExpr &expr) {
    format(expr.object.get());
    out << "[";
    format(expr.index.get());
    out << "]";
    return {};
}

ExprVisitorType NyxFormatter::visit(ListExpr &expr) {
    out << "[";
    if (not expr.elements.empty()) {
        for (std::size_t i = 0; i < expr.elements.size() - 1; i++) {
            format(std::get<ExprNode>(expr.elements[i]).get());
            out << ", ";
        }
        format(std::get<ExprNode>(expr.elements.back()).get());
    }
    out << "]";
    return {};
}

ExprVisitorType NyxFormatter::visit(ListAssignExpr &expr) {
    format(&expr.list);
    out << " = ";
    format(expr.value.get());
    return {};
}

ExprVisitorType NyxFormatter::visit(ListRepeatExpr &expr) {
    out << "[";
    format(std::get<ExprNode>(expr.expr).get());
    out << "; ";
    format(std::get<ExprNode>(expr.quantity).get());
    out << "]";
    return {};
}

ExprVisitorType NyxFormatter::visit(LiteralExpr &expr) {
    if (expr.value.is_string()) {
        using namespace std::string_literals;
        std::string string_value{expr.value.to_string()};
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
        out << "\""s + result + "\""s;
    } else {
        out << expr.synthesized_attrs.token.lexeme;
    }
    return {};
}

ExprVisitorType NyxFormatter::visit(LogicalExpr &expr) {
    format(expr.left.get());
    out << " " << expr.synthesized_attrs.token.lexeme << " ";
    format(expr.right.get());
    return {};
}

ExprVisitorType NyxFormatter::visit(MoveExpr &expr) {
    out << expr.synthesized_attrs.token.lexeme;
    format(expr.expr.get());
    return {};
}

ExprVisitorType NyxFormatter::visit(ScopeAccessExpr &expr) {
    format(expr.scope.get());
    out << "::" << expr.name.lexeme;
    return {};
}

ExprVisitorType NyxFormatter::visit(ScopeNameExpr &expr) {
    out << expr.name.lexeme;
    return {};
}

ExprVisitorType NyxFormatter::visit(SetExpr &expr) {
    format(expr.object.get());
    out << "." << expr.name.lexeme << " = ";
    format(expr.value.get());
    return {};
}

ExprVisitorType NyxFormatter::visit(SuperExpr &expr) {
    out << "super";
    return {};
}

ExprVisitorType NyxFormatter::visit(TernaryExpr &expr) {
    format(expr.left.get());
    out << " ? ";
    format(expr.middle.get());
    out << " : ";
    format(expr.right.get());
    return {};
}

ExprVisitorType NyxFormatter::visit(ThisExpr &expr) {
    out << "this";
    return {};
}

ExprVisitorType NyxFormatter::visit(TupleExpr &expr) {
    out << "{";
    if (not expr.elements.empty()) {
        for (std::size_t i = 0; i < expr.elements.size() - 1; i++) {
            format(std::get<ExprNode>(expr.elements[i]).get());
            out << ", ";
        }
        format(std::get<ExprNode>(expr.elements.back()).get());
    }
    out << "}";
    return {};
}

ExprVisitorType NyxFormatter::visit(UnaryExpr &expr) {
    out << expr.oper.lexeme << ' ';
    format(expr.right.get());
    return {};
}

ExprVisitorType NyxFormatter::visit(VariableExpr &expr) {
    out << expr.name.lexeme;
    return {};
}

StmtVisitorType NyxFormatter::visit(BlockStmt &stmt) {
    if (stmt.stmts.size() == 1 && ctx->config->contains(COLLAPSE_SINGLE_LINE_BLOCK)) {
        out << "{ ";
        format(stmt.stmts[0].get());
        out << "; }";
    } else {
        out << "{\n";
        indent++;
        for (auto &s : stmt.stmts) {
            print_indent(indent);
            format(s.get());
            out << '\n';
        }
        indent--;
        print_indent(indent) << "}";
    }
}

StmtVisitorType NyxFormatter::visit(BreakStmt &stmt) {
    out << "break";
}

StmtVisitorType NyxFormatter::visit(ClassStmt &stmt) {
    out << "class " << stmt.name.lexeme;
    if (ctx->config->contains(BRACE_NEXT_LINE) &&
        config_contains(ctx->config->get<std::vector<std::string>>(BRACE_NEXT_LINE), "class")) {
        out << '\n';
        print_indent(indent);
    } else {
        out << ' ';
    }
    out << "{\n";
    indent++;
    for (auto &member : stmt.members) {
        print_indent(indent);
        if (member.second == VisibilityType::PUBLIC) {
            out << "public ";
        } else if (member.second == VisibilityType::PRIVATE) {
            out << "private ";
        } else if (member.second == VisibilityType::PROTECTED) {
            out << "protected ";
        }
        format(member.first);
        out << ";\n";
    }

    if (not stmt.methods.empty() || not stmt.members.empty()) {
        out << '\n';
    }

    for (auto &method : stmt.methods) {
        print_indent(indent);
        if (method.second == VisibilityType::PUBLIC) {
            out << "public ";
        } else if (method.second == VisibilityType::PRIVATE) {
            out << "private ";
        } else if (method.second == VisibilityType::PROTECTED) {
            out << "protected ";
        }
        format(method.first);
        out << "\n\n";
    }
    indent--;
    out << "}";
}

StmtVisitorType NyxFormatter::visit(ContinueStmt &stmt) {
    out << "continue";
}

StmtVisitorType NyxFormatter::visit(ExpressionStmt &stmt) {
    format(stmt.expr.get());
}

StmtVisitorType NyxFormatter::visit(ForStmt &stmt) {
    out << "for (";
    if (stmt.initializer != nullptr) {
        format(stmt.initializer.get());
    }
    out << ';';
    if (stmt.condition != nullptr) {
        out << ' ';
        format(stmt.condition.get());
    }
    out << ';';
    if (stmt.increment != nullptr) {
        out << ' ';
        format(stmt.increment.get());
    }
    out << ')';
    if (stmt.body->type_tag() == NodeType::BlockStmt) {
        if (ctx->config->contains(BRACE_NEXT_LINE) &&
            config_contains(ctx->config->get<std::vector<std::string>>(BRACE_NEXT_LINE), "for")) {
            out << '\n';
            print_indent(indent);
        } else {
            out << " ";
        }
        format(stmt.body.get());
    } else {
        out << '\n';
        indent++;
        print_indent(indent);
        format(stmt.body.get());
        indent--;
    }
}

StmtVisitorType NyxFormatter::visit(FunctionStmt &stmt) {
    out << "fn " << stmt.name.lexeme << "(";
    for (auto &param : stmt.params) {
        if (param.first.index() == FunctionStmt::IDENT_TUPLE) {
            auto &tup = std::get<IdentifierTuple>(param.first);
            print_vartuple(tup);
        } else {
            auto &tok = std::get<Token>(param.first);
            out << tok.lexeme << ": ";
        }
        format(param.second.get());
    }
    out << ") -> ";
    format(stmt.return_type.get());
    if (ctx->config->contains(BRACE_NEXT_LINE) &&
        config_contains(ctx->config->get<std::vector<std::string>>(BRACE_NEXT_LINE), "function")) {
        out << '\n';
        print_indent(indent);
    } else {
        out << " ";
    }
    format(stmt.body.get());
}

StmtVisitorType NyxFormatter::visit(IfStmt &stmt) {
    out << "if ";
    format(stmt.condition.get());
    if (stmt.thenBranch->type_tag() == NodeType::BlockStmt) {
        if (ctx->config->contains(BRACE_NEXT_LINE) &&
            config_contains(ctx->config->get<std::vector<std::string>>(BRACE_NEXT_LINE), "if")) {
            out << '\n';
            print_indent(indent);
        } else {
            out << " ";
        }
        format(stmt.thenBranch.get());
    } else {
        out << '\n';
        indent++;
        print_indent(indent);
        format(stmt.thenBranch.get());
        out << ';';
        indent--;
    }
    if (stmt.elseBranch != nullptr) {
        if (stmt.thenBranch->type_tag() == NodeType::BlockStmt) {
            out << " else";
        } else {
            out << '\n';
            print_indent(indent) << "else";
        }
        if (stmt.elseBranch->type_tag() == NodeType::IfStmt) {
            out << ' ';
        } else if (stmt.elseBranch->type_tag() == NodeType::BlockStmt) {
            if (ctx->config->contains(BRACE_NEXT_LINE) &&
                config_contains(ctx->config->get<std::vector<std::string>>(BRACE_NEXT_LINE), "if")) {
                out << '\n';
                print_indent(indent);
            } else {
                out << " ";
            }
        } else {
            out << '\n';
        }
        format(stmt.thenBranch.get());
    }
}

StmtVisitorType NyxFormatter::visit(ReturnStmt &stmt) {
    out << "return";
    if (stmt.value != nullptr) {
        out << " ";
        format(stmt.value.get());
    }
}

StmtVisitorType NyxFormatter::visit(SwitchStmt &stmt) {
    out << "switch ";
    format(stmt.condition.get());
    if (ctx->config->contains(BRACE_NEXT_LINE) &&
        config_contains(ctx->config->get<std::vector<std::string>>(BRACE_NEXT_LINE), "switch")) {
        out << '\n';
        print_indent(indent);
    } else {
        out << " ";
    }
    out << "{\n";
    indent++;
    for (auto &case_ : stmt.cases) {
        print_indent(indent);
        format(case_.first.get());
        out << " -> ";
        format(case_.second.get());
        out << ";\n";
    }
    if (stmt.default_case != nullptr) {
        print_indent(indent) << "default -> ";
        format(stmt.default_case.get());
        out << ";\n";
    }
    indent--;
    print_indent(indent);
    out << "}";
}

StmtVisitorType NyxFormatter::visit(TypeStmt &stmt) {
    out << "type " << stmt.name.lexeme << " = ";
    format(stmt.type.get());
}

StmtVisitorType NyxFormatter::visit(VarStmt &stmt) {
    switch (stmt.keyword.type) {
        case TokenType::VAR: out << "var "; break;
        case TokenType::REF: out << "ref "; break;
        case TokenType::CONST: out << "const "; break;
        default: break;
    }
    out << stmt.name.lexeme;
    if (not stmt.originally_typeless) {
        out << ": ";
        format(stmt.type.get());
    }
    out << " = ";
    format(stmt.initializer.get());
}

StmtVisitorType NyxFormatter::visit(VarTupleStmt &stmt) {
    switch (stmt.keyword.type) {
        case TokenType::VAR: out << "var "; break;
        case TokenType::REF: out << "ref "; break;
        case TokenType::CONST: out << "const "; break;
        default: break;
    }
    print_vartuple(stmt.names);
    if (not stmt.originally_typeless) {
        out << ": ";
        format(stmt.type.get());
    }
    out << " = ";
    format(stmt.initializer.get());
    if (stmt.initializer->type_tag() == NodeType::TupleExpr) {
        out << ";";
    }
}

StmtVisitorType NyxFormatter::visit(WhileStmt &stmt) {
    out << "while ";
    format(stmt.condition.get());
    if (ctx->config->contains(BRACE_NEXT_LINE) &&
        config_contains(ctx->config->get<std::vector<std::string>>(BRACE_NEXT_LINE), "while")) {
        out << '\n';
        print_indent(indent);
    } else {
        out << " ";
    }
    if (stmt.increment != nullptr) {
        out << "{\n";
        indent++;
    }
    if (stmt.body->type_tag() == NodeType::BlockStmt) {
        if (stmt.increment == nullptr) {
            out << ' ';
        }
    } else {
        out << '\n';
    }
    if (stmt.increment != nullptr) {
        print_indent(indent);
    }
    format(stmt.body.get());
    if (stmt.increment != nullptr) {
        out << '\n';
        print_indent(indent);
        format(stmt.increment.get());
        indent--;
        out << '\n';
        print_indent(indent) << '}';
    }
}

StmtVisitorType NyxFormatter::visit(SingleLineCommentStmt &stmt) {
    out << stmt.contents.lexeme;
}

StmtVisitorType NyxFormatter::visit(MultiLineCommentStmt &stmt) {
    out << stmt.contents.lexeme;
}

BaseTypeVisitorType NyxFormatter::visit(PrimitiveType &basetype) {
    if (basetype.is_const) {
        out << "const ";
    }
    if (basetype.is_ref) {
        out << "ref ";
    }
    switch (basetype.primitive) {
        case Type::BOOL: out << "bool"; break;
        case Type::INT: out << "int"; break;
        case Type::FLOAT: out << "float"; break;
        case Type::STRING: out << "string"; break;
        case Type::NULL_: out << "null"; break;
        default: break;
    }
    return {};
}

BaseTypeVisitorType NyxFormatter::visit(UserDefinedType &basetype) {
    if (basetype.is_const) {
        out << "const ";
    }
    if (basetype.is_ref) {
        out << "ref ";
    }
    out << basetype.name.lexeme;
    return {};
}

BaseTypeVisitorType NyxFormatter::visit(ListType &basetype) {
    if (basetype.is_const) {
        out << "const ";
    }
    if (basetype.is_ref) {
        out << "ref ";
    }
    out << "[";
    format(basetype.contained.get());
    out << "]";
    return {};
}

BaseTypeVisitorType NyxFormatter::visit(TupleType &basetype) {
    if (basetype.is_const) {
        out << "const ";
    }
    if (basetype.is_ref) {
        out << "ref ";
    }
    out << "{";
    if (not basetype.types.empty()) {
        for (std::size_t i = 0; i < basetype.types.size() - 1; i++) {
            format(basetype.types[i].get());
            out << ", ";
        }
        format(basetype.types.back().get());
    }
    out << "}";
    return {};
}

BaseTypeVisitorType NyxFormatter::visit(TypeofType &basetype) {
    if (basetype.is_const) {
        out << "const ";
    }
    if (basetype.is_ref) {
        out << "ref ";
    }
    out << "typeof ";
    format(basetype.expr.get());
    return {};
}
