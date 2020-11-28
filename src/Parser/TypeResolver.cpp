/* Copyright (C) 2020  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "TypeResolver.hpp"

#include "../Common.hpp"
#include "../ErrorLogger/ErrorLogger.hpp"
#include "Parser.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string_view>

#define allocate_node(T, ...)                                                                                          \
    new T { __VA_ARGS__ }

struct TypeException : public std::runtime_error {
    explicit TypeException(std::string_view string) : std::runtime_error{std::string{string}} {}
};

struct scoped_boolean_manager {
    bool &scoped_bool;
    bool previous_value{};
    explicit scoped_boolean_manager(bool &controlled) : scoped_bool{controlled} {
        previous_value = controlled;
        controlled = true;
    }

    ~scoped_boolean_manager() { scoped_bool = previous_value; }
};

struct scoped_scope_manager {
    TypeResolver &resolver;
    explicit scoped_scope_manager(TypeResolver &resolver) : resolver{resolver} { resolver.begin_scope(); }
    ~scoped_scope_manager() { resolver.end_scope(); }
};

TypeResolver::TypeResolver(Module &module)
    : current_module{module}, classes{module.classes}, functions{module.functions} {}

////////////////////////////////////////////////////////////////////////////////

bool convertible_to(QualifiedTypeInfo to, QualifiedTypeInfo from, bool from_lvalue, const Token &where) {
    bool class_condition = [&to, &from]() {
        if (to->type_tag() == NodeType::UserDefinedType && from->type_tag() == NodeType::UserDefinedType) {
            return dynamic_cast<UserDefinedType *>(to)->name.lexeme ==
                   dynamic_cast<UserDefinedType *>(from)->name.lexeme;
        } else {
            return true;
        }
    }();
    // The class condition is used to check if the names of the classes being converted between are same
    // This is only useful if both to and from are class types, hence it is only checked if so

    if (to->data.is_ref) {
        if (!from_lvalue) {
            error("Cannot bind reference to non l-value type object", where);
            return false;
        } else if (from->data.is_const && !to->data.is_const) {
            error("Cannot bind non-const reference to constant object", where);
            return false;
        }

        return from->data.type == to->data.type && class_condition;
    } else if ((from->data.type == Type::FLOAT && to->data.type == Type::INT) ||
               (from->data.type == Type::INT && to->data.type == Type::FLOAT)) {
        warning("Implicit conversion from float to int", where);
        return true;
    } else {
        return from->data.type == to->data.type && class_condition;
    }
}

ClassStmt *TypeResolver::find_class(const std::string &class_name) {
    if (auto class_ = classes.find(class_name); class_ != classes.end()) {
        return class_->second;
    }

    return nullptr;
}

FunctionStmt *TypeResolver::find_function(const std::string &function_name) {
    if (auto func = functions.find(function_name); func != functions.end()) {
        return func->second;
    }

    return nullptr;
}

void TypeResolver::check(std::vector<StmtNode> &program) {
    for (auto &stmt : program) {
        if (stmt != nullptr) {
            try {
                resolve(stmt.get());
            } catch (...) {}
        }
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
    stmt->accept(*this);
}

BaseTypeVisitorType TypeResolver::resolve(BaseType *type) {
    return type->accept(*this);
}

template <typename T, typename... Args>
BaseType *TypeResolver::make_new_type(Type type, bool is_const, bool is_ref, Args &&... args) {
    if constexpr (sizeof...(args) > 0) {
        type_scratch_space.emplace_back(
            allocate_node(T, SharedData{type, is_const, is_ref}, std::forward<Args>(args)...));
    } else {
        for (TypeNode &existing_type : type_scratch_space) {
            if (existing_type->data.type == type && existing_type->data.is_const == is_const &&
                existing_type->data.is_ref == is_ref) {
                return existing_type.get();
            }
        }
        type_scratch_space.emplace_back(allocate_node(T, SharedData{type, is_const, is_ref}));
    }
    return type_scratch_space.back().get();
}

ExprVisitorType TypeResolver::visit(AssignExpr &expr) {
    for (auto it = values.end() - 1; it >= values.begin(); it--) {
        if (it->lexeme == expr.target.lexeme) {
            ExprVisitorType value = resolve(expr.value.get());
            if (it->info->data.is_const) {
                error("Cannot assign to a const variable", expr.target);
            } else if (!convertible_to(it->info, value.info, value.is_lvalue, expr.target)) {
                error("Cannot convert type of value to type of target", expr.target);
            } else if (value.info->data.type == Type::FLOAT && it->info->data.type == Type::INT) {
                expr.conversion_type = NumericConversionType::FLOAT_TO_INT;
            } else if (value.info->data.type == Type::INT && it->info->data.type == Type::FLOAT) {
                expr.conversion_type = NumericConversionType::INT_TO_FLOAT;
            }
            return {it->info, expr.target};
        }
    }

    error("No such variable in the current scope", expr.target);
    throw TypeException{"No such variable in the current scope"};
}

template <typename... Args>
bool one_of(Type type, Args... args) {
    const std::array arr{args...};
    return std::any_of(arr.begin(), arr.end(), [type](const auto &arg) { return type == arg; });
}

ExprVisitorType TypeResolver::visit(BinaryExpr &expr) {
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
                if (expr.oper.type == TokenType::MODULO) {
                    error("Wrong types of arguments to modulo operator (expected integral arguments)", expr.oper);
                } else {
                    error(
                        "Wrong types of arguments to bitwise binary operator (expected integral arguments)", expr.oper);
                }
            }
            expr.resolved_type = {left_expr.info, expr.oper};
            return {left_expr.info, expr.oper};
        case TokenType::NOT_EQUAL:
        case TokenType::EQUAL_EQUAL:
            if (one_of(left_expr.info->data.type, Type::BOOL, Type::STRING, Type::LIST, Type::NULL_)) {
                if (left_expr.info->data.type != right_expr.info->data.type) {
                    error("Cannot compare equality of objects of different types", expr.oper);
                }
                expr.resolved_type = {make_new_type<PrimitiveType>(Type::BOOL, true, false), expr.oper};
                return expr.resolved_type;
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
                expr.resolved_type = {make_new_type<PrimitiveType>(Type::BOOL, true, false), expr.oper};
                return expr.resolved_type;
            } else if (left_expr.info->data.type == Type::BOOL && right_expr.info->data.type == Type::BOOL) {
                expr.resolved_type = {make_new_type<PrimitiveType>(Type::BOOL, true, false), expr.oper};
                return expr.resolved_type;
            } else {
                error("Cannot compare objects of incompatible types", expr.oper);
                expr.resolved_type = {make_new_type<PrimitiveType>(Type::BOOL, true, false), expr.oper};
                return expr.resolved_type;
            }
        case TokenType::PLUS:
            if (left_expr.info->data.type == Type::STRING && right_expr.info->data.type == Type::STRING) {
                expr.resolved_type = {make_new_type<PrimitiveType>(Type::STRING, true, false), expr.oper};
                return expr.resolved_type;
            }
            [[fallthrough]];
        case TokenType::MINUS:
        case TokenType::SLASH:
        case TokenType::STAR:
            if (one_of(left_expr.info->data.type, Type::INT, Type::FLOAT) &&
                one_of(right_expr.info->data.type, Type::INT, Type::FLOAT)) {
                if (left_expr.info->data.type == Type::INT && right_expr.info->data.type == Type::INT) {
                    expr.resolved_type = {make_new_type<PrimitiveType>(Type::INT, true, false), expr.oper};
                    return expr.resolved_type;
                }
                expr.resolved_type = {make_new_type<PrimitiveType>(Type::FLOAT, true, false), expr.oper};
                return expr.resolved_type;
                // Integral promotion
            } else {
                error("Cannot use arithmetic operators on objects of incompatible types", expr.oper);
                throw TypeException{"Cannot use arithmetic operators on objects of incompatible types"};
            }

        default:
            error("Bug in parser with illegal token type of expression's operator", expr.oper);
            throw TypeException{"Bug in parser with illegal token type of expression's operator"};
    }
}

bool is_inbuilt(VariableExpr *expr) {
    constexpr std::array inbuilt_functions{"print", "int", "string", "float"};
    return std::any_of(inbuilt_functions.begin(), inbuilt_functions.end(),
        [&expr](const char *const arg) { return expr->name.lexeme == arg; });
}

ExprVisitorType TypeResolver::check_inbuilt(
    VariableExpr *function, const Token &oper, std::vector<std::pair<ExprNode, NumericConversionType>> &args) {
    using namespace std::string_literals;
    if (function->name.lexeme == "print") {
        for (auto &arg : args) {
            ExprVisitorType type = resolve(arg.first.get());
            if (type.info->data.type == Type::CLASS) {
                error("Cannot print object of user defined type", type.lexeme);
            }
        }
        return ExprVisitorType{make_new_type<PrimitiveType>(Type::NULL_, true, false), function->name};
    } else if (function->name.lexeme == "int" || function->name.lexeme == "float" ||
               function->name.lexeme == "string") {
        if (args.size() > 1) {
            error("Cannot pass more than one argument to builtin function '"s + function->name.lexeme + "'"s, oper);
            throw TypeException{"Too many arguments"};
        } else if (args.empty()) {
            error("Cannot call builtin function '"s + function->name.lexeme + "' with zero arguments"s, oper);
            throw TypeException{"No arguments"};
        }

        ExprVisitorType arg_type = resolve(args[0].first.get());
        if (!one_of(arg_type.info->data.type, Type::INT, Type::FLOAT, Type::STRING, Type::BOOL)) {
            error("Expected one of integral, floating, string or boolean arguments to be passed to builtin "
                  "function '"s +
                      function->name.lexeme + "'"s,
                oper);
        }

        Type return_type = [&function]() {
            if (function->name.lexeme == "int") {
                return Type::INT;
            } else if (function->name.lexeme == "float") {
                return Type::FLOAT;
            } else {
                return Type::STRING;
            }
        }();
        return ExprVisitorType{make_new_type<PrimitiveType>(return_type, true, false), function->name};
    } else {
        unreachable();
    }
}

ExprVisitorType TypeResolver::visit(CallExpr &expr) {
    if (expr.function->type_tag() == NodeType::VariableExpr) {
        auto *function = dynamic_cast<VariableExpr *>(expr.function.get());
        if (is_inbuilt(function)) {
            return check_inbuilt(function, expr.paren, expr.args);
        }
    }

    ExprVisitorType callee = resolve(expr.function.get());

    FunctionStmt *called = callee.func;
    if (callee.class_ != nullptr) {
        for (auto &method_decl : callee.class_->methods) {
            if (method_decl.first->name == callee.lexeme) {
                called = method_decl.first.get();
            }
        }
    }

    if (expr.function->type_tag() == NodeType::GetExpr) {
        auto *get = dynamic_cast<GetExpr *>(expr.function.get());
        expr.args.insert(expr.args.begin(), {std::move(get->object), NumericConversionType::NONE});
    }

    if (called->params.size() != expr.args.size()) {
        error("Number of arguments passed to function must match the number of parameters", expr.paren);
        throw TypeException{"Number of arguments passed to function must match the number of parameters"};
    }

    for (std::size_t i{0}; i < expr.args.size(); i++) {
        ExprVisitorType argument = resolve(expr.args[i].first.get());
        if (!convertible_to(called->params[i].second.get(), argument.info, argument.is_lvalue, argument.lexeme)) {
            error("Type of argument is not convertible to type of parameter", argument.lexeme);
        } else if (argument.info->data.type == Type::FLOAT && called->params[i].second->data.type == Type::INT) {
            expr.args[i].second = NumericConversionType::FLOAT_TO_INT;
        } else if (argument.info->data.type == Type::INT && called->params[i].second->data.type == Type::FLOAT) {
            expr.args[i].second = NumericConversionType::INT_TO_FLOAT;
        }
    }

    return {called->return_type.get(), called, callee.class_, expr.paren};
}

ExprVisitorType TypeResolver::visit(CommaExpr &expr) {
    auto it = begin(expr.exprs);

    for (auto next = std::next(it); next != end(expr.exprs); it = next, ++next)
        resolve(it->get());

    return resolve(it->get());
}

ExprVisitorType TypeResolver::resolve_class_access(ExprVisitorType &object, const Token &name) {
    if (object.info->data.type == Type::LIST) {
        if (name.lexeme != "size") {
            error("Can only get 'size' attribute of a list", name);
            throw TypeException{"Can only get 'size' attribute of a list"};
        }
    } else if (object.info->data.type == Type::CLASS) {
        ClassStmt *accessed_type = object.class_;

        for (auto &member_decl : accessed_type->members) {
            auto *member = member_decl.first.get();
            if (member->name == name) {
                if (member_decl.second == VisibilityType::PUBLIC ||
                    (in_class && current_class->name == accessed_type->name)) {
                    return {member->type.get(), name};
                } else if (member_decl.second == VisibilityType::PROTECTED) {
                    error("Cannot access protected member outside class", name);
                } else if (member_decl.second == VisibilityType::PRIVATE) {
                    error("Cannot access private member outside class", name);
                }
                return {member->type.get(), name};
            }
        }

        for (auto &method_decl : accessed_type->methods) {
            auto *method = method_decl.first.get();
            if (method->name == name) {
                if ((method_decl.second == VisibilityType::PUBLIC) ||
                    (in_class && current_class->name == accessed_type->name)) {
                    return {make_new_type<PrimitiveType>(Type::FUNCTION, true, false), method_decl.first.get(), name};
                } else if (method_decl.second == VisibilityType::PROTECTED) {
                    error("Cannot access protected method outside class", name);
                } else if (method_decl.second == VisibilityType::PRIVATE) {
                    error("Cannot access private method outside class", name);
                }
                return {make_new_type<PrimitiveType>(Type::FUNCTION, true, false), method_decl.first.get(), name};
            }
        }

        error("No such attribute exists in the class", name);
        throw TypeException{"No such attribute exists in the class"};
    } else {
        error("Expected class or list type to take attribute of", name);
        throw TypeException{"Expected class or list type to take attribute of"};
    }

    unreachable();
}

ExprVisitorType TypeResolver::visit(GetExpr &expr) {
    ExprVisitorType object = resolve(expr.object.get());
    return resolve_class_access(object, expr.name);
}

ExprVisitorType TypeResolver::visit(GroupingExpr &expr) {
    return resolve(expr.expr.get());
}

ExprVisitorType TypeResolver::visit(IndexExpr &expr) {
    ExprVisitorType list = resolve(expr.object.get());

    if (list.info->data.type != Type::LIST) {
        error("Expected list type for indexing", expr.oper);
        throw TypeException{"Expected list type for indexing"};
    }

    ExprVisitorType index = resolve(expr.index.get());

    if (index.info->data.type != Type::INT) {
        error("Expected integral type for list index", expr.oper);
        throw TypeException{"Expected integral type for list index"};
    }

    auto *contained_type = dynamic_cast<ListType *>(list.info)->contained.get();
    return {contained_type, expr.oper};
}

ExprVisitorType TypeResolver::visit(LiteralExpr &expr) {
    switch (expr.value.tag) {
        case LiteralValue::INT:
        case LiteralValue::DOUBLE:
        case LiteralValue::STRING:
        case LiteralValue::BOOL:
        case LiteralValue::NULL_: return {expr.type.get(), expr.lexeme};

        default:
            error("Bug in parser with illegal type for literal value", expr.lexeme);
            throw TypeException{"Bug in parser with illegal type for literal value"};
    }
}

ExprVisitorType TypeResolver::visit(LogicalExpr &expr) {
    resolve(expr.left.get());
    resolve(expr.right.get());
    return {make_new_type<PrimitiveType>(Type::BOOL, true, false), expr.oper};
}

ExprVisitorType TypeResolver::visit(ScopeAccessExpr &expr) {
    ExprVisitorType left = resolve(expr.scope.get());

    switch (left.scope_type) {
        case ExprTypeInfo::ScopeType::CLASS:
            for (auto &method : left.class_->methods) {
                if (method.first->name == expr.name) {
                    return {make_new_type<PrimitiveType>(Type::FUNCTION, true, false), method.first.get(), left.class_,
                        expr.name};
                }
            }
            error("No such method exists in the class", expr.name);
            throw TypeException{"No such method exists in the class"};

        case ExprTypeInfo::ScopeType::MODULE: {
            auto &module = Parser::parsed_modules[left.module_index].first;
            if (auto class_ = module.classes.find(expr.name.lexeme); class_ != module.classes.end()) {
                return {make_new_type<PrimitiveType>(Type::CLASS, true, false), class_->second, expr.name};
            }

            if (auto func = module.functions.find(expr.name.lexeme); func != module.functions.end()) {
                return {make_new_type<PrimitiveType>(Type::FUNCTION, true, false), func->second, expr.name};
            }
        }
            error("No such function/class exists in the module", expr.name);
            throw TypeException{"No such function/class exists in the module"};

        case ExprTypeInfo::ScopeType::NONE:
        default:
            error("No such module/class exists in the current global scope", expr.name);
            throw TypeException{"No such module/class exists in the current global scope"};
    }
}

ExprVisitorType TypeResolver::visit(ScopeNameExpr &expr) {
    for (std::size_t i{0}; i < Parser::parsed_modules.size(); i++) {
        if (Parser::parsed_modules[i].first.name.substr(0, Parser::parsed_modules[i].first.name.find_last_of('.')) ==
            expr.name.lexeme) {
            return {make_new_type<PrimitiveType>(Type::MODULE, true, false), i, expr.name};
        }
    }

    if (ClassStmt *class_ = find_class(expr.name.lexeme); class_ != nullptr) {
        return {make_new_type<PrimitiveType>(Type::CLASS, true, false), class_, expr.name};
    }

    error("No such scope exists with the given name", expr.name);
    throw TypeException{"No such scope exists with the given name"};
}

ExprVisitorType TypeResolver::visit(SetExpr &expr) {
    ExprVisitorType object = resolve(expr.object.get());
    ExprVisitorType attribute_type = resolve_class_access(object, expr.name);
    ExprVisitorType value_type = resolve(expr.value.get());

    if (object.info->data.is_const) {
        error("Cannot assign to a const object", expr.name);
    } else if (!in_ctor && attribute_type.info->data.is_const) {
        error("Cannot assign to const attribute", expr.name);
    }

    if (!convertible_to(attribute_type.info, value_type.info, value_type.is_lvalue, expr.name)) {
        error("Cannot convert value of assigned expresion to type of target", expr.name);
        throw TypeException{"Cannot convert value of assigned expression to type of target"};
    } else if (value_type.info->data.type == Type::FLOAT && attribute_type.info->data.type == Type::INT) {
        expr.conversion_type = NumericConversionType::FLOAT_TO_INT;
    } else if (value_type.info->data.type == Type::INT && attribute_type.info->data.type == Type::FLOAT) {
        expr.conversion_type = NumericConversionType::INT_TO_FLOAT;
    }

    return {attribute_type.info, expr.name};
}

ExprVisitorType TypeResolver::visit(SuperExpr &) {
    // TODO: Implement me
    throw TypeException{"Super expressions/inheritance not implemented yet"};
}

ExprVisitorType TypeResolver::visit(TernaryExpr &expr) {
    ExprVisitorType left = resolve(expr.left.get());
    ExprVisitorType middle = resolve(expr.middle.get());
    ExprVisitorType right = resolve(expr.right.get());

    if (!convertible_to(middle.info, right.info, right.is_lvalue, expr.question) &&
        !convertible_to(right.info, middle.info, right.is_lvalue, expr.question)) {
        error("Expected equivalent expression types for branches of ternary expression", expr.question);
    }

    return {middle.info, expr.question};
}

ExprVisitorType TypeResolver::visit(ThisExpr &expr) {
    if (!in_ctor && !in_dtor) {
        error("Cannot use 'this' keyword outside a class's constructor or destructor", expr.keyword);
        throw TypeException{"Cannot use 'this' keyword outside a class's constructor or destructor"};
    }
    return {
        make_new_type<UserDefinedType>(Type::CLASS, false, false, current_class->name), current_class, expr.keyword};
}

ExprVisitorType TypeResolver::visit(UnaryExpr &expr) {
    ExprVisitorType right = resolve(expr.right.get());
    switch (expr.oper.type) {
        case TokenType::BIT_NOT:
            if (right.info->data.type != Type::INT) {
                error("Wrong type of argument to bitwise unary operator (expected integral argument)", expr.oper);
            }
            return {make_new_type<PrimitiveType>(Type::INT, true, false), expr.oper};
        case TokenType::NOT:
            if (one_of(right.info->data.type, Type::CLASS, Type::LIST, Type::NULL_)) {
                error("Wrong type of argument to logical not operator", expr.oper);
            }
            return {make_new_type<PrimitiveType>(Type::BOOL, true, false), expr.oper};
        case TokenType::PLUS_PLUS:
        case TokenType::MINUS_MINUS:
            if (right.info->data.is_const ||
                (expr.right->type_tag() != NodeType::VariableExpr && expr.right->type_tag() != NodeType::GetExpr)) {
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

ExprVisitorType TypeResolver::visit(VariableExpr &expr) {
    if (is_inbuilt(&expr)) {
        error("Cannot use in-built function as an expression", expr.name);
        throw TypeException{"Cannot use in-built function as an expression"};
    }

    for (auto it = values.end() - 1; !values.empty() && it >= values.begin(); it--) {
        if (it->lexeme == expr.name.lexeme) {
            expr.stack_slot = it->stack_slot;
            return {it->info, it->class_, expr.name, true};
        }
    }

    if (FunctionStmt *func = find_function(expr.name.lexeme); func != nullptr) {
        return {make_new_type<PrimitiveType>(Type::FUNCTION, true, false), func, expr.name};
    }

    if (ClassStmt *class_ = find_class(expr.name.lexeme); class_ != nullptr) {
        return {make_new_type<PrimitiveType>(Type::CLASS, true, false), class_, expr.name};
    }

    error("No such variable/function in the current module's scope", expr.name);
    throw TypeException{"No such variable/function in the current module's scope"};
}

StmtVisitorType TypeResolver::visit(BlockStmt &stmt) {
    scoped_scope_manager manager{*this};
    for (auto &statement : stmt.stmts) {
        if (statement != nullptr) {
            try {
                resolve(statement.get());
            } catch (...) {}
        }
    }
}

StmtVisitorType TypeResolver::visit(BreakStmt &) {}

StmtVisitorType TypeResolver::visit(ClassStmt &stmt) {
    scoped_boolean_manager class_manager{in_class};

    ////////////////////////////////////////////////////////////////////////////
    struct scoped_class_manager {
        ClassStmt *&managed_class;
        ClassStmt *previous_value{nullptr};
        scoped_class_manager(ClassStmt *(&current_class), ClassStmt *stmt)
            : managed_class{current_class}, previous_value{current_class} {
            current_class = stmt;
        }
        ~scoped_class_manager() { managed_class = previous_value; }
    } pointer_manager{current_class, &stmt};
    ////////////////////////////////////////////////////////////////////////////

    // Creation of the implicit constructor
    if (stmt.ctor == nullptr) {
        stmt.ctor = allocate_node(FunctionStmt, stmt.name,
            TypeNode{allocate_node(UserDefinedType, {Type::CLASS, false, false}, stmt.name)}, {},
            StmtNode{allocate_node(BlockStmt, {})});
        stmt.methods.emplace_back(std::unique_ptr<FunctionStmt>{stmt.ctor}, VisibilityType::PUBLIC);
    }

    std::size_t initialized_count = std::count_if(stmt.members.begin(), stmt.members.end(),
        [](const auto &member) { return member.first->initializer != nullptr; });

    auto *ctor_body = dynamic_cast<BlockStmt *>(stmt.ctor->body.get());
    ctor_body->stmts.reserve(initialized_count + ctor_body->stmts.size());

    for (auto &member_declaration : stmt.members) {
        auto *member = member_declaration.first.get();
        if (member->initializer != nullptr) {
            using namespace std::string_literals;
            // Transform the declaration of the member into an implicit assignment in the constructor
            // i.e `public var x = 0` becomes `public var x: int` and `this.x = 0`
            // TODO: What about references?
            Expr *this_expr = allocate_node(ThisExpr, member->name);

            if (member->type == nullptr) {
                member->type = TypeNode{copy_type(resolve(member->initializer.get()).info)};
            }

            ExprNode member_initializer{allocate_node(SetExpr, ExprNode{this_expr}, member->name,
                {std::move(member->initializer)}, NumericConversionType::NONE)};
            StmtNode member_init_stmt{allocate_node(ExpressionStmt, std::move(member_initializer))};

            ctor_body->stmts.insert(ctor_body->stmts.begin(), std::move(member_init_stmt));

            member->initializer = nullptr;
        }
    }

    for (auto &member_decl : stmt.members) {
        resolve(member_decl.first.get());
    }

    for (auto &method_decl : stmt.methods) {
        resolve(method_decl.first.get());
    }
}

StmtVisitorType TypeResolver::visit(ContinueStmt &) {}

StmtVisitorType TypeResolver::visit(ExpressionStmt &stmt) {
    resolve(stmt.expr.get());
}

StmtVisitorType TypeResolver::visit(FunctionStmt &stmt) {
    scoped_scope_manager manager{*this};
    scoped_boolean_manager function_manager{in_function};

    ////////////////////////////////////////////////////////////////////////////
    struct scoped_function_manager {
        FunctionStmt *&managed_class;
        FunctionStmt *previous_value{nullptr};
        scoped_function_manager(FunctionStmt *(&current_class), FunctionStmt *stmt)
            : managed_class{current_class}, previous_value{current_class} {
            current_class = stmt;
        }
        ~scoped_function_manager() { managed_class = previous_value; }
    } pointer_manager{current_function, &stmt};

    bool throwaway{};
    bool is_in_ctor = current_class != nullptr && stmt.name == current_class->name;
    bool is_in_dtor = current_class != nullptr && stmt.name.lexeme[0] == '~' &&
                      (stmt.name.lexeme.substr(1) == current_class->name.lexeme);

    scoped_boolean_manager special_func_manager{is_in_ctor ? in_ctor : (is_in_dtor ? in_dtor : throwaway)};
    ////////////////////////////////////////////////////////////////////////////

    for (const auto &param : stmt.params) {
        ClassStmt *param_class = nullptr;

        if (param.second->type_tag() == NodeType::UserDefinedType) {
            param_class = find_class(dynamic_cast<UserDefinedType &>(*param.second).name.lexeme);
            if (param_class == nullptr) {
                error("No such module/class exists in the current global scope", stmt.name);
                throw TypeException{"No such module/class exists in the current global scope"};
            }
        }

        values.push_back({param.first.lexeme, param.second.get(), scope_depth, param_class, values.size()});
    }

    resolve(stmt.body.get());
}

StmtVisitorType TypeResolver::visit(IfStmt &stmt) {
    resolve(stmt.condition.get());
    resolve(stmt.thenBranch.get());

    if (stmt.elseBranch != nullptr) {
        resolve(stmt.elseBranch.get());
    }
}

StmtVisitorType TypeResolver::visit(ReturnStmt &stmt) {
    ExprVisitorType return_value = resolve(stmt.value.get());
    if (!convertible_to(current_function->return_type.get(), return_value.info, return_value.is_lvalue, stmt.keyword)) {
        error("Type of expression in return statement does not match return type of function", stmt.keyword);
        using namespace std::string_literals;
        std::string note_message = "The type being returned is '"s + stringify(return_value.info) +
                                   "' whereas the function return type is '"s +
                                   stringify(current_function->return_type.get()) + "'";
        note(note_message);
    }
}

StmtVisitorType TypeResolver::visit(SwitchStmt &stmt) {
    scoped_boolean_manager switch_manager{in_switch};

    ExprVisitorType condition = resolve(stmt.condition.get());

    for (auto &case_stmt : stmt.cases) {
        ExprVisitorType case_expr = resolve(case_stmt.first.get());
        if (!convertible_to(case_expr.info, condition.info, condition.is_lvalue, case_expr.lexeme)) {
            error("Type of case expression cannot be converted to type of switch condition", case_expr.lexeme);
        }
        resolve(case_stmt.second.get());
    }

    if (stmt.default_case != nullptr) {
        resolve(stmt.default_case.get());
    }
}

StmtVisitorType TypeResolver::visit(TypeStmt &) {
    // TODO: Implement me
    throw TypeException{"Type statements are not implemented yet"};
}

StmtVisitorType TypeResolver::visit(VarStmt &stmt) {
    if (!in_class && std::any_of(values.crbegin(), values.crend(), [this, &stmt](const Value &value) {
            return value.scope_depth == scope_depth && value.lexeme == stmt.name.lexeme;
        })) {
        error("A variable with the same name has already been created in this scope", stmt.name);
    }

    if (stmt.is_val && stmt.type != nullptr) {
        stmt.type->data.is_const = true;
    }

    if (stmt.initializer != nullptr && stmt.type != nullptr) {
        QualifiedTypeInfo type = resolve(stmt.type.get());
        ExprVisitorType initializer = resolve(stmt.initializer.get());
        if (!convertible_to(type, initializer.info, initializer.is_lvalue, stmt.name)) {
            error("Cannot convert from initializer type to type of variable", stmt.name);
        } else if (initializer.info->data.type == Type::FLOAT && type->data.type == Type::INT) {
            stmt.conversion_type = NumericConversionType::FLOAT_TO_INT;
        } else if (initializer.info->data.type == Type::INT && type->data.type == Type::FLOAT) {
            stmt.conversion_type = NumericConversionType::INT_TO_FLOAT;
        }

        if (!in_class || in_function) {
            values.push_back({stmt.name.lexeme, type, scope_depth, initializer.class_, values.size()});
        }
    } else if (stmt.initializer != nullptr) {
        ExprVisitorType initializer = resolve(stmt.initializer.get());
        stmt.type = TypeNode{copy_type(initializer.info)};

        if (stmt.is_val) {
            stmt.type->data.is_const = true;
        } else if (!initializer.info->data.is_ref) {
            // If there is a var statement without a specified type that is not binding to a reference it is
            // automatically non-const
            stmt.type->data.is_const = false;
        }

        if (!in_class || in_function) {
            values.push_back({stmt.name.lexeme, stmt.type.get(), scope_depth, initializer.class_, values.size()});
        }
    } else if (stmt.type != nullptr) {
        QualifiedTypeInfo type = resolve(stmt.type.get());
        ClassStmt *stmt_class = nullptr;

        if (stmt.type->type_tag() == NodeType::UserDefinedType) {
            stmt_class = find_class(dynamic_cast<UserDefinedType &>(*stmt.type).name.lexeme);
            if (stmt_class == nullptr) {
                error("No such module/class exists in the current global scope", stmt.name);
                throw TypeException{"No such module/class exists in the current global scope"};
            }
        }

        if (!in_class || in_function) {
            values.push_back({stmt.name.lexeme, type, scope_depth, stmt_class, values.size()});
        }
    } else {
        error("Expected type for variable", stmt.name);
        // The variable is never created if there is an error creating it thus any references to it also break
    }
}

StmtVisitorType TypeResolver::visit(WhileStmt &stmt) {
    scoped_scope_manager manager{*this};
    scoped_boolean_manager loop_manager{in_loop};

    ExprVisitorType condition = resolve(stmt.condition.get());
    if (one_of(condition.info->data.type, Type::CLASS, Type::LIST)) {
        error("Class or list types are not implicitly convertible to bool", stmt.keyword);
    }

    resolve(stmt.body.get());
}

BaseTypeVisitorType TypeResolver::visit(PrimitiveType &type) {
    return &type;
}

BaseTypeVisitorType TypeResolver::visit(UserDefinedType &type) {
    return &type;
}

BaseTypeVisitorType TypeResolver::visit(ListType &type) {
    if (type.contained->type_tag() == NodeType::TypeofType) {
        auto *contained = dynamic_cast<TypeofType *>(type.contained.get());
        return resolve(contained->expr.get()).info;
    } else {
        return &type;
    }
}

BaseTypeVisitorType TypeResolver::visit(TypeofType &type) {
    BaseTypeVisitorType typeof_expr = copy_type(resolve(type.expr.get()).info);
    typeof_expr->data.is_const = typeof_expr->data.is_const || type.data.is_const;
    typeof_expr->data.is_ref = typeof_expr->data.is_ref || type.data.is_ref;
    type_scratch_space.emplace_back(typeof_expr); // This is to make sure the newly synthesized type is managed properly
    return typeof_expr;
}