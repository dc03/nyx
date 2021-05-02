/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "TypeResolver.hpp"

#include "../Common.hpp"
#include "../ErrorLogger/ErrorLogger.hpp"
#include "../VM/Natives.hpp"
#include "Parser.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string_view>

struct TypeException : public std::runtime_error {
    explicit TypeException(std::string_view string) : std::runtime_error{std::string{string}} {}
};

struct ScopedBooleanManager {
    bool &scoped_bool;
    bool previous_value{};
    explicit ScopedBooleanManager(bool &controlled) : scoped_bool{controlled} {
        previous_value = controlled;
        controlled = true;
    }

    ~ScopedBooleanManager() { scoped_bool = previous_value; }
};

struct ScopedScopeManager {
    TypeResolver &resolver;
    explicit ScopedScopeManager(TypeResolver &resolver) : resolver{resolver} { resolver.begin_scope(); }
    ~ScopedScopeManager() { resolver.end_scope(); }
};

TypeResolver::TypeResolver(Module &module)
    : current_module{module}, classes{module.classes}, functions{module.functions} {}

////////////////////////////////////////////////////////////////////////////////

bool TypeResolver::convertible_to(
    QualifiedTypeInfo to, QualifiedTypeInfo from, bool from_lvalue, const Token &where, bool in_initializer) {
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

    if (to->data.is_ref &&
        in_initializer) { // Only need to check conversion between references when we are in an initializer
        if (!from_lvalue && !from->data.is_ref) {
            error("Cannot bind reference to non l-value type object", where);
            return false;
        } else if (from->data.is_const && !to->data.is_const) {
            error("Cannot bind non-const reference to constant object", where);
            return false;
        }

        if (from->data.primitive == Type::LIST && to->data.primitive == Type::LIST) {
            return are_equivalent_types(
                dynamic_cast<ListType *>(from)->contained.get(), dynamic_cast<ListType *>(to)->contained.get());
        }

        return from->data.primitive == to->data.primitive && class_condition;
    } else if ((from->data.primitive == Type::FLOAT && to->data.primitive == Type::INT) ||
               (from->data.primitive == Type::INT && to->data.primitive == Type::FLOAT)) {
        warning("Implicit conversion between float and int", where);
        return true;
    } else if (from->data.primitive == Type::LIST && to->data.primitive == Type::LIST) {
        return are_equivalent_types(
            dynamic_cast<ListType *>(from)->contained.get(), dynamic_cast<ListType *>(to)->contained.get());
    } else {
        return from->data.primitive == to->data.primitive && class_condition;
    }
}

void show_conversion_note(QualifiedTypeInfo from, QualifiedTypeInfo to) {
    using namespace std::string_literals;
    std::string note_message = "Trying to convert from '"s + stringify(from) + "' to '" + stringify(to) + "'";
    note(note_message);
}

void show_equality_note(QualifiedTypeInfo from, QualifiedTypeInfo to) {
    using namespace std::string_literals;
    std::string note_message = "Trying to check equality of '"s + stringify(from) + "' and '" + stringify(to) + "'";
    note(note_message);
}

bool is_builtin_type(Type type) {
    switch (type) {
        case Type::BOOL:
        case Type::FLOAT:
        case Type::INT:
        case Type::STRING:
        case Type::NULL_: return true;
        default: return false;
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
BaseType *TypeResolver::make_new_type(Type type, bool is_const, bool is_ref, Args &&...args) {
    if constexpr (sizeof...(args) > 0) {
        type_scratch_space.emplace_back(
            allocate_node(T, SharedData{type, is_const, is_ref}, std::forward<Args>(args)...));
    } else {
        for (TypeNode &existing_type : type_scratch_space) {
            if (existing_type->data.primitive == type && existing_type->data.is_const == is_const &&
                existing_type->data.is_ref == is_ref) {
                return existing_type.get();
            }
        }
        type_scratch_space.emplace_back(allocate_node(T, SharedData{type, is_const, is_ref}));
    }
    return type_scratch_space.back().get();
}

void TypeResolver::replace_if_typeof(TypeNode &type) {
    if (type->type_tag() == NodeType::TypeofType) {
        resolve(type.get());
        using std::swap;
        type.swap(type_scratch_space.back());
    }
}

void TypeResolver::infer_list_type(ListExpr *of, ListType *from) {
    if (!are_equivalent_primitives(of->type.get(), from)) {
        return; // Need to have exact same primitives, i.e. same dimension lists storing the same type of elements
    }
    if (from->data.is_ref) {
        return; // Cannot make reference to a ListExpr
    }
    if (from->contained->data.is_ref &&
        std::all_of(of->elements.begin(), of->elements.end(), [](const ListExpr::ElementType &elem) {
            return std::get<ExprNode>(elem)->resolved.is_lvalue || std::get<ExprNode>(elem)->resolved.info->data.is_ref;
        })) {
        // If there is a ListExpr consisting solely of references or lvalues, it can be safely inferred as a list of
        // references if it is being stored in a name with a reference type
        of->type->contained->data.is_ref = true;
        if (std::any_of(of->elements.cbegin(), of->elements.cend(), [](const ListExpr::ElementType &elem) {
                return std::get<ExprNode>(elem)->resolved.info->data.is_const;
            })) {
            of->type->contained->data.is_const = true;
            // A list of references has all its elements become const if any one of them are const
        }
    }

    of->type->contained->data.is_const = of->type->contained->data.is_const || from->contained->data.is_const;
    of->type->data.is_const = of->type->data.is_const || from->data.is_const;
    // Infer const-ness

    // Decide if copy is needed after inferring type
    for (ListExpr::ElementType &element : of->elements) {
        if (!is_builtin_type(of->type->contained->data.primitive)) {
            if (!of->type->contained->data.is_ref && std::get<ExprNode>(element)->resolved.is_lvalue) {
                std::get<RequiresCopy>(element) = true;
            }
        }
    }
}

bool TypeResolver::are_equivalent_primitives(QualifiedTypeInfo first, QualifiedTypeInfo second) {
    if (first->data.primitive == second->data.primitive) {
        if (first->data.primitive == Type::LIST && second->data.primitive == Type::LIST) {
            auto *first_contained = dynamic_cast<ListType *>(first)->contained.get();
            auto *second_contained = dynamic_cast<ListType *>(second)->contained.get();
            if (first_contained->data.primitive == Type::LIST && second_contained->data.primitive == Type::LIST) {
                return are_equivalent_primitives(first_contained, second_contained);
            } else {
                return first_contained->data.primitive == second_contained->data.primitive;
            }
        } else {
            return true;
        }
    }
    return false;
}

bool TypeResolver::are_equivalent_types(QualifiedTypeInfo first, QualifiedTypeInfo second) {
    if (first->data.primitive == Type::LIST && second->data.primitive == Type::LIST) {
        return are_equivalent_types(dynamic_cast<ListType *>(first)->contained.get(),
                   dynamic_cast<ListType *>(second)->contained.get()) &&
               (first->data.is_const == second->data.is_const) && (first->data.is_ref == second->data.is_ref);
    } else {
        return are_equivalent_primitives(first, second) && (first->data.is_const == second->data.is_const) &&
               (first->data.is_ref == second->data.is_ref);
    }
}

template <typename T, typename... Args>
bool one_of(T type, Args... args) {
    const std::array arr{args...};
    return std::any_of(arr.begin(), arr.end(), [type](const auto &arg) { return type == arg; });
}

ExprVisitorType TypeResolver::visit(AssignExpr &expr) {
    auto it = values.end() - 1;
    for (; it >= values.begin(); it--) {
        if (it->lexeme == expr.target.lexeme) {
            break;
        }
        if (it->scope_depth == 0) {
            expr.target_type = IdentifierType::GLOBAL;
        } else {
            expr.target_type = IdentifierType::LOCAL;
        }
    }

    if (it < values.begin()) {
        error("No such variable in the current scope", expr.target);
        throw TypeException{"No such variable in the current scope"};
    }

    ExprVisitorType value = resolve(expr.value.get());
    if (it->info->data.is_const) {
        error("Cannot assign to a const variable", expr.resolved.token);
    } else if (!convertible_to(it->info, value.info, value.is_lvalue, expr.target, false)) {
        error("Cannot convert type of value to type of target", expr.resolved.token);
        show_conversion_note(value.info, it->info);
    } else if (one_of(expr.resolved.token.type, TokenType::PLUS_EQUAL, TokenType::MINUS_EQUAL, TokenType::STAR_EQUAL,
                   TokenType::SLASH_EQUAL) &&
               !one_of(it->info->data.primitive, Type::INT, Type::FLOAT) &&
               !one_of(value.info->data.primitive, Type::INT, Type::FLOAT)) {
        error("Expected integral types for compound assignment operator", expr.resolved.token);
        throw TypeException{"Expected integral types for compound assignment operator"};
    } else if (value.info->data.primitive == Type::FLOAT && it->info->data.primitive == Type::INT) {
        expr.conversion_type = NumericConversionType::FLOAT_TO_INT;
    } else if (value.info->data.primitive == Type::INT && it->info->data.primitive == Type::FLOAT) {
        expr.conversion_type = NumericConversionType::INT_TO_FLOAT;
    }

    if (!is_builtin_type(value.info->data.primitive)) {
        if (expr.value->resolved.is_lvalue || expr.value->resolved.info->data.is_ref) {
            expr.requires_copy = true;
        }
    }
    // Assignment leads to copy when the primitive is not an inbuilt one as those are implicitly copied when the
    // values are pushed onto the stack
    expr.resolved.info = it->info;
    expr.resolved.stack_slot = it->stack_slot;
    return expr.resolved;
}

ExprVisitorType TypeResolver::visit(BinaryExpr &expr) {
    ExprVisitorType left_expr = resolve(expr.left.get());
    ExprVisitorType right_expr = resolve(expr.right.get());
    switch (expr.resolved.token.type) {
        case TokenType::LEFT_SHIFT:
        case TokenType::RIGHT_SHIFT:
        case TokenType::BIT_AND:
        case TokenType::BIT_OR:
        case TokenType::BIT_XOR:
        case TokenType::MODULO:
            if (left_expr.info->data.primitive != Type::INT || right_expr.info->data.primitive != Type::INT) {
                if (expr.resolved.token.type == TokenType::MODULO) {
                    error("Wrong types of arguments to modulo operator (expected integral arguments)",
                        expr.resolved.token);
                } else {
                    error("Wrong types of arguments to bitwise binary operator (expected integral arguments)",
                        expr.resolved.token);
                }
            }
            return expr.resolved = {left_expr.info, expr.resolved.token};
        case TokenType::NOT_EQUAL:
        case TokenType::EQUAL_EQUAL:
            if (left_expr.info->data.primitive == Type::LIST && right_expr.info->data.primitive == Type::LIST) {
                if (!convertible_to(left_expr.info, right_expr.info, false, expr.resolved.token, false) &&
                    !convertible_to(right_expr.info, left_expr.info, false, expr.resolved.token, false)) {
                    error("Cannot compare two lists that have incompatible contained types", expr.resolved.token);
                    show_equality_note(left_expr.info, right_expr.info);
                }
                return expr.resolved = {make_new_type<PrimitiveType>(Type::BOOL, true, false), expr.resolved.token};
            } else if (one_of(left_expr.info->data.primitive, Type::BOOL, Type::STRING, Type::NULL_)) {
                if (left_expr.info->data.primitive != right_expr.info->data.primitive) {
                    error("Cannot compare equality of objects of different types", expr.resolved.token);
                }
                return expr.resolved = {make_new_type<PrimitiveType>(Type::BOOL, true, false), expr.resolved.token};
            }
            [[fallthrough]];
        case TokenType::GREATER:
        case TokenType::GREATER_EQUAL:
        case TokenType::LESS:
        case TokenType::LESS_EQUAL:
            if (one_of(left_expr.info->data.primitive, Type::INT, Type::FLOAT) &&
                one_of(right_expr.info->data.primitive, Type::INT, Type::FLOAT)) {
                if (left_expr.info->data.primitive != right_expr.info->data.primitive) {
                    warning("Comparison between objects of types int and float", expr.resolved.token);
                }
                return expr.resolved = {make_new_type<PrimitiveType>(Type::BOOL, true, false), expr.resolved.token};
            } else if (left_expr.info->data.primitive == Type::BOOL && right_expr.info->data.primitive == Type::BOOL) {
                return expr.resolved = {make_new_type<PrimitiveType>(Type::BOOL, true, false), expr.resolved.token};
            } else {
                error("Cannot compare objects of incompatible types", expr.resolved.token);
                return expr.resolved = {make_new_type<PrimitiveType>(Type::BOOL, true, false), expr.resolved.token};
            }
        case TokenType::PLUS:
            if (left_expr.info->data.primitive == Type::STRING && right_expr.info->data.primitive == Type::STRING) {
                return expr.resolved = {make_new_type<PrimitiveType>(Type::STRING, true, false), expr.resolved.token};
            }
            [[fallthrough]];
        case TokenType::MINUS:
        case TokenType::SLASH:
        case TokenType::STAR:
            if (one_of(left_expr.info->data.primitive, Type::INT, Type::FLOAT) &&
                one_of(right_expr.info->data.primitive, Type::INT, Type::FLOAT)) {
                if (left_expr.info->data.primitive == Type::INT && right_expr.info->data.primitive == Type::INT) {
                    return expr.resolved = {make_new_type<PrimitiveType>(Type::INT, true, false), expr.resolved.token};
                }
                return expr.resolved = {make_new_type<PrimitiveType>(Type::FLOAT, true, false), expr.resolved.token};
                // Integral promotion
            } else {
                error("Cannot use arithmetic operators on objects of incompatible types", expr.resolved.token);
                throw TypeException{"Cannot use arithmetic operators on objects of incompatible types"};
            }
        case TokenType::DOT_DOT:
        case TokenType::DOT_DOT_EQUAL:
            if (left_expr.info->data.primitive == Type::INT && right_expr.info->data.primitive == Type::INT) {
                BaseType *list = make_new_type<ListType>(
                    Type::LIST, true, false, TypeNode{make_new_type<PrimitiveType>(Type::INT, true, false)}, nullptr);
                return expr.resolved = {list, expr.resolved.token};
            } else {
                error("Ranges can only be created for integral types", expr.resolved.token);
                throw TypeException{"Ranges can only be created for integral types"};
            }
            break;

        default:
            error("Bug in parser with illegal token type of expression's operator", expr.resolved.token);
            throw TypeException{"Bug in parser with illegal token type of expression's operator"};
    }
}

bool is_builtin_function(VariableExpr *expr) {
    return std::any_of(native_functions.begin(), native_functions.end(),
        [&expr](const NativeFn &native) { return native.name == expr->name.lexeme; });
}

ExprVisitorType TypeResolver::check_inbuilt(
    VariableExpr *function, const Token &oper, std::vector<std::tuple<ExprNode, NumericConversionType, bool>> &args) {
    using namespace std::string_literals;

    auto it = std::find_if(native_functions.begin(), native_functions.end(),
        [&function](const NativeFn &native) { return native.name == function->name.lexeme; });

    if (args.size() != it->arity) {
        using namespace std::string_literals;
        std::string arity_error = "Cannot pass "s + (args.size() < it->arity ? "less"s : "more"s) + " than "s +
                                  std::to_string(it->arity) + " argument(s) to function '"s + it->name + "'"s;
        error(arity_error, oper);
        throw TypeException{arity_error};
    }

    for (std::size_t i = 0; i < it->arity; i++) {
        ExprVisitorType arg = resolve(std::get<ExprNode>(args[i]).get());
        if (!std::any_of(it->arguments[i].begin(), it->arguments[i].end(),
                [&arg](const Type &type) { return type == arg.info->data.primitive; })) {
            using namespace std::string_literals;
            std::string type_error = "Cannot pass argument of type '"s + stringify(arg.info) +
                                     "' as argument number "s + std::to_string(i + 1) + " to builtin function '"s +
                                     it->name + "'";
            error(type_error, oper);
            throw TypeException{type_error};
        }
    }

    return ExprVisitorType{make_new_type<PrimitiveType>(it->return_type, true, false), function->name};
}

ExprVisitorType TypeResolver::visit(CallExpr &expr) {
    if (expr.function->type_tag() == NodeType::VariableExpr) {
        auto *function = dynamic_cast<VariableExpr *>(expr.function.get());
        if (is_builtin_function(function)) {
            expr.is_native_call = true;
            return expr.resolved = check_inbuilt(function, expr.resolved.token, expr.args);
        }
    }

    ExprVisitorType callee = resolve(expr.function.get());

    FunctionStmt *called = callee.func;
    if (callee.class_ != nullptr) {
        for (auto &method_decl : callee.class_->methods) {
            if (method_decl.first->name == callee.token) {
                called = method_decl.first.get();
            }
        }
    }

    if (expr.function->type_tag() == NodeType::GetExpr) {
        auto *get = dynamic_cast<GetExpr *>(expr.function.get());
        expr.args.insert(expr.args.begin(),
            {std::move(get->object), NumericConversionType::NONE, !called->params[0].second->data.is_ref});
    }

    if (called->params.size() != expr.args.size()) {
        error("Number of arguments passed to function must match the number of parameters", expr.resolved.token);
        throw TypeException{"Number of arguments passed to function must match the number of parameters"};
    }

    for (std::size_t i{0}; i < expr.args.size(); i++) {
        ExprVisitorType argument = resolve(std::get<ExprNode>(expr.args[i]).get());
        if (!convertible_to(called->params[i].second.get(), argument.info, argument.is_lvalue, argument.token, true)) {
            error("Type of argument is not convertible to type of parameter", argument.token);
            show_conversion_note(argument.info, called->params[i].second.get());
        } else if (argument.info->data.primitive == Type::FLOAT &&
                   called->params[i].second->data.primitive == Type::INT) {
            std::get<NumericConversionType>(expr.args[i]) = NumericConversionType::FLOAT_TO_INT;
        } else if (argument.info->data.primitive == Type::INT &&
                   called->params[i].second->data.primitive == Type::FLOAT) {
            std::get<NumericConversionType>(expr.args[i]) = NumericConversionType::INT_TO_FLOAT;
        }

        auto &param = called->params[i];
        if (!is_builtin_type(param.second->data.primitive)) {
            if (param.second->data.is_ref) {
                std::get<RequiresCopy>(expr.args[i]) = false; // A reference binding to anything does not need a copy
            } else if (argument.is_lvalue) {
                std::get<RequiresCopy>(expr.args[i]) =
                    true; // A copy is made when initializing from an lvalue without a reference
            }
        }
    }

    return expr.resolved = {called->return_type.get(), called, callee.class_, expr.resolved.token};
}

ExprVisitorType TypeResolver::visit(CommaExpr &expr) {
    auto it = begin(expr.exprs);

    for (auto next = std::next(it); next != end(expr.exprs); it = next, ++next)
        resolve(it->get());

    return expr.resolved = resolve(it->get());
}

ExprVisitorType TypeResolver::resolve_class_access(ExprVisitorType &object, const Token &name) {
    if (object.info->data.primitive == Type::CLASS) {
        ClassStmt *accessed_type = object.class_;

        for (auto &member_decl : accessed_type->members) {
            auto *member = member_decl.first.get();
            if (member->name == name) {
                if (!in_class || (in_class && current_class->name != accessed_type->name)) {
                    if (member_decl.second == VisibilityType::PROTECTED) {
                        error("Cannot access protected member outside class", name);
                    } else if (member_decl.second == VisibilityType::PRIVATE) {
                        error("Cannot access private member outside class", name);
                    }
                }

                if (member->type->type_tag() == NodeType::UserDefinedType) {
                    return {member->type.get(),
                        find_class(dynamic_cast<UserDefinedType *>(member->type.get())->name.lexeme), name, true};
                }
                return {member->type.get(), name, true};
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
}

ExprVisitorType TypeResolver::visit(GetExpr &expr) {
    ExprVisitorType object = resolve(expr.object.get());
    return expr.resolved = resolve_class_access(object, expr.name);
}

ExprVisitorType TypeResolver::visit(GroupingExpr &expr) {
    return expr.resolved = resolve(expr.expr.get());
}

ExprVisitorType TypeResolver::visit(IndexExpr &expr) {
    ExprVisitorType list = resolve(expr.object.get());
    // I think calling a string a list is fair since its technically just a list of chars

    ExprVisitorType index = resolve(expr.index.get());

    if (index.info->data.primitive != Type::INT) {
        error("Expected integral type for index", expr.resolved.token);
        throw TypeException{"Expected integral type for index"};
    }

    if (list.info->data.primitive == Type::LIST) {
        auto *contained_type = dynamic_cast<ListType *>(list.info)->contained.get();
        return expr.resolved = {contained_type, expr.resolved.token, true};
    } else if (list.info->data.primitive == Type::STRING) {
        return expr.resolved = {list.info, expr.resolved.token, false}; // For now, strings are immutable.
    } else {
        error("Expected list or string type for indexing", expr.resolved.token);
        throw TypeException{"Expected list or string type for indexing"};
    }
}

ExprVisitorType TypeResolver::visit(ListExpr &expr) {
    if (expr.elements.empty()) {
        error("Cannot have empty list expression", expr.bracket);
        throw TypeException{"Cannot have empty list expression"};
    } else if (expr.elements.size() > 255) {
        error("Cannot have more than 255 elements in list expression", expr.bracket);
        throw TypeException{"Cannot have more than 255 elements in list expression"};
    }

    auto *list_size = allocate_node(LiteralExpr, LiteralValue{static_cast<int>(expr.elements.size())},
        TypeNode{allocate_node(PrimitiveType, {Type::INT, true, false})});

    expr.type.reset(allocate_node(ListType, {Type::LIST, false, false},
        TypeNode{copy_type(resolve(std::get<ExprNode>(*expr.elements.begin()).get()).info)}, ExprNode{list_size}));

    for (std::size_t i = 1; i < expr.elements.size(); i++) {
        resolve(std::get<ExprNode>(expr.elements[i]).get()); // Will store the type info in the 'resolved' class member
    }

    if (std::none_of(expr.elements.begin(), expr.elements.end(),
            [](const ListExpr::ElementType &x) { return std::get<ExprNode>(x)->resolved.info->data.is_ref; })) {
        // If there are no references in the list, the individual elements can safely be made non-const
        expr.type->contained->data.is_const = false;
    }

    for (ListExpr::ElementType &element : expr.elements) {
        if (std::get<ExprNode>(element)->resolved.info->data.primitive == Type::INT &&
            expr.type->contained->data.primitive == Type::FLOAT) {
            std::get<NumericConversionType>(element) = NumericConversionType::INT_TO_FLOAT;
        } else if (std::get<ExprNode>(element)->resolved.info->data.primitive == Type::FLOAT &&
                   expr.type->contained->data.primitive == Type::INT) {
            std::get<NumericConversionType>(element) = NumericConversionType::FLOAT_TO_INT;
        }
    }
    return expr.resolved = {expr.type.get(), expr.bracket, false};
}

ExprVisitorType TypeResolver::visit(ListAssignExpr &expr) {
    ExprVisitorType contained = resolve(&expr.list);
    ExprVisitorType value = resolve(expr.value.get());
    if (!contained.is_lvalue) {
        error("Cannot assign to non-lvalue element", expr.resolved.token);
        note("String elements are non-assignable");
        throw TypeException{"Cannot assign to non-lvalue element"};
    }

    if (contained.info->data.is_const) {
        error("Cannot assign to constant value", expr.resolved.token);
        show_conversion_note(value.info, contained.info);
        throw TypeException{"Cannot assign to constant value"};
    } else if (expr.list.object->resolved.info->data.is_const) {
        error("Cannot assign to constant list", expr.resolved.token);
        show_conversion_note(value.info, expr.list.object->resolved.info);
        throw TypeException{"Cannot assign to constant list"};
    } else if (!convertible_to(contained.info, value.info, value.is_lvalue, expr.resolved.token, false)) {
        error("Cannot convert from contained type of list to type being assigned", expr.resolved.token);
        show_conversion_note(contained.info, value.info);
        throw TypeException{"Cannot convert from contained type of list to type being assigned"};
    }

    return expr.resolved = {contained.info, expr.resolved.token, false};
}

ExprVisitorType TypeResolver::visit(LiteralExpr &expr) {
    switch (expr.value.index()) {
        case LiteralValue::tag::INT:
        case LiteralValue::tag::DOUBLE:
        case LiteralValue::tag::STRING:
        case LiteralValue::tag::BOOL:
        case LiteralValue::tag::NULL_: return expr.resolved = {expr.type.get(), expr.resolved.token};

        default:
            error("Bug in parser with illegal type for literal value", expr.resolved.token);
            throw TypeException{"Bug in parser with illegal type for literal value"};
    }
}

ExprVisitorType TypeResolver::visit(LogicalExpr &expr) {
    resolve(expr.left.get());
    resolve(expr.right.get());
    return expr.resolved = {make_new_type<PrimitiveType>(Type::BOOL, true, false), expr.resolved.token};
}

ExprVisitorType TypeResolver::visit(ScopeAccessExpr &expr) {
    ExprVisitorType left = resolve(expr.scope.get());

    switch (left.scope_type) {
        case ExprTypeInfo::ScopeType::CLASS:
            for (auto &method : left.class_->methods) {
                if (method.first->name == expr.name) {
                    return expr.resolved = {make_new_type<PrimitiveType>(Type::FUNCTION, true, false),
                               method.first.get(), left.class_, expr.resolved.token};
                }
            }
            error("No such method exists in the class", expr.name);
            throw TypeException{"No such method exists in the class"};

        case ExprTypeInfo::ScopeType::MODULE: {
            auto &module = Parser::parsed_modules[left.module_index].first;
            if (auto class_ = module.classes.find(expr.name.lexeme); class_ != module.classes.end()) {
                return expr.resolved = {
                           make_new_type<PrimitiveType>(Type::CLASS, true, false), class_->second, expr.resolved.token};
            }

            if (auto func = module.functions.find(expr.name.lexeme); func != module.functions.end()) {
                return expr.resolved = {make_new_type<PrimitiveType>(Type::FUNCTION, true, false), func->second,
                           expr.resolved.token};
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
            return expr.resolved = {make_new_type<PrimitiveType>(Type::MODULE, true, false), i, expr.resolved.token};
        }
    }

    if (ClassStmt *class_ = find_class(expr.name.lexeme); class_ != nullptr) {
        return expr.resolved = {make_new_type<PrimitiveType>(Type::CLASS, true, false), class_, expr.resolved.token};
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

    if (!convertible_to(attribute_type.info, value_type.info, value_type.is_lvalue, expr.name, false)) {
        error("Cannot convert value of assigned expresion to type of target", expr.name);
        show_conversion_note(value_type.info, attribute_type.info);
        throw TypeException{"Cannot convert value of assigned expression to type of target"};
    } else if (value_type.info->data.primitive == Type::FLOAT && attribute_type.info->data.primitive == Type::INT) {
        expr.conversion_type = NumericConversionType::FLOAT_TO_INT;
    } else if (value_type.info->data.primitive == Type::INT && attribute_type.info->data.primitive == Type::FLOAT) {
        expr.conversion_type = NumericConversionType::INT_TO_FLOAT;
    }

    expr.requires_copy = !is_builtin_type(value_type.info->data.primitive); // Similar case to AssignExpr
    return expr.resolved = {attribute_type.info, expr.resolved.token};
}

ExprVisitorType TypeResolver::visit(SuperExpr &expr) {
    // TODO: Implement me
    expr.resolved.token = expr.keyword;
    throw TypeException{"Super expressions/inheritance not implemented yet"};
}

ExprVisitorType TypeResolver::visit(TernaryExpr &expr) {
    ExprVisitorType left = resolve(expr.left.get());
    ExprVisitorType middle = resolve(expr.middle.get());
    ExprVisitorType right = resolve(expr.right.get());

    if (!convertible_to(middle.info, right.info, right.is_lvalue, expr.resolved.token, false) &&
        !convertible_to(right.info, middle.info, right.is_lvalue, expr.resolved.token, false)) {
        error("Expected equivalent expression types for branches of ternary expression", expr.resolved.token);
        show_conversion_note(right.info, middle.info);
    }

    return expr.resolved = {middle.info, expr.resolved.token};
}

ExprVisitorType TypeResolver::visit(ThisExpr &expr) {
    if (!in_ctor && !in_dtor) {
        error("Cannot use 'this' keyword outside a class's constructor or destructor", expr.keyword);
        throw TypeException{"Cannot use 'this' keyword outside a class's constructor or destructor"};
    }
    return expr.resolved = {make_new_type<UserDefinedType>(Type::CLASS, false, false, current_class->name),
               current_class, expr.keyword};
}

ExprVisitorType TypeResolver::visit(UnaryExpr &expr) {
    ExprVisitorType right = resolve(expr.right.get());
    switch (expr.oper.type) {
        case TokenType::BIT_NOT:
            if (right.info->data.primitive != Type::INT) {
                error("Wrong type of argument to bitwise unary operator (expected integral argument)", expr.oper);
            }
            return expr.resolved = {make_new_type<PrimitiveType>(Type::INT, true, false), expr.resolved.token};
        case TokenType::NOT:
            if (one_of(right.info->data.primitive, Type::CLASS, Type::LIST, Type::NULL_)) {
                error("Wrong type of argument to logical not operator", expr.oper);
            }
            return expr.resolved = {make_new_type<PrimitiveType>(Type::BOOL, true, false), expr.resolved.token};
        case TokenType::PLUS_PLUS:
        case TokenType::MINUS_MINUS:
            if (!one_of(right.info->data.primitive, Type::INT, Type::FLOAT)) {
                error("Expected integral or floating type as argument to increment operator", expr.oper);
                throw TypeException{"Expected integral or floating type as argument to increment operator"};
            } else if (right.info->data.is_const || !(right.is_lvalue || right.info->data.is_ref)) {
                error("Expected non-const l-value or reference type as argument for increment operator", expr.oper);
                throw TypeException{"Expected non-const l-value or reference type as argument for increment operator"};
            };
            return expr.resolved = {right.info, expr.oper};
        case TokenType::MINUS:
        case TokenType::PLUS:
            if (!one_of(right.info->data.primitive, Type::INT, Type::FLOAT)) {
                error("Expected integral or floating point argument to operator", expr.oper);
                return expr.resolved = {make_new_type<PrimitiveType>(Type::INT, true, false), expr.resolved.token};
            }
            return expr.resolved = {right.info, expr.resolved.token};

        default:
            error("Bug in parser with illegal type for unary expression", expr.oper);
            throw TypeException{"Bug in parser with illegal type for unary expression"};
    }
}

ExprVisitorType TypeResolver::visit(VariableExpr &expr) {
    if (is_builtin_function(&expr)) {
        error("Cannot use in-built function as an expression", expr.name);
        throw TypeException{"Cannot use in-built function as an expression"};
    }

    for (auto it = values.end() - 1; !values.empty() && it >= values.begin(); it--) {
        if (it->lexeme == expr.name.lexeme) {
            if (it->scope_depth == 0) {
                expr.type = IdentifierType::GLOBAL;
            } else {
                expr.type = IdentifierType::LOCAL;
            }
            expr.resolved = {it->info, it->class_, expr.resolved.token, true};
            expr.resolved.stack_slot = it->stack_slot;
            return expr.resolved;
        }
    }

    if (FunctionStmt *func = find_function(expr.name.lexeme); func != nullptr) {
        expr.type = IdentifierType::FUNCTION;
        return expr.resolved = {make_new_type<PrimitiveType>(Type::FUNCTION, true, false), func, expr.resolved.token};
    }

    if (ClassStmt *class_ = find_class(expr.name.lexeme); class_ != nullptr) {
        expr.type = IdentifierType::CLASS;
        return expr.resolved = {make_new_type<PrimitiveType>(Type::CLASS, true, false), class_, expr.resolved.token};
    }

    error("No such variable/function in the current module's scope", expr.name);
    throw TypeException{"No such variable/function in the current module's scope"};
}

StmtVisitorType TypeResolver::visit(BlockStmt &stmt) {
    ScopedScopeManager manager{*this};
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
    ScopedBooleanManager class_manager{in_class};

    ////////////////////////////////////////////////////////////////////////////
    struct ScopedClassManager {
        ClassStmt *&managed_class;
        ClassStmt *previous_value{nullptr};
        ScopedClassManager(ClassStmt *(&current_class), ClassStmt *stmt)
            : managed_class{current_class}, previous_value{current_class} {
            current_class = stmt;
        }
        ~ScopedClassManager() { managed_class = previous_value; }
    } pointer_manager{current_class, &stmt};
    ////////////////////////////////////////////////////////////////////////////

    // Creation of the implicit constructor
    if (stmt.ctor == nullptr) {
        stmt.ctor = allocate_node(FunctionStmt, stmt.name,
            TypeNode{allocate_node(UserDefinedType, {Type::CLASS, false, false}, stmt.name)}, {},
            StmtNode{allocate_node(BlockStmt, {})}, {}, values.empty() ? 0 : values.crbegin()->scope_depth);
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
                {std::move(member->initializer)}, NumericConversionType::NONE, false)};
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
    ScopedScopeManager manager{*this};
    ScopedBooleanManager function_manager{in_function};

    ////////////////////////////////////////////////////////////////////////////
    struct ScopedFunctionManager {
        FunctionStmt *&managed_class;
        FunctionStmt *previous_value{nullptr};
        ScopedFunctionManager(FunctionStmt *(&current_class), FunctionStmt *stmt)
            : managed_class{current_class}, previous_value{current_class} {
            current_class = stmt;
        }
        ~ScopedFunctionManager() { managed_class = previous_value; }
    } pointer_manager{current_function, &stmt};

    bool throwaway{};
    bool is_in_ctor = current_class != nullptr && stmt.name == current_class->name;
    bool is_in_dtor = current_class != nullptr && stmt.name.lexeme[0] == '~' &&
                      (stmt.name.lexeme.substr(1) == current_class->name.lexeme);

    ScopedBooleanManager special_func_manager{is_in_ctor ? in_ctor : (is_in_dtor ? in_dtor : throwaway)};
    ////////////////////////////////////////////////////////////////////////////

    if (!values.empty()) {
        stmt.scope_depth = values.crbegin()->scope_depth + 1;
    }

    replace_if_typeof(stmt.return_type);

    std::size_t i = 0;
    for (auto &param : stmt.params) {
        ClassStmt *param_class = nullptr;
        replace_if_typeof(param.second);

        if (param.second->type_tag() == NodeType::UserDefinedType) {
            param_class = find_class(dynamic_cast<UserDefinedType &>(*param.second).name.lexeme);
            if (param_class == nullptr) {
                error("No such module/class exists in the current global scope", stmt.name);
                throw TypeException{"No such module/class exists in the current global scope"};
            }
        }

        values.push_back({param.first.lexeme, param.second.get(), scope_depth + 1, param_class, i++});
    }

    if (auto *body = dynamic_cast<BlockStmt *>(stmt.body.get());
        (!body->stmts.empty() && body->stmts.back()->type_tag() != NodeType::ReturnStmt) || body->stmts.empty()) {
        // TODO: also for constructors and destructors
        if (stmt.return_type->data.primitive == Type::NULL_) {
            body->stmts.emplace_back(allocate_node(ReturnStmt, stmt.name, nullptr, 0, nullptr));
        }
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
    if (stmt.value == nullptr) {
        if (current_function->return_type->data.primitive != Type::NULL_) {
            error("Can only have empty return expressions in functions which return 'null'", stmt.keyword);
            throw TypeException{"Can only have empty return expressions in functions which return 'null'"};
        }
    } else if (ExprVisitorType return_value = resolve(stmt.value.get());
               !convertible_to(current_function->return_type.get(), return_value.info, return_value.is_lvalue,
                   stmt.keyword, true)) {
        error("Type of expression in return statement does not match return type of function", stmt.keyword);
        show_conversion_note(return_value.info, current_function->return_type.get());
    }

    stmt.locals_popped = std::count_if(values.crbegin(), values.crend(),
        [this](const TypeResolver::Value &x) { return x.scope_depth >= current_function->scope_depth; });
    stmt.function = current_function;
    current_function->return_stmts.push_back(&stmt);
}

StmtVisitorType TypeResolver::visit(SwitchStmt &stmt) {
    ScopedBooleanManager switch_manager{in_switch};

    ExprVisitorType condition = resolve(stmt.condition.get());

    for (auto &case_stmt : stmt.cases) {
        ExprVisitorType case_expr = resolve(case_stmt.first.get());
        if (!convertible_to(case_expr.info, condition.info, condition.is_lvalue, case_expr.token, false)) {
            error("Type of case expression cannot be converted to type of switch condition", case_expr.token);
            show_conversion_note(condition.info, case_expr.info);
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
        throw TypeException{"A variable with the same name has already been created in this scope"};
    }

    if (stmt.initializer != nullptr) {
        ExprVisitorType initializer = resolve(stmt.initializer.get());
        QualifiedTypeInfo type = nullptr;
        bool originally_typeless = stmt.type == nullptr;
        if (stmt.type == nullptr) {
            stmt.type = TypeNode{copy_type(initializer.info)};
            type = stmt.type.get();
        } else {
            replace_if_typeof(stmt.type);
            type = resolve(stmt.type.get());
        }
        switch (stmt.keyword.type) {
            case TokenType::VAR:
                // If there is a var statement without a specified type that is not binding to a reference it is
                // automatically non-const
                if (originally_typeless) {
                    stmt.type->data.is_const = false;
                    stmt.type->data.is_ref = false;
                }
                break;
            case TokenType::CONST: stmt.type->data.is_const = true; break;
            case TokenType::REF: stmt.type->data.is_ref = true; break;
            default: break;
        }

        // Infer some more information about the type of the list expression from the type of the variable if needed
        //  If a variable is defined as `var x: [ref int] = [a, b, c]`, then the list needs to be inferred as a list of
        //  [ref int], not as [int]
        if (stmt.initializer->type_tag() == NodeType::ListExpr && stmt.type->data.primitive == Type::LIST) {
            infer_list_type(
                dynamic_cast<ListExpr *>(stmt.initializer.get()), dynamic_cast<ListType *>(stmt.type.get()));
        }

        if (!convertible_to(type, initializer.info, initializer.is_lvalue, stmt.name, true)) {
            error("Cannot convert from initializer type to type of variable", stmt.name);
            show_conversion_note(initializer.info, type);
            throw TypeException{"Cannot convert from initializer type to type of variable"};
        } else if (initializer.info->data.primitive == Type::FLOAT && type->data.primitive == Type::INT) {
            stmt.conversion_type = NumericConversionType::FLOAT_TO_INT;
        } else if (initializer.info->data.primitive == Type::INT && type->data.primitive == Type::FLOAT) {
            stmt.conversion_type = NumericConversionType::INT_TO_FLOAT;
        }

        if (!is_builtin_type(stmt.type->data.primitive)) {
            if (type->data.is_ref) {
                stmt.requires_copy = false; // A reference binding to anything does not need a copy
            } else if (initializer.is_lvalue) {
                stmt.requires_copy = true; // A copy is made when initializing from an lvalue without a reference
            }
        }

        if (!in_class || in_function) {
            values.push_back({stmt.name.lexeme, type, scope_depth, initializer.class_,
                (values.empty() ? 0 : values.back().stack_slot + 1)});
        }
    } else if (stmt.type != nullptr) {
        replace_if_typeof(stmt.type);
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
            values.push_back(
                {stmt.name.lexeme, type, scope_depth, stmt_class, (values.empty() ? 0 : values.back().stack_slot + 1)});
        }
    } else {
        error("Expected type for variable", stmt.name);
        // The variable is never created if there is an error creating it thus any references to it also break
    }
}

StmtVisitorType TypeResolver::visit(WhileStmt &stmt) {
    // ScopedScopeManager manager{*this};
    ScopedBooleanManager loop_manager{in_loop};

    ExprVisitorType condition = resolve(stmt.condition.get());
    if (one_of(condition.info->data.primitive, Type::CLASS, Type::LIST)) {
        error("Class or list types are not implicitly convertible to bool", stmt.keyword);
    }
    if (stmt.increment != nullptr) {
        resolve(stmt.increment.get());
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
    replace_if_typeof(type.contained);
    resolve(type.contained.get());
    if (type.size != nullptr) {
        resolve(type.size.get());
    }
    return &type;
}

BaseTypeVisitorType TypeResolver::visit(TypeofType &type) {
    BaseTypeVisitorType typeof_expr = copy_type(resolve(type.expr.get()).info);
    typeof_expr->data.is_const = typeof_expr->data.is_const || type.data.is_const;
    typeof_expr->data.is_ref = typeof_expr->data.is_ref || type.data.is_ref;
    type_scratch_space.emplace_back(typeof_expr); // This is to make sure the newly synthesized type is managed properly
    return typeof_expr;
}