/* See LICENSE at project root for license details */
#include "TypeResolver.hpp"

#include "../ErrorLogger/ErrorLogger.hpp"

#include <algorithm>
#include <stdexcept>
#include <string_view>

#define allocate_node(T, ...)                                                                                          \
    new T {                                                                                                            \
        __VA_ARGS__                                                                                                    \
    }

#define unreachable() __builtin_unreachable()

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

    ~scoped_boolean_manager() {
        scoped_bool = previous_value;
    }
};

struct scoped_scope_manager {
    TypeResolver &resolver;
    scoped_scope_manager(TypeResolver &resolver) : resolver{resolver} {
        resolver.begin_scope();
    }
    ~scoped_scope_manager() {
        resolver.end_scope();
    }
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

void TypeResolver::check(std::vector<stmt_node_t> &program) {
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
    return stmt->accept(*this);
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
        for (type_node_t &existing_type : type_scratch_space) {
            if (existing_type->data.type == type && existing_type->data.is_const == is_const &&
                existing_type->data.is_ref == is_ref) {
                return existing_type.get();
            }
        }
        type_scratch_space.emplace_back(allocate_node(T, SharedData{type, is_const, is_ref}));
    }
    return type_scratch_space.back().get();
}

ExprVisitorType TypeResolver::visit(AccessExpr &expr) {
    for (auto &module : current_module.imported) {
        std::string module_name = module.name.substr(0, module.name.find_last_of('.'));
        if (module_name == expr.module.lexeme) {
            for (FunctionStmt *func : module.functions) {
                if (func->name.lexeme == expr.name.lexeme) {
                    return {make_new_type<PrimitiveType>(Type::FUNCTION, true, false), func, expr.name};
                }
            }

            for (ClassStmt *class_ : module.classes) {
                if (class_->name.lexeme == expr.name.lexeme) {
                    return {make_new_type<PrimitiveType>(Type::FUNCTION, true, false), class_, expr.name};
                }
            }
        }
    }

    error("No such function/class exists in any imported module", expr.name);
    throw TypeException{"No such function/class exists in any imported module"};
}

ExprVisitorType TypeResolver::visit(AssignExpr &expr) {
    for (auto it = values.end(); it > values.begin(); it--) {
        if (it->lexeme == expr.target.lexeme) {
            ExprVisitorType value = resolve(expr.value.get());
            if (value.info->data.is_const) {
                error("Cannot assign to a const variable", expr.target);
            } else if (!convertible_to(it->info, value.info, value.is_lvalue, expr.target)) {
                error("Cannot convert type of value to type of target", expr.target);
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
                error("Wrong types of arguments to bitwise binary operators (expected integral arguments)", expr.oper);
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

ExprVisitorType TypeResolver::check_inbuilt(VariableExpr *function, const Token &oper, std::vector<expr_node_t> &args) {
    using namespace std::string_literals;
    if (function->name.lexeme == "print") {
        for (auto &arg : args) {
            ExprVisitorType type = resolve(arg.get());
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

        ExprVisitorType arg_type = resolve(args[0].get());
        if (!one_of(arg_type.info->data.type, Type::INT, Type::FLOAT, Type::STRING, Type::BOOL)) {
            error("Expected one of integral, floating, string or boolean arguments to be passed to builtin "
                  "function '"s +
                      function->name.lexeme + "'"s,
                oper);
        }

        return ExprVisitorType{make_new_type<PrimitiveType>(Type::INT, true, false), function->name};
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
    if (callee.func == nullptr && callee.class_ == nullptr) {
        error("Expected function to be called in call expression", callee.lexeme);
        throw TypeException{"Expected function to be called in call expression"};
    } else if (callee.func != nullptr && callee.func->params.size() != expr.args.size()) {
        error("Number of arguments passed to function must match the number of parameters", expr.paren);
        throw TypeException{"Number of arguments passed to function must match the number of parameters"};
    } else if (callee.func != nullptr) {
        for (std::size_t i{0}; i < expr.args.size(); i++) {
            ExprVisitorType argument = resolve(expr.args[i].get());
            if (!convertible_to(
                    callee.func->params[i].second.get(), argument.info, argument.is_lvalue, argument.lexeme)) {
                error("Type of argument is not convertible to type of parameter", argument.lexeme);
            }
        }

        return {callee.func->return_type.get(), callee.func, expr.paren};
    } else {
        FunctionStmt *ctor = nullptr;
        for (auto &method_decl : callee.class_->methods) {
            auto *method = dynamic_cast<FunctionStmt *>(method_decl.first.get());
            if (method->name.lexeme == callee.class_->name.lexeme) {
                if (method_decl.second == VisibilityType::PUBLIC ||
                    (in_class && current_class->name.lexeme == method->name.lexeme)) {
                    ctor = method;
                    break;
                } else if (method_decl.second == VisibilityType::PROTECTED) {
                    error("Cannot access protected constructor outside class", callee.lexeme);
                } else if (method_decl.second == VisibilityType::PRIVATE) {
                    error("Cannot access private constructor outside class", callee.lexeme);
                }
                ctor = method;
                break;
            }
        }

        if (ctor != nullptr) {
            for (std::size_t i{0}; i < expr.args.size(); i++) {
                ExprVisitorType argument = resolve(expr.args[i].get());
                if (!convertible_to(ctor->params[i].second.get(), argument.info, argument.is_lvalue, argument.lexeme)) {
                    error("Type of argument is not convertible to type of parameter", argument.lexeme);
                }
            }
            return {ctor->return_type.get(), callee.class_, expr.paren};
        } else {
            error("No such class exists to be constructed", callee.lexeme);
            throw TypeException{"No such class exists to be constructed"};
        }
    }
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
        auto *type = dynamic_cast<UserDefinedType *>(object.info);
        ClassStmt *user_type = object.class_;

        for (ClassStmt *stored_type : classes) {
            if (stored_type->name.lexeme == type->name.lexeme) {
                user_type = stored_type;
                break;
            }
        }

        if (user_type == nullptr) {
            error("Cannot find class with the given name", type->name);
            throw TypeException{"Cannot find class with the given name"};
        }

        for (auto &member_decl : user_type->members) {
            auto *member = dynamic_cast<VarStmt *>(member_decl.first.get());
            if (member->name.lexeme == name.lexeme) {
                if (member_decl.second == VisibilityType::PUBLIC ||
                    (in_class && current_class->name.lexeme == type->name.lexeme)) {
                    return {member->type.get(), name};
                } else if (member_decl.second == VisibilityType::PROTECTED) {
                    error("Cannot access protected member outside class", name);
                } else if (member_decl.second == VisibilityType::PRIVATE) {
                    error("Cannot access private member outside class", name);
                }
                return {member->type.get(), name};
            }
        }

        for (auto &method_decl : user_type->methods) {
            auto *method = dynamic_cast<FunctionStmt *>(method_decl.first.get());
            if (method->name.lexeme == name.lexeme) {
                if ((method_decl.second == VisibilityType::PUBLIC) ||
                    (in_class && current_class->name.lexeme == type->name.lexeme)) {
                    return {method->return_type.get(), name};
                } else if (method_decl.second == VisibilityType::PROTECTED) {
                    error("Cannot access protected method outside class", name);
                } else if (method_decl.second == VisibilityType::PRIVATE) {
                    error("Cannot access private method outside class", name);
                }
                return {method->return_type.get(), name};
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
    switch (expr.value.value.index()) {
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

ExprVisitorType TypeResolver::visit(SetExpr &expr) {
    ExprVisitorType object = resolve(expr.object.get());
    ExprVisitorType attribute_type = resolve_class_access(object, expr.name);
    ExprVisitorType value_type = resolve(expr.value.get());

    if (attribute_type.info->data.is_const) {
        error("Cannot assign to const attribute", expr.name);
    } else if (!convertible_to(attribute_type.info, value_type.info, value_type.is_lvalue, expr.name)) {
        error("Cannot convert value of assigned expresion to type of target", expr.name);
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
    return {make_new_type<UserDefinedType>(Type::CLASS, false, false, current_class->name), expr.keyword};
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
            using namespace std::string_literals;
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
            expr.scope_depth = it->scope_depth;
            return {it->info, it->class_, expr.name, true};
        }
    }

    for (FunctionStmt *func : functions) {
        if (func->name.lexeme == expr.name.lexeme) {
            return {make_new_type<PrimitiveType>(Type::FUNCTION, true, false), func, expr.name};
        }
    }

    for (ClassStmt *class_ : classes) {
        if (class_->name.lexeme == expr.name.lexeme) {
            return {make_new_type<PrimitiveType>(Type::FUNCTION, true, false), class_, expr.name};
        }
    }

    error("No such variable/function in the current module's scope", expr.name);
    throw TypeException{"No such variable/function in the current module's scope"};
}

StmtVisitorType TypeResolver::visit(BlockStmt &stmt) {
    scoped_scope_manager manager{*this};
    for (auto &statement : stmt.stmts) {
        if (statement != nullptr) {
            resolve(statement.get());
        }
    }
}

StmtVisitorType TypeResolver::visit(BreakStmt &) {}

StmtVisitorType TypeResolver::visit(ClassStmt &stmt) {
    scoped_boolean_manager class_manager{in_class};

    struct scoped_class_manager {
        ClassStmt *&managed_class;
        ClassStmt *previous_value{nullptr};
        scoped_class_manager(ClassStmt *(&current_class), ClassStmt *stmt)
            : managed_class{current_class}, previous_value{current_class} {
            current_class = stmt;
        }
        ~scoped_class_manager() {
            managed_class = previous_value;
        }
    } pointer_manager{current_class, &stmt};

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
    scoped_boolean_manager function_manager{in_function};

    struct scoped_function_manager {
        FunctionStmt *&managed_class;
        FunctionStmt *previous_value{nullptr};
        scoped_function_manager(FunctionStmt *(&current_class), FunctionStmt *stmt)
            : managed_class{current_class}, previous_value{current_class} {
            current_class = stmt;
        }
        ~scoped_function_manager() {
            managed_class = previous_value;
        }
    } pointer_manager{current_function, &stmt};

    scoped_scope_manager manager{*this};

    for (const auto &param : stmt.params) {
        values.push_back({param.first.lexeme, param.second.get(), scope_depth});
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

    if (stmt.initializer != nullptr && stmt.type != nullptr) {
        QualifiedTypeInfo type = resolve(stmt.type.get());
        ExprVisitorType initializer = resolve(stmt.initializer.get());
        if (!convertible_to(type, initializer.info, initializer.is_lvalue, stmt.name)) {
            error("Cannot convert from initializer type to type of variable", stmt.name);
        }
        if (!in_class || in_function) {
            values.push_back({stmt.name.lexeme, type, scope_depth, initializer.class_});
        }
    } else if (stmt.initializer != nullptr) {
        ExprVisitorType initializer = resolve(stmt.initializer.get());
        if (!in_class || in_function) {
            values.push_back({stmt.name.lexeme, initializer.info, scope_depth, initializer.class_});
        }
    } else if (stmt.type != nullptr) {
        QualifiedTypeInfo type = resolve(stmt.type.get());
        if (!in_class || in_function) {
            values.push_back({stmt.name.lexeme, type, scope_depth});
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