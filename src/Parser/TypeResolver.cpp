/* See LICENSE at project root for license details */
#include <algorithm>
#include <array>
#include <stdexcept>
#include <string_view>

#include "../ErrorLogger/ErrorLogger.hpp"
#include "TypeResolver.hpp"

#define allocate(T, ...) new T{__VA_ARGS__}

struct TypeException : public std::runtime_error {
    explicit TypeException(std::string_view string): std::runtime_error{std::string{string}} {}
};

TypeResolver::TypeResolver(const std::vector<ClassStmt *> &classes, const std::vector<FunctionStmt *> &functions):
    classes{classes}, functions{functions} {}

////////////////////////////////////////////////////////////////////////////////

bool convertible_to(QualifiedTypeInfo to, QualifiedTypeInfo from, const Token &where) {
    if (to->data.is_ref) {
        if (from->data.is_const) {
            return from->data.type == to->data.type && from->data.is_const == to->data.is_const;
        } else {
            return from->data.type == to->data.type;
        }
    } else if (from->data.type == Type::FLOAT && to->data.type == Type::INT) {
        warning("Implicit conversion from float to int", where);
    } else {
        return from->data.type == to->data.type;
    }
    return false;
}

void TypeResolver::check(std::vector<stmt_node_t> &program) {
    for (auto &stmt : program) {
        resolve(stmt.get());
    }
}

void TypeResolver::begin_scope() {
    scope_depth++;
}

void TypeResolver::end_scope() {
    while (!values.empty() && values.back().scope_depth == scope_depth) {
        values.pop_back();
    }
    scope_depth--;
}

ExprVisitorType TypeResolver::resolve(Expr *expr) {
    return expr->accept(*this);
}

StmtVisitorType TypeResolver::resolve(Stmt *stmt) {
    return stmt->accept(*this);
}

BaseTypeVisitorType TypeResolver::resolve(BaseType *type) {
    return type->accept(*this);
}

template <typename T>
BaseType *TypeResolver::make_new_type(Type type, bool is_const, bool is_ref) {
    type_scratch_space.emplace_back(allocate(T, SharedData{type, is_const, is_ref}));
    return type_scratch_space.back().get();
}

ExprVisitorType TypeResolver::visit(AssignExpr& expr) {
    for (auto it = values.end(); it > values.begin(); it--) {
        if (it->lexeme == expr.target.lexeme) {
            ExprVisitorType value = resolve(expr.value.get());
            if (!convertible_to(it->info, value.info, expr.target)) {
                error("Cannot convert type of value to type of target", expr.target);
            }
            return {it->info, expr.target};
        }
    }

    error("No such variable in the current scope", expr.target);
    note("This error can lead to other random errors, fix this first");
    return {make_new_type<PrimitiveType>(Type::INT, true, false), expr.target};
}

template <typename ... Args>
bool one_of(Type type, Args ... args) {
    const std::array arr{args...};
    return std::any_of(arr.begin(), arr.end(), type);
}

ExprVisitorType TypeResolver::visit(BinaryExpr& expr) {
    ExprVisitorType left_expr = resolve(expr.left.get());
    ExprVisitorType right_expr = resolve(expr.right.get());
    switch (expr.oper.type) {
        case TokenType::LEFT_SHIFT:
        case TokenType::RIGHT_SHIFT:
        case TokenType::BIT_AND:
        case TokenType::BIT_OR:
        case TokenType::BIT_XOR:
        case TokenType::MODULO:
            if (left_expr.info->data.type != Type::INT || right_expr.info->data.type != Type::INT) {
                error("Wrong types of arguments to bitwise binary operators (expected integral arguments)",
                      expr.oper);
            }
            return {left_expr.info, expr.oper};
        case TokenType::NOT_EQUAL:
        case TokenType::EQUAL_EQUAL:
            if (one_of(left_expr.info->data.type, Type::BOOL, Type::STRING, Type::LIST, Type::NULL_)) {
                if (left_expr.info->data.type != right_expr.info->data.type) {
                    error("Cannot compare equality of objects of different types", expr.oper);
                }
                return {make_new_type<PrimitiveType>(Type::BOOL, true, false), expr.oper};
                // It may seem illogical to return the correct answer even for mis-matching types, but it prevents
                // propagation of random errors
            }
            [[fallthrough]];
        case TokenType::GREATER:
        case TokenType::GREATER_EQUAL:
        case TokenType::LESS:
        case TokenType::LESS_EQUAL:
            if (one_of(left_expr.info->data.type, Type::INT, Type::FLOAT) &&
                one_of(right_expr.info->data.type, Type::INT, Type::FLOAT)) {
                if (left_expr.info->data.type != right_expr.info->data.type) {
                    warning("Comparison between objects of types int and float", expr.oper);
                }
                return {make_new_type<PrimitiveType>(Type::BOOL, true, false), expr.oper};
            } else if (left_expr.info->data.type == Type::BOOL && right_expr.info->data.type == Type::BOOL) {
                return {make_new_type<PrimitiveType>(Type::BOOL, true, false), expr.oper};
            } else {
                error("Cannot compare objects of incompatible types", expr.oper);
                return {make_new_type<PrimitiveType>(Type::BOOL, true, false), expr.oper};
            }
        case TokenType::PLUS:
            if (left_expr.info->data.type == Type::STRING && right_expr.info->data.type == Type::STRING) {
                return {make_new_type<PrimitiveType>(Type::STRING, true, false), expr.oper};
            }
            [[fallthrough]];
        case TokenType::MINUS:
        case TokenType::SLASH:
        case TokenType::STAR:
            if (one_of(left_expr.info->data.type, Type::INT, Type::FLOAT) &&
                one_of(right_expr.info->data.type, Type::INT, Type::FLOAT)) {
                if (left_expr.info->data.type == Type::INT && right_expr.info->data.type == Type::INT) {
                    return {make_new_type<PrimitiveType>(Type::INT, true, false), expr.oper};
                }
                return {make_new_type<PrimitiveType>(Type::FLOAT, true, false), expr.oper};
                // Integral promotion
            } else {
                error("Cannot use arithmetic operators on objects of incompatible types", expr.oper);
                note("This error can lead to other warnings/errors that may be incorrect,"
                     " try to fix this error first");
                return {make_new_type<PrimitiveType>(Type::INT, true, false), expr.oper};
                // Returning int here (because it works with most operators) to prevent the type checker from freaking
                // out too much
            }

        default:
            error("Bug in parser with illegal token type of expression's operator", expr.oper);
            throw TypeException{"Bug in parser with illegal token type of expression's operator"};
    }
}

ExprVisitorType TypeResolver::visit(CallExpr& expr) {
    ExprTypeInfo callee = resolve(expr.function.get());
    if (callee.func == nullptr) {
        error("Expected function to be called in call expression", callee.lexeme);
        note("This error can lead to other warnings/errors that may be incorrect,"
             " try to fix this error first");
        return {make_new_type<PrimitiveType>(Type::INT, true, false), callee.lexeme};
    } else if (callee.func->params.size() != expr.args.size()) {
        error("Number of arguments passed to function must match the number of parameters", expr.paren);
        note("This error can lead to other warnings/errors that may be incorrect,"
             " try to fix this error first");
        return {make_new_type<PrimitiveType>(Type::INT, true, false), callee.lexeme};
    } else {
        for (std::size_t i{0}; i < expr.args.size(); i++) {
            ExprVisitorType argument = resolve(expr.args[i].get());
            if (!convertible_to(callee.func->params[i].second.get(), argument.info, argument.lexeme)) {
                error("Type of argument is not convertible to type of parameter", argument.lexeme);
            }
        }

        return {callee.func->return_type.get(), expr.paren};
    }
}

ExprVisitorType TypeResolver::visit(CommaExpr& expr) {
    for (std::size_t i{0}; i < expr.exprs.size(); i++) {
        if (i == expr.exprs.size() - 1) {
            return resolve(expr.exprs[i].get());
        }

        resolve(expr.exprs[i].get());
    }

    throw TypeException{"Unreachable"};
}

ExprVisitorType TypeResolver::visit(GetExpr& expr) {
    ExprTypeInfo object = resolve(expr.object.get());
    if (object.info->data.type == Type::LIST) {
        if (expr.name.lexeme != "size") {
            error("Can only get 'size' attribute of a list", expr.name);
            note("This error can lead to other warnings/errors that may be incorrect,"
                 " try to fix this error first");
            return {make_new_type<PrimitiveType>(Type::INT, true, false), expr.name};
        }
    } else if (object.info->data.type == Type::CLASS) {
        auto *type = dynamic_cast<UserDefinedType *>(object.info);
        for (ClassStmt *user_type : classes) {
            if (user_type->name.lexeme == type->name.lexeme) {
                for (auto &member_decl : user_type->members) {
                    auto *member = dynamic_cast<VarStmt*>(member_decl.first.get());
                    if (member->name.lexeme == expr.name.lexeme) {
                        if (in_class || (member_decl.second == VisibilityType::PUBLIC)) {
                            return {member->type.get(), expr.name};
                        }
                    }
                }
            }
        }

    } else {
        error("Expected class or list type to take attribute of", expr.name);
        note("This error can lead to other warnings/errors that may be incorrect,"
             " try to fix this error first");
        return {make_new_type<PrimitiveType>(Type::INT, true, false), expr.name};
    }

    throw TypeException{"Unreachable"};
}

ExprVisitorType TypeResolver::visit(GroupingExpr& expr) {
    return resolve(expr.expr.get());
}

ExprVisitorType TypeResolver::visit(IndexExpr& expr) {
    ExprVisitorType list = resolve(expr.object.get());

    if (list.info->data.type != Type::LIST) {
        error("Expected list type for indexing", expr.oper);
        note("This error can lead to other warnings/errors that may be incorrect,"
             " try to fix this error first");
        return {make_new_type<PrimitiveType>(Type::INT, true, false), expr.oper};
    }

    ExprVisitorType index = resolve(expr.index.get());

    if (index.info->data.type != Type::INT) {
        error("Expected integral type for list index", expr.oper);
        note("This error can lead to other warnings/errors that may be incorrect,"
             " try to fix this error first");
        return {make_new_type<PrimitiveType>(Type::INT, true, false), expr.oper};
    }

    return {list.info, expr.oper};
}

ExprVisitorType TypeResolver::visit(LiteralExpr& expr) {
    switch(expr.value.value.index()) {
        case LiteralValue::INT:
        case LiteralValue::DOUBLE:
        case LiteralValue::STRING:
        case LiteralValue::BOOL:
        case LiteralValue::NULL_:
            return {expr.type.get(), expr.lexeme};

        default:
            error("Bug in parser with illegal type for literal value", expr.lexeme);
            throw TypeException{"Bug in parser with illegal type for literal value"};
    }
}

ExprVisitorType TypeResolver::visit(LogicalExpr& expr) {
    resolve(expr.left.get());
    resolve(expr.right.get());
    return {make_new_type<PrimitiveType>(Type::BOOL, true, false), expr.oper};
}

ExprVisitorType TypeResolver::visit(SetExpr& expr) {

}

ExprVisitorType TypeResolver::visit(SuperExpr& expr) {
    // TODO: Implement me
    throw TypeException{"Super expressions/inheritance not implemented yet"};
}

ExprVisitorType TypeResolver::visit(TernaryExpr& expr) {
    ExprVisitorType left = resolve(expr.left.get());
    ExprVisitorType middle = resolve(expr.middle.get());
    ExprVisitorType right = resolve(expr.right.get());

    if (middle.info->data.type != right.info->data.type) {
        error("Expected equivalent expression types for branches of ternary expression", expr.question);
    }

    return {middle.info, expr.question};
}

ExprVisitorType TypeResolver::visit(ThisExpr& expr) {
    return {current_class, expr.keyword};
}

ExprVisitorType TypeResolver::visit(UnaryExpr& expr) {
    ExprVisitorType right = resolve(expr.right.get());
    switch(expr.oper.type) {
        case TokenType::BIT_NOT:
            if (right.info->data.type != Type::INT) {
                error("Wrong type of argument to bitwise unary operator (expected integral argument)",
                      expr.oper);
            }
            return {make_new_type<PrimitiveType>(Type::INT, true, false), expr.oper};
        case TokenType::NOT:
            if (one_of(right.info->data.type, Type::CLASS, Type::LIST, Type::NULL_)) {
                error("Wrong type of argument to logical not operator", expr.oper);
            }
            return {make_new_type<PrimitiveType>(Type::BOOL, true, false), expr.oper};
        case TokenType::PLUS_PLUS:
        case TokenType::MINUS_MINUS:
            if (right.info->data.is_const || (expr.right->type_tag() != NodeType::VariableExpr &&
                                              expr.right->type_tag() != NodeType::GetExpr)) {
                if (expr.oper.type == TokenType::PLUS_PLUS) {
                    constexpr std::string_view message =
                            "Expected non-const l-value type as argument for unary increment operator";
                    error(message, expr.oper);
                } else if (expr.oper.type == TokenType::MINUS_MINUS) {
                    constexpr std::string_view message =
                            "Expected non-const l-value type as argument for unary decrement operator";
                    error(message, expr.oper);
                }
                return {make_new_type<PrimitiveType>(Type::INT, true, false), expr.oper};
            }
            return {right.info, expr.oper};
        case TokenType::MINUS:
        case TokenType::PLUS:
            if (!one_of(right.info->data.type, Type::INT, Type::FLOAT)) {
                error("Expected integral or floating point argument to operator", expr.oper);
                return {make_new_type<PrimitiveType>(Type::INT, true, false), expr.oper};
            }
            return {right.info, expr.oper};

        default:
            error("Bug in parser with illegal type for unary expression", expr.oper);
            throw TypeException{"Bug in parser with illegal type for unary expression"};
    }
}

ExprVisitorType TypeResolver::visit(VariableExpr& expr) {
    for (auto it = values.end(); it > values.begin(); it--) {
        if (it->lexeme == expr.name.lexeme) {
            expr.scope_depth = it->scope_depth;
            return {it->info, expr.name};
        }
    }

    for (FunctionStmt *func : functions) {
        if (func->name.lexeme == expr.name.lexeme) {
            return {func, expr.name};
        }
    }

    error("No such variable/function in the current scope", expr.name);
    note("This error can lead to other warnings/errors that may be incorrect,"
         " try to fix this error first");
    return {make_new_type<PrimitiveType>(Type::INT, true, false), expr.name};
}


StmtVisitorType TypeResolver::visit(BlockStmt& stmt) {
    begin_scope();
    for (auto &statement : stmt.stmts) {
        resolve(statement.get());
    }
    end_scope();
}

StmtVisitorType TypeResolver::visit(BreakStmt&) {}

StmtVisitorType TypeResolver::visit(ClassStmt& stmt) {
    bool in_class_outer = in_class;
    in_class = true;
    current_class = &stmt;

    in_class = in_class_outer;
}

StmtVisitorType TypeResolver::visit(ContinueStmt&) {}

StmtVisitorType TypeResolver::visit(ExpressionStmt& stmt) {
    resolve(stmt.expr.get());
}

StmtVisitorType TypeResolver::visit(FunctionStmt& stmt) {
    bool in_function_outer = in_function;
    in_function = true;
    begin_scope();



    end_scope();
    in_function = in_function_outer;
}

StmtVisitorType TypeResolver::visit(IfStmt& stmt) {
    resolve(stmt.condition.get());
    resolve(stmt.thenBranch.get());
    if (stmt.elseBranch != nullptr) {
        resolve(stmt.elseBranch.get());
    }
}

StmtVisitorType TypeResolver::visit(ImportStmt& stmt) {

}

StmtVisitorType TypeResolver::visit(ReturnStmt& stmt) {

}

StmtVisitorType TypeResolver::visit(SwitchStmt& stmt) {
    bool in_switch_outer = in_switch;
    in_switch = true;

    in_switch = in_switch_outer;
}

StmtVisitorType TypeResolver::visit(TypeStmt&) {
    // TODO: Implement me
}

StmtVisitorType TypeResolver::visit(VarStmt& stmt) {
    if (std::any_of(values.crbegin(), values.crend(), [this, &stmt](const Value &value) {
        return value.lexeme == stmt.name.lexeme && value.scope_depth == scope_depth;
    })) {
        error("A variable with the same name has already been created in this scope", stmt.name);
    }

    if (stmt.initializer != nullptr && stmt.type != nullptr) {
        QualifiedTypeInfo type = resolve(stmt.type.get());
        ExprVisitorType initializer = resolve(stmt.initializer.get());
        if (!convertible_to(type, initializer.info, stmt.name)) {
            error("Cannot convert from initializer type to type of variable", stmt.name);
        }
        if (!in_class) {
            values.push_back({stmt.name.lexeme, stmt.type.get(), scope_depth});
        }
    } else if (stmt.initializer != nullptr) {
        ExprVisitorType initializer = resolve(stmt.initializer.get());
        if (!in_class) {
            values.push_back({stmt.name.lexeme, stmt.type.get(), scope_depth});
        }
    } else if (stmt.type == nullptr) {
        error("Expected type for variable", stmt.name);
        // The variable is never created if there is an error creating it thus any references to it also break
    }
}

StmtVisitorType TypeResolver::visit(WhileStmt& stmt) {
    begin_scope();
    bool in_loop_outer = in_loop;
    in_loop = true;

    ExprTypeInfo condition = resolve(stmt.condition.get());
    if (one_of(condition.info->data.type, Type::CLASS, Type::LIST)) {
        error("Class or list types are not implicitly convertible to bool", stmt.keyword);
    }

    resolve(stmt.body.get());

    in_loop = in_loop_outer;
    end_scope();
}

BaseTypeVisitorType TypeResolver::visit(PrimitiveType& type) {
    return &type;
}

BaseTypeVisitorType TypeResolver::visit(UserDefinedType& type) {
    return &type;
}

BaseTypeVisitorType TypeResolver::visit(ListType& type) {
    if (type.contained->type_tag() == NodeType::TypeofType) {
        auto *contained = dynamic_cast<TypeofType*>(type.contained.get());
        return resolve(contained->expr.get()).info;
    } else {
        return &type;
    }
}

BaseTypeVisitorType TypeResolver::visit(TypeofType& type) {
    return resolve(type.expr.get()).info;
}