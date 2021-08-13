/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "TypeResolver.hpp"

#include "../Common.hpp"
#include "../ErrorLogger/ErrorLogger.hpp"
#include "../VirtualMachine/Natives.hpp"
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

    auto compare_tuples = [this, &to, &from, &from_lvalue, &where, &in_initializer] {
        auto *from_tuple = dynamic_cast<TupleType *>(from);
        auto *to_tuple = dynamic_cast<TupleType *>(to);
        if (to_tuple->types.size() != from_tuple->types.size()) {
            return false;
        }

        for (std::size_t i = 0; i < to_tuple->types.size(); i++) {
            if (not convertible_to(
                    to_tuple->types[i].get(), from_tuple->types[i].get(), from_lvalue, where, in_initializer)) {
                return false;
            }
        }
        return true;
    };

    if (to->is_ref &&
        in_initializer) { // Only need to check conversion between references when we are in an initializer
        if (not from_lvalue && not from->is_ref) {
            error({"Cannot bind reference to non l-value type object"}, where);
            return false;
        } else if (from->is_const && not to->is_const) {
            error({"Cannot bind non-const reference to constant object"}, where);
            return false;
        }

        if (from->primitive == Type::LIST && to->primitive == Type::LIST) {
            return are_equivalent_types(
                dynamic_cast<ListType *>(from)->contained.get(), dynamic_cast<ListType *>(to)->contained.get());
        } else if (from->primitive == Type::TUPLE && to->primitive == Type::TUPLE) {
            return compare_tuples();
        }

        return from->primitive == to->primitive && class_condition;
    } else if ((from->primitive == Type::FLOAT && to->primitive == Type::INT) ||
               (from->primitive == Type::INT && to->primitive == Type::FLOAT)) {
        warning({"Implicit conversion between float and int"}, where);
        return true;
    } else if (from->primitive == Type::LIST && to->primitive == Type::LIST) {
        return are_equivalent_types(
            dynamic_cast<ListType *>(from)->contained.get(), dynamic_cast<ListType *>(to)->contained.get());
    } else if (from->primitive == Type::TUPLE && to->primitive == Type::TUPLE) {
        return compare_tuples();
    } else {
        return from->primitive == to->primitive && class_condition;
    }
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
    while (not values.empty() && values.back().scope_depth == scope_depth) {
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
        type_scratch_space.emplace_back(allocate_node(T, type, is_const, is_ref, std::forward<Args>(args)...));
    } else {
        for (TypeNode &existing_type : type_scratch_space) {
            if (existing_type->primitive == type && existing_type->is_const == is_const &&
                existing_type->is_ref == is_ref) {
                return existing_type.get();
            }
        }
        type_scratch_space.emplace_back(allocate_node(T, type, is_const, is_ref));
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
    if (not are_equivalent_primitives(of->type.get(), from)) {
        return; // Need to have exact same primitives, i.e. same dimension lists storing the same type of elements
    }
    if (from->is_ref) {
        return; // Cannot make reference to a ListExpr
    }
    if (from->contained->is_ref &&
        std::all_of(of->elements.begin(), of->elements.end(), [](const ListExpr::ElementType &elem) {
            return std::get<ExprNode>(elem)->resolved.is_lvalue || std::get<ExprNode>(elem)->resolved.info->is_ref;
        })) {
        // If there is a ListExpr consisting solely of references or lvalues, it can be safely inferred as a list of
        // references if it is being stored in a name with a reference type
        of->type->contained->is_ref = true;
        if (std::any_of(of->elements.cbegin(), of->elements.cend(),
                [](const ListExpr::ElementType &elem) { return std::get<ExprNode>(elem)->resolved.info->is_const; })) {
            of->type->contained->is_const = true;
            // A list of references has all its elements become const if any one of them are const
        }
    }

    of->type->contained->is_const = of->type->contained->is_const || from->contained->is_const;
    of->type->is_const = of->type->is_const || from->is_const;
    // Infer const-ness

    // Decide if copy is needed after inferring type
    for (ListExpr::ElementType &element : of->elements) {
        if (not is_builtin_type(of->type->contained->primitive)) {
            if (not of->type->contained->is_ref && std::get<ExprNode>(element)->resolved.is_lvalue) {
                std::get<RequiresCopy>(element) = true;
            }
        }
    }
}

void TypeResolver::infer_tuple_type(TupleExpr *of, TupleType *from) {
    // The two tuple types need to store the same number of convertible primitives

    if (from->is_ref) {
        return;
    }

    for (std::size_t i = 0; i < from->types.size(); i++) {
        auto &expr = std::get<ExprNode>(of->elements[i]);
        if (from->types[i]->primitive == Type::FLOAT && expr->resolved.info->primitive == Type::INT) {
            std::get<NumericConversionType>(of->elements[i]) = NumericConversionType::INT_TO_FLOAT;
        } else if (from->types[i]->primitive == Type::INT && expr->resolved.info->primitive == Type::FLOAT) {
            std::get<NumericConversionType>(of->elements[i]) = NumericConversionType::FLOAT_TO_INT;
        }
    }
}

bool TypeResolver::are_equivalent_primitives(QualifiedTypeInfo first, QualifiedTypeInfo second) {
    if (first->primitive == second->primitive) {
        if (first->primitive == Type::LIST && second->primitive == Type::LIST) {
            auto *first_contained = dynamic_cast<ListType *>(first)->contained.get();
            auto *second_contained = dynamic_cast<ListType *>(second)->contained.get();
            if (first_contained->primitive == Type::LIST && second_contained->primitive == Type::LIST) {
                return are_equivalent_primitives(first_contained, second_contained);
            } else {
                return first_contained->primitive == second_contained->primitive;
            }
        } else if (first->primitive == Type::TUPLE && second->primitive == Type::TUPLE) {
            auto *first_tuple = dynamic_cast<TupleType *>(first);
            auto *second_tuple = dynamic_cast<TupleType *>(second);
            if (first_tuple->types.size() != second_tuple->types.size()) {
                return false;
            }

            for (std::size_t i = 0; i < first_tuple->types.size(); i++) {
                if (not are_equivalent_primitives(first_tuple->types[i].get(), second_tuple->types[i].get())) {
                    return false;
                }
            }
            return true;
        } else {
            return true;
        }
    }
    return false;
}

bool TypeResolver::are_equivalent_types(QualifiedTypeInfo first, QualifiedTypeInfo second) {
    if (first->primitive == Type::LIST && second->primitive == Type::LIST) {
        return are_equivalent_types(dynamic_cast<ListType *>(first)->contained.get(),
                   dynamic_cast<ListType *>(second)->contained.get()) &&
               (first->is_const == second->is_const) && (first->is_ref == second->is_ref);
    } else if (first->primitive == Type::TUPLE && second->primitive == Type::TUPLE) {
        auto *first_tuple = dynamic_cast<TupleType *>(first);
        auto *second_tuple = dynamic_cast<TupleType *>(second);
        if (first_tuple->types.size() != second_tuple->types.size()) {
            return false;
        }

        for (std::size_t i = 0; i < first_tuple->types.size(); i++) {
            if (not are_equivalent_types(first_tuple->types[i].get(), second_tuple->types[i].get())) {
                return false;
            }
        }

        return (first_tuple->is_const == second_tuple->is_const) && (first_tuple->is_ref == second_tuple->is_ref);
    } else {
        return are_equivalent_primitives(first, second) && (first->is_const == second->is_const) &&
               (first->is_ref == second->is_ref);
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
        error({"No such variable in the current scope"}, expr.target);
        throw TypeException{"No such variable in the current scope"};
    }

    ExprVisitorType value = resolve(expr.value.get());
    if (it->info->is_const) {
        error({"Cannot assign to a const variable"}, expr.resolved.token);
    } else if (not convertible_to(it->info, value.info, value.is_lvalue, expr.target, false)) {
        error({"Cannot convert type of value to type of target"}, expr.resolved.token);
        note({"Trying to convert from '", stringify(value.info), "' to '", stringify(it->info), "'"});
    } else if (one_of(expr.resolved.token.type, TokenType::PLUS_EQUAL, TokenType::MINUS_EQUAL, TokenType::STAR_EQUAL,
                   TokenType::SLASH_EQUAL) &&
               not one_of(it->info->primitive, Type::INT, Type::FLOAT) &&
               not one_of(value.info->primitive, Type::INT, Type::FLOAT)) {
        error({"Expected integral types for compound assignment operator"}, expr.resolved.token);
        note({"Trying to assign '", stringify(value.info), "' to '", stringify(it->info), "'"});
        throw TypeException{"Expected integral types for compound assignment operator"};
    } else if (value.info->primitive == Type::FLOAT && it->info->primitive == Type::INT) {
        expr.conversion_type = NumericConversionType::FLOAT_TO_INT;
    } else if (value.info->primitive == Type::INT && it->info->primitive == Type::FLOAT) {
        expr.conversion_type = NumericConversionType::INT_TO_FLOAT;
    }

    if (not is_builtin_type(value.info->primitive)) {
        if (expr.value->resolved.is_lvalue || expr.value->resolved.info->is_ref) {
            expr.requires_copy = true;
        }
    }
    // Assignment leads to copy when the primitive is not an inbuilt one as inbuilt types are implicitly copied when the
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
            if (left_expr.info->primitive == Type::LIST) {
                auto *left_type = dynamic_cast<ListType *>(left_expr.info);
                if (expr.resolved.token.type == TokenType::LEFT_SHIFT) {
                    if (not convertible_to(left_type->contained.get(), right_expr.info, right_expr.is_lvalue,
                            right_expr.token, false)) {
                        error({"Appended value cannot be converted to type of list"}, expr.resolved.token);
                        note({"The list type is '", stringify(left_expr.info), "' and the appended type is '",
                            stringify(right_expr.info), "'"});
                        throw TypeException{"Appended value cannot be converted to type of list"};
                    }
                    return expr.resolved = {left_expr.info, expr.resolved.token};
                } else if (expr.resolved.token.type == TokenType::RIGHT_SHIFT) {
                    if (right_expr.info->primitive != Type::INT) {
                        error({"Expected integral type as amount of elements to pop from list"}, expr.resolved.token);
                        note({"Received type '", stringify(right_expr.info), "'"});
                        throw TypeException{"Expected integral type as amount of elements to pop from list"};
                    }
                    return expr.resolved = {left_expr.info, expr.resolved.token};
                }
            }
            [[fallthrough]];
        case TokenType::BIT_AND:
        case TokenType::BIT_OR:
        case TokenType::BIT_XOR:
        case TokenType::MODULO:
            if (left_expr.info->primitive != Type::INT || right_expr.info->primitive != Type::INT) {
                error({"Wrong types of arguments to ",
                          (expr.resolved.token.type == TokenType::MODULO ? "modulo" : "binary bitwise"),
                          " operator (expected integral arguments)"},
                    expr.resolved.token);
                note({"Received types '", stringify(left_expr.info), "' and '", stringify(right_expr.info), "'"});
            }
            return expr.resolved = {left_expr.info, expr.resolved.token};
        case TokenType::NOT_EQUAL:
        case TokenType::EQUAL_EQUAL:
            if ((left_expr.info->primitive == Type::LIST && right_expr.info->primitive == Type::LIST) ||
                (left_expr.info->primitive == Type::TUPLE && right_expr.info->primitive == Type::TUPLE)) {
                //                if (not convertible_to(left_expr.info, right_expr.info, false, expr.resolved.token,
                //                false) &&
                //                    not convertible_to(right_expr.info, left_expr.info, false, expr.resolved.token,
                //                    false)) {
                if (not are_equivalent_primitives(left_expr.info, right_expr.info)) {
                    error({"Cannot compare two ", (left_expr.info->primitive == Type::LIST ? "lists" : "tuples"),
                              " that have incompatible types"},
                        expr.resolved.token);
                    note({"Only types with equivalent primitives can be compared"});
                    note({"Received types '", stringify(left_expr.info), "' and '", stringify(right_expr.info), "'"});
                }
                return expr.resolved = {make_new_type<PrimitiveType>(Type::BOOL, true, false), expr.resolved.token};
            } else if (one_of(left_expr.info->primitive, Type::BOOL, Type::STRING, Type::NULL_)) {
                if (left_expr.info->primitive != right_expr.info->primitive) {
                    error({"Cannot compare equality of objects of different types"}, expr.resolved.token);
                    note(
                        {"Trying to compare '", stringify(left_expr.info), "' and '", stringify(right_expr.info), "'"});
                }
                return expr.resolved = {make_new_type<PrimitiveType>(Type::BOOL, true, false), expr.resolved.token};
            }
            [[fallthrough]];
        case TokenType::GREATER:
        case TokenType::GREATER_EQUAL:
        case TokenType::LESS:
        case TokenType::LESS_EQUAL:
            if (one_of(left_expr.info->primitive, Type::INT, Type::FLOAT) &&
                one_of(right_expr.info->primitive, Type::INT, Type::FLOAT)) {
                if (left_expr.info->primitive != right_expr.info->primitive) {
                    warning({"Comparison between objects of types int and float"}, expr.resolved.token);
                }
                return expr.resolved = {make_new_type<PrimitiveType>(Type::BOOL, true, false), expr.resolved.token};
            } else if (left_expr.info->primitive == Type::BOOL && right_expr.info->primitive == Type::BOOL) {
                return expr.resolved = {make_new_type<PrimitiveType>(Type::BOOL, true, false), expr.resolved.token};
            } else {
                error({"Cannot compare objects of incompatible types"}, expr.resolved.token);
                note({"Trying to compare '", stringify(left_expr.info), "' and '", stringify(right_expr.info), "'"});
                return expr.resolved = {make_new_type<PrimitiveType>(Type::BOOL, true, false), expr.resolved.token};
            }
        case TokenType::PLUS:
            if (left_expr.info->primitive == Type::STRING && right_expr.info->primitive == Type::STRING) {
                return expr.resolved = {make_new_type<PrimitiveType>(Type::STRING, true, false), expr.resolved.token};
            }
            [[fallthrough]];
        case TokenType::MINUS:
        case TokenType::SLASH:
        case TokenType::STAR:
            if (one_of(left_expr.info->primitive, Type::INT, Type::FLOAT) &&
                one_of(right_expr.info->primitive, Type::INT, Type::FLOAT)) {
                if (left_expr.info->primitive == Type::INT && right_expr.info->primitive == Type::INT) {
                    return expr.resolved = {make_new_type<PrimitiveType>(Type::INT, true, false), expr.resolved.token};
                }
                return expr.resolved = {make_new_type<PrimitiveType>(Type::FLOAT, true, false), expr.resolved.token};
                // Integral promotion
            } else {
                error({"Cannot use arithmetic operators on objects of incompatible types"}, expr.resolved.token);
                note({"Trying to use '", stringify(left_expr.info), "' and '", stringify(right_expr.info), "'"});
                note({"The operators '+', '-', '/' and '*' currently only work on integral types"});
                throw TypeException{"Cannot use arithmetic operators on objects of incompatible types"};
            }
        case TokenType::DOT_DOT:
        case TokenType::DOT_DOT_EQUAL:
            if (left_expr.info->primitive == Type::INT && right_expr.info->primitive == Type::INT) {
                BaseType *list = make_new_type<ListType>(
                    Type::LIST, true, false, TypeNode{allocate_node(PrimitiveType, Type::INT, true, false)}, nullptr);
                return expr.resolved = {list, expr.resolved.token};
            } else {
                error({"Ranges can only be created for integral types"}, expr.resolved.token);
                note({"Trying to use '", stringify(left_expr.info), "' and '", stringify(right_expr.info),
                    "' as range interval"});
                throw TypeException{"Ranges can only be created for integral types"};
            }
            break;

        default:
            error({"Bug in parser with illegal token type of expression's operator"}, expr.resolved.token);
            throw TypeException{"Bug in parser with illegal token type of expression's operator"};
    }
}

bool is_builtin_function(VariableExpr *expr) {
    return std::any_of(native_functions.begin(), native_functions.end(),
        [&expr](const NativeFn &native) { return native.name == expr->name.lexeme; });
}

ExprVisitorType TypeResolver::check_inbuilt(
    VariableExpr *function, const Token &oper, std::vector<std::tuple<ExprNode, NumericConversionType, bool>> &args) {
    auto it = std::find_if(native_functions.begin(), native_functions.end(),
        [&function](const NativeFn &native) { return native.name == function->name.lexeme; });

    if (args.size() != it->arity) {
        std::string num_args = args.size() < it->arity ? "less" : "more";
        error({"Cannot pass ", num_args, " than ", std::to_string(it->arity), " argument(s) to function '", it->name,
                  "'"},
            oper);
        note({"Trying to pass ", std::to_string(args.size()), " arguments"});
        throw TypeException{"Arity error"};
    }

    for (std::size_t i = 0; i < it->arity; i++) {
        ExprVisitorType arg = resolve(std::get<ExprNode>(args[i]).get());
        if (not std::any_of(it->arguments[i].begin(), it->arguments[i].end(),
                [&arg](const Type &type) { return type == arg.info->primitive; })) {
            error({"Cannot pass argument of type '", stringify(arg.info), "' as argument number ",
                      std::to_string(i + 1), " to builtin function '", it->name, "'"},
                oper);
            throw TypeException{"Builtin argument type error"};
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
            {std::move(get->object), NumericConversionType::NONE, not called->params[0].second->is_ref});
    }

    if (called->params.size() != expr.args.size()) {
        error({"Number of arguments passed to function must match the number of parameters"}, expr.resolved.token);
        note({"Trying to pass ", std::to_string(expr.args.size()), " arguments"});
        throw TypeException{"Number of arguments passed to function must match the number of parameters"};
    }

    for (std::size_t i{0}; i < expr.args.size(); i++) {
        ExprVisitorType argument = resolve(std::get<ExprNode>(expr.args[i]).get());
        if (not convertible_to(
                called->params[i].second.get(), argument.info, argument.is_lvalue, argument.token, true)) {
            error({"Type of argument is not convertible to type of parameter"}, argument.token);
            note({"Trying to convert to '", stringify(called->params[i].second.get()), "' from '",
                stringify(argument.info), "'"});
        } else if (argument.info->primitive == Type::FLOAT && called->params[i].second->primitive == Type::INT) {
            std::get<NumericConversionType>(expr.args[i]) = NumericConversionType::FLOAT_TO_INT;
        } else if (argument.info->primitive == Type::INT && called->params[i].second->primitive == Type::FLOAT) {
            std::get<NumericConversionType>(expr.args[i]) = NumericConversionType::INT_TO_FLOAT;
        }

        auto &param = called->params[i];
        if (not is_builtin_type(param.second->primitive)) {
            if (param.second->is_ref) {
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
    if (object.info->primitive == Type::CLASS) {
        ClassStmt *accessed_type = object.class_;

        for (auto &member_decl : accessed_type->members) {
            auto *member = member_decl.first.get();
            if (member->name == name) {
                if (not in_class || (in_class && current_class->name != accessed_type->name)) {
                    if (member_decl.second == VisibilityType::PROTECTED) {
                        error({"Cannot access protected member outside class"}, name);
                    } else if (member_decl.second == VisibilityType::PRIVATE) {
                        error({"Cannot access private member outside class"}, name);
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
                    error({"Cannot access protected method outside class"}, name);
                } else if (method_decl.second == VisibilityType::PRIVATE) {
                    error({"Cannot access private method outside class"}, name);
                }
                return {make_new_type<PrimitiveType>(Type::FUNCTION, true, false), method_decl.first.get(), name};
            }
        }

        error({"No such attribute exists in the class"}, name);
        throw TypeException{"No such attribute exists in the class"};
    } else {
        error({"Expected class or list type to take attribute of"}, name);
        throw TypeException{"Expected class or list type to take attribute of"};
    }
}

ExprVisitorType TypeResolver::visit(GetExpr &expr) {
    ExprVisitorType object = resolve(expr.object.get());
    if (expr.object->resolved.info->primitive == Type::TUPLE && expr.name.type == TokenType::INT_VALUE) {
        int index = std::stoi(expr.name.lexeme); // Get the 0 in x.0
        auto *tuple = dynamic_cast<TupleType *>(expr.object->resolved.info);
        if (index >= static_cast<int>(tuple->types.size())) {
            error({"Tuple index out of range"}, expr.name);
            note({"Tuple holds '", std::to_string(tuple->types.size()), "' elements, but given index is '",
                std::to_string(index), "'"});
            throw TypeException{"Tuple index out of range"};
        }

        return expr.resolved = {tuple->types[index].get(), expr.name, object.is_lvalue};
    } else if (expr.object->resolved.info->primitive == Type::CLASS && expr.name.type == TokenType::IDENTIFIER) {
        return expr.resolved = resolve_class_access(object, expr.name);
    } else if (expr.object->resolved.info->primitive == Type::TUPLE) {
        error({"Expected integer to access tuple type"}, expr.name);
        throw TypeException{"Expected integer to access tuple type"};
    } else if (expr.object->resolved.info->primitive == Type::CLASS) {
        error({"Expected name of member to access in class"}, expr.name);
        throw TypeException{"Expected name of member to access in class"};
    } else {
        error({"Expected tuple or class type to access member of"}, expr.object->resolved.token);
        note({"Received type '", stringify(expr.object->resolved.info), "'"});
        throw TypeException{"Expected tuple or class type to access member of"};
    }
}

ExprVisitorType TypeResolver::visit(GroupingExpr &expr) {
    expr.resolved = resolve(expr.expr.get());
    expr.type.reset(copy_type(expr.resolved.info));
    expr.type->is_ref = false;
    expr.resolved.info = expr.type.get();
    expr.resolved.is_lvalue = false;
    return expr.resolved;
}

ExprVisitorType TypeResolver::visit(IndexExpr &expr) {
    ExprVisitorType list = resolve(expr.object.get());
    // I think calling a string a list is fair since its technically just a list of chars

    ExprVisitorType index = resolve(expr.index.get());

    if (index.info->primitive != Type::INT) {
        error({"Expected integral type for index"}, expr.resolved.token);
        throw TypeException{"Expected integral type for index"};
    }

    if (list.info->primitive == Type::LIST) {
        auto *contained_type = dynamic_cast<ListType *>(list.info)->contained.get();
        return expr.resolved = {contained_type, expr.resolved.token,
                   expr.object->resolved.is_lvalue || expr.object->resolved.info->is_ref};
    } else if (list.info->primitive == Type::STRING) {
        return expr.resolved = {list.info, expr.resolved.token, false}; // For now, strings are immutable.
    } else {
        error({"Expected list or string type for indexing"}, expr.resolved.token);
        note({"Received type '", stringify(list.info), "'"});
        throw TypeException{"Expected list or string type for indexing"};
    }
}

ExprVisitorType TypeResolver::visit(ListExpr &expr) {
    if (expr.elements.empty()) {
        error({"Cannot have empty list expression"}, expr.bracket);
        throw TypeException{"Cannot have empty list expression"};
    } else if (expr.elements.size() > 255) {
        error({"Cannot have more than 255 elements in list expression"}, expr.bracket);
        throw TypeException{"Cannot have more than 255 elements in list expression"};
    }

    auto *list_size = allocate_node(LiteralExpr, LiteralValue{static_cast<int>(expr.elements.size())},
        TypeNode{allocate_node(PrimitiveType, Type::INT, true, false)});

    expr.type.reset(allocate_node(ListType, Type::LIST, false, false,
        TypeNode{copy_type(resolve(std::get<ExprNode>(*expr.elements.begin()).get()).info)}, ExprNode{list_size}));

    for (std::size_t i = 1; i < expr.elements.size(); i++) {
        resolve(std::get<ExprNode>(expr.elements[i]).get()); // Will store the type info in the 'resolved' class member
    }

    if (std::none_of(expr.elements.begin(), expr.elements.end(),
            [](const ListExpr::ElementType &x) { return std::get<ExprNode>(x)->resolved.info->is_ref; })) {
        // If there are no references in the list, the individual elements can safely be made non-const
        expr.type->contained->is_const = false;
    }

    for (ListExpr::ElementType &element : expr.elements) {
        if (std::get<ExprNode>(element)->resolved.info->primitive == Type::INT &&
            expr.type->contained->primitive == Type::FLOAT) {
            std::get<NumericConversionType>(element) = NumericConversionType::INT_TO_FLOAT;
        } else if (std::get<ExprNode>(element)->resolved.info->primitive == Type::FLOAT &&
                   expr.type->contained->primitive == Type::INT) {
            std::get<NumericConversionType>(element) = NumericConversionType::FLOAT_TO_INT;
        }

        // Converting to non-ref from any non-trivial type (i.e. list) regardless of ref-ness requires a copy
        if (not expr.type->contained->is_ref && not is_builtin_type(expr.type->contained->primitive) &&
            std::get<ExprNode>(element)->resolved.is_lvalue) {
            std::get<RequiresCopy>(element) = true;
        }
    }
    return expr.resolved = {expr.type.get(), expr.bracket, false};
}

ExprVisitorType TypeResolver::visit(ListAssignExpr &expr) {
    ExprVisitorType contained = resolve(&expr.list);
    ExprVisitorType value = resolve(expr.value.get());

    if (expr.list.object->resolved.info->primitive == Type::STRING) {
        error({"Strings are immutable and non-assignable"}, expr.resolved.token);
        throw TypeException{"Strings are immutable and non-assignable"};
    }

    if (not(expr.list.resolved.is_lvalue || expr.list.resolved.info->is_ref)) {
        error({"Cannot assign to non-lvalue or non-ref list"}, expr.resolved.token);
        note({"Only variables or references can be assigned to"});
        throw TypeException{"Cannot assign to non-lvalue or non-ref list"};
    }

    if (not contained.is_lvalue) {
        error({"Cannot assign to non-lvalue element"}, expr.resolved.token);
        note({"String elements are non-assignable"});
        throw TypeException{"Cannot assign to non-lvalue element"};
    }

    if (contained.info->is_const) {
        error({"Cannot assign to constant value"}, expr.resolved.token);
        note({"Trying to assign to '", stringify(contained.info), "'"});
        throw TypeException{"Cannot assign to constant value"};
    } else if (expr.list.object->resolved.info->is_const) {
        error({"Cannot assign to constant list"}, expr.resolved.token);
        note({"Trying to assign to '", stringify(contained.info), "'"});
        throw TypeException{"Cannot assign to constant list"};
    } else if (one_of(expr.resolved.token.type, TokenType::PLUS_EQUAL, TokenType::MINUS_EQUAL, TokenType::STAR_EQUAL,
                   TokenType::SLASH_EQUAL) &&
               not one_of(contained.info->primitive, Type::INT, Type::FLOAT) &&
               not one_of(value.info->primitive, Type::INT, Type::FLOAT)) {
        error({"Expected integral types for compound assignment operator"}, expr.resolved.token);
        note({"Received types '", stringify(contained.info), "' and '", stringify(value.info), "'"});
        throw TypeException{"Expected integral types for compound assignment operator"};
    } else if (not convertible_to(contained.info, value.info, value.is_lvalue, expr.resolved.token, false)) {
        error({"Cannot convert from contained type of list to type being assigned"}, expr.resolved.token);
        note({"Trying to assign to '", stringify(contained.info), "' from '", stringify(value.info), "'"});
        throw TypeException{"Cannot convert from contained type of list to type being assigned"};
    } else if (value.info->primitive == Type::FLOAT && contained.info->primitive == Type::INT) {
        expr.conversion_type = NumericConversionType::FLOAT_TO_INT;
    } else if (value.info->primitive == Type::INT && contained.info->primitive == Type::FLOAT) {
        expr.conversion_type = NumericConversionType::INT_TO_FLOAT;
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
            error({"Bug in parser with illegal type for literal value"}, expr.resolved.token);
            throw TypeException{"Bug in parser with illegal type for literal value"};
    }
}

ExprVisitorType TypeResolver::visit(LogicalExpr &expr) {
    resolve(expr.left.get());
    resolve(expr.right.get());
    return expr.resolved = {make_new_type<PrimitiveType>(Type::BOOL, true, false), expr.resolved.token};
}

ExprVisitorType TypeResolver::visit(MoveExpr &expr) {
    ExprVisitorType right = resolve(expr.expr.get());
    if (not one_of(right.info->primitive, Type::CLASS, Type::LIST, Type::TUPLE)) {
        error({"Can only move classes, lists or tuples"}, right.token);
        note({"Trying to move type '", stringify(right.info), "'"});
        throw TypeException{"Can only move classes, lists or tuples"};
    } else if (not right.is_lvalue) {
        error({"Can only move lvalues"}, right.token);
        note({"Trying to move type '", stringify(right.info), "'"});
        throw TypeException{"Can only move classes, lists or tuples"};
    } else if (right.info->is_const || right.info->is_ref) {
        error({"Cannot move a ", right.info->is_const ? "constant" : "", right.info->is_ref ? "reference to" : "",
                  " value"},
            expr.expr->resolved.token);
        note({"Trying to move type '", stringify(right.info), "'"});
        throw TypeException{"Cannot move a constant value"};
    }
    return expr.resolved = {right.info, expr.resolved.token, false};
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
            error({"No such method exists in the class"}, expr.name);
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
            error({"No such function/class exists in the module"}, expr.name);
            throw TypeException{"No such function/class exists in the module"};

        case ExprTypeInfo::ScopeType::NONE:
        default:
            error({"No such module/class exists in the current global scope"}, expr.name);
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

    error({"No such scope exists with the given name"}, expr.name);
    throw TypeException{"No such scope exists with the given name"};
}

ExprVisitorType TypeResolver::visit(SetExpr &expr) {
    ExprVisitorType object = resolve(expr.object.get());
    ExprVisitorType value_type = resolve(expr.value.get());

    if (object.info->primitive == Type::TUPLE && expr.name.type == TokenType::INT_VALUE) {
        int index = std::stoi(expr.name.lexeme); // Get the 0 in x.0
        auto *tuple = dynamic_cast<TupleType *>(expr.object->resolved.info);
        if (index >= static_cast<int>(tuple->types.size())) {
            error({"Tuple index out of range"}, expr.name);
            note({"Tuple holds '", std::to_string(tuple->types.size()), "' elements, but given index is '",
                std::to_string(index), "'"});
            if (index == static_cast<int>(tuple->types.size())) {}
            throw TypeException{"Tuple index out of range"};
        }

        auto *assigned_type = tuple->types[index].get();
        if (assigned_type->is_const) {
            error({"Cannot assign to const tuple member"}, expr.name);
            note({"Trying to assign '", stringify(expr.value->resolved.info), "' to '", stringify(assigned_type), "'"});
            throw TypeException{"Cannot assign to const tuple member"};
        } else if (expr.object->resolved.info->is_const) {
            error({"Cannot assign to const tuple"}, expr.name);
            note({"Trying to assign to '", stringify(expr.object->resolved.info), "'"});
            throw TypeException{"Cannot assign to const tuple member"};
        }

        if (assigned_type->primitive == Type::FLOAT && value_type.info->primitive == Type::INT) {
            expr.conversion_type = NumericConversionType::INT_TO_FLOAT;
        } else if (assigned_type->primitive == Type::INT && value_type.info->primitive == Type::FLOAT) {
            expr.conversion_type = NumericConversionType::FLOAT_TO_INT;
        }

        expr.requires_copy = not is_builtin_type(value_type.info->primitive);
        return expr.resolved = {assigned_type, expr.name};
    } else if (object.info->primitive == Type::CLASS && expr.name.type == TokenType::IDENTIFIER) {
        ExprVisitorType attribute_type = resolve_class_access(object, expr.name);

        if (object.info->is_const) {
            error({"Cannot assign to a const object"}, expr.name);
            note({"Trying to assign to '", stringify(object.info), "'"});
        } else if (not in_ctor && attribute_type.info->is_const) {
            error({"Cannot assign to const attribute"}, expr.name);
            note({"Trying to assign to '", stringify(attribute_type.info), "'"});
        }

        if (not convertible_to(attribute_type.info, value_type.info, value_type.is_lvalue, expr.name, false)) {
            error({"Cannot convert value of assigned expression to type of target"}, expr.name);
            note({"Trying to convert to '", stringify(attribute_type.info), "' from '", stringify(value_type.info),
                "'"});
            throw TypeException{"Cannot convert value of assigned expression to type of target"};
        } else if (value_type.info->primitive == Type::FLOAT && attribute_type.info->primitive == Type::INT) {
            expr.conversion_type = NumericConversionType::FLOAT_TO_INT;
        } else if (value_type.info->primitive == Type::INT && attribute_type.info->primitive == Type::FLOAT) {
            expr.conversion_type = NumericConversionType::INT_TO_FLOAT;
        }

        expr.requires_copy = not is_builtin_type(value_type.info->primitive); // Similar case to AssignExpr
        return expr.resolved = {attribute_type.info, expr.resolved.token};
    } else if (expr.object->resolved.info->primitive == Type::TUPLE) {
        error({"Expected integer to access tuple type"}, expr.name);
        throw TypeException{"Expected integer to access tuple type"};
    } else if (expr.object->resolved.info->primitive == Type::CLASS) {
        error({"Expected name of member to access in class"}, expr.name);
        throw TypeException{"Expected name of member to access in class"};
    } else {
        error({"Expected tuple or class type to access member of"}, expr.object->resolved.token);
        note({"Received type '", stringify(expr.object->resolved.info), "'"});
        throw TypeException{"Expected tuple or class type to access member of"};
    }
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

    if (not convertible_to(middle.info, right.info, right.is_lvalue, expr.resolved.token, false) &&
        not convertible_to(right.info, middle.info, right.is_lvalue, expr.resolved.token, false)) {
        error({"Expected equivalent expression types for branches of ternary expression"}, expr.resolved.token);
        note({"Received types '", stringify(middle.info), "' for middle expression and '", stringify(right.info),
            "' for right expression"});
    }

    return expr.resolved = {middle.info, expr.resolved.token};
}

ExprVisitorType TypeResolver::visit(ThisExpr &expr) {
    if (not in_ctor && not in_dtor) {
        error({"Cannot use 'this' keyword outside a class's constructor or destructor"}, expr.keyword);
        throw TypeException{"Cannot use 'this' keyword outside a class's constructor or destructor"};
    }
    return expr.resolved = {make_new_type<UserDefinedType>(Type::CLASS, false, false, current_class->name),
               current_class, expr.keyword};
}

ExprVisitorType TypeResolver::visit(TupleExpr &expr) {
    expr.type.reset(allocate_node(TupleType, Type::TUPLE, false, false, {}));

    for (auto &element : expr.elements) {
        resolve(std::get<ExprNode>(element).get());
        expr.type->types.emplace_back(copy_type(std::get<ExprNode>(element)->resolved.info));
    }

    return expr.resolved = {expr.type.get(), expr.brace, false};
}

ExprVisitorType TypeResolver::visit(UnaryExpr &expr) {
    ExprVisitorType right = resolve(expr.right.get());
    switch (expr.oper.type) {
        case TokenType::BIT_NOT:
            if (right.info->primitive != Type::INT) {
                error({"Wrong type of argument to bitwise unary operator (expected integral argument)"}, expr.oper);
                note({"Received operand of type '", stringify(right.info), "'"});
            }
            return expr.resolved = {make_new_type<PrimitiveType>(Type::INT, true, false), expr.resolved.token};
        case TokenType::NOT:
            if (one_of(right.info->primitive, Type::CLASS, Type::LIST, Type::NULL_)) {
                error({"Wrong type of argument to logical not operator"}, expr.oper);
                note({"Received operand of type '", stringify(right.info), "'"});
            }
            return expr.resolved = {make_new_type<PrimitiveType>(Type::BOOL, true, false), expr.resolved.token};
        case TokenType::PLUS_PLUS:
        case TokenType::MINUS_MINUS:
            if (not one_of(right.info->primitive, Type::INT, Type::FLOAT)) {
                error({"Expected integral or floating type as argument to increment operator"}, expr.oper);
                note({"Received operand of type '", stringify(right.info), "'"});
                throw TypeException{"Expected integral or floating type as argument to increment operator"};
            } else if (right.info->is_const || not(right.is_lvalue || right.info->is_ref)) {
                error({"Expected non-const l-value or reference type as argument for increment operator"}, expr.oper);
                note({"Received operand of type '", stringify(right.info), "'"});
                throw TypeException{"Expected non-const l-value or reference type as argument for increment operator"};
            };
            return expr.resolved = {right.info, expr.oper};
        case TokenType::MINUS:
        case TokenType::PLUS:
            if (not one_of(right.info->primitive, Type::INT, Type::FLOAT)) {
                error({"Expected integral or floating point argument to operator"}, expr.oper);
                note({"Received operand of type '", stringify(right.info), "'"});
                return expr.resolved = {make_new_type<PrimitiveType>(Type::INT, true, false), expr.resolved.token};
            }
            return expr.resolved = {right.info, expr.resolved.token};

        default:
            error({"Bug in parser with illegal type for unary expression"}, expr.oper);
            throw TypeException{"Bug in parser with illegal type for unary expression"};
    }
}

ExprVisitorType TypeResolver::visit(VariableExpr &expr) {
    if (is_builtin_function(&expr)) {
        error({"Cannot use in-built function as an expression"}, expr.name);
        throw TypeException{"Cannot use in-built function as an expression"};
    }

    for (auto it = values.end() - 1; not values.empty() && it >= values.begin(); it--) {
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

    error({"No such variable/function '", expr.name.lexeme, "' in the current module's scope"}, expr.name);
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
            TypeNode{allocate_node(UserDefinedType, Type::CLASS, false, false, stmt.name)}, {},
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

    if (not values.empty()) {
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
                error({"No such module/class exists in the current global scope"}, stmt.name);
                throw TypeException{"No such module/class exists in the current global scope"};
            }
        }

        values.push_back({param.first.lexeme, param.second.get(), scope_depth + 1, param_class, i++});
    }

    if (auto *body = dynamic_cast<BlockStmt *>(stmt.body.get());
        (not body->stmts.empty() && body->stmts.back()->type_tag() != NodeType::ReturnStmt) || body->stmts.empty()) {
        // TODO: also for constructors and destructors
        if (stmt.return_type->primitive == Type::NULL_) {
            body->stmts.emplace_back(allocate_node(ReturnStmt, stmt.name, nullptr, 0, nullptr));
        }
    }

    resolve(stmt.body.get());
}

StmtVisitorType TypeResolver::visit(IfStmt &stmt) {
    ExprVisitorType condition = resolve(stmt.condition.get());
    if (one_of(condition.info->primitive, Type::CLASS, Type::LIST)) {
        error({"Class or list types are not implicitly convertible to bool"}, stmt.keyword);
    }

    resolve(stmt.thenBranch.get());

    if (stmt.elseBranch != nullptr) {
        resolve(stmt.elseBranch.get());
    }
}

StmtVisitorType TypeResolver::visit(ReturnStmt &stmt) {
    if (stmt.value == nullptr) {
        if (current_function->return_type->primitive != Type::NULL_) {
            error({"Can only have empty return expressions in functions which return 'null'"}, stmt.keyword);
            note({"Return type of the function is '", stringify(current_function->return_type.get()), "'"});
            throw TypeException{"Can only have empty return expressions in functions which return 'null'"};
        }
    } else if (ExprVisitorType return_value = resolve(stmt.value.get());
               not convertible_to(current_function->return_type.get(), return_value.info, return_value.is_lvalue,
                   stmt.keyword, true)) {
        error({"Type of expression in return statement does not match return type of function"}, stmt.keyword);
        note({"Trying to convert to '", stringify(current_function->return_type.get()), "' from '",
            stringify(return_value.info), "'"});
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
        if (not convertible_to(case_expr.info, condition.info, condition.is_lvalue, case_expr.token, false)) {
            error({"Type of case expression cannot be converted to type of switch condition"}, case_expr.token);
            note({"Trying to compare '", stringify(condition.info), "' (condition) and '", stringify(case_expr.info),
                "' (case expression)"});
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
    if (not in_class && std::any_of(values.crbegin(), values.crend(), [this, &stmt](const Value &value) {
            return value.scope_depth == scope_depth && value.lexeme == stmt.name.lexeme;
        })) {
        error({"A variable with the same name has already been created in this scope"}, stmt.name);
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
                    stmt.type->is_const = false;
                    stmt.type->is_ref = false;
                }
                break;
            case TokenType::CONST: stmt.type->is_const = true; break;
            case TokenType::REF: stmt.type->is_ref = true; break;
            default: break;
        }

        // Infer some more information about the type of the list expression from the type of the variable if needed
        //  If a variable is defined as `var x: [ref int] = [a, b, c]`, then the list needs to be inferred as a list of
        //  [ref int], not as [int]
        if (stmt.initializer->type_tag() == NodeType::ListExpr && stmt.type->primitive == Type::LIST) {
            infer_list_type(
                dynamic_cast<ListExpr *>(stmt.initializer.get()), dynamic_cast<ListType *>(stmt.type.get()));
        } else if (stmt.initializer->type_tag() == NodeType::TupleExpr && stmt.type->primitive == Type::TUPLE) {
            infer_tuple_type(
                dynamic_cast<TupleExpr *>(stmt.initializer.get()), dynamic_cast<TupleType *>(stmt.type.get()));
        }

        if (not convertible_to(type, initializer.info, initializer.is_lvalue, stmt.name, true)) {
            error({"Cannot convert from initializer type to type of variable"}, stmt.name);
            note({"Trying to convert to '", stringify(type), "' from '", stringify(initializer.info), "'"});
            throw TypeException{"Cannot convert from initializer type to type of variable"};
        } else if (initializer.info->primitive == Type::FLOAT && type->primitive == Type::INT) {
            stmt.conversion_type = NumericConversionType::FLOAT_TO_INT;
        } else if (initializer.info->primitive == Type::INT && type->primitive == Type::FLOAT) {
            stmt.conversion_type = NumericConversionType::INT_TO_FLOAT;
        }

        if (not is_builtin_type(stmt.type->primitive)) {
            if (type->is_ref) {
                stmt.requires_copy = false; // A reference binding to anything does not need a copy
            } else if (initializer.is_lvalue || initializer.info->is_ref) {
                stmt.requires_copy = true; // A copy is made when initializing from an lvalue without a reference or
                                           // when converting a ref to non-ref
            }
        }

        if (not in_class || in_function) {
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
                error({"No such module/class exists in the current global scope"}, stmt.name);
                throw TypeException{"No such module/class exists in the current global scope"};
            }
        }

        if (not in_class || in_function) {
            values.push_back(
                {stmt.name.lexeme, type, scope_depth, stmt_class, (values.empty() ? 0 : values.back().stack_slot + 1)});
        }
    } else {
        error({"Expected type for variable"}, stmt.name);
        // The variable is never created if there is an error creating it thus any references to it also break
    }
}

StmtVisitorType TypeResolver::visit(WhileStmt &stmt) {
    // ScopedScopeManager manager{*this};
    ScopedBooleanManager loop_manager{in_loop};

    ExprVisitorType condition = resolve(stmt.condition.get());
    if (one_of(condition.info->primitive, Type::CLASS, Type::LIST)) {
        error({"Class or list types are not implicitly convertible to bool"}, stmt.keyword);
        note({"Received type '", stringify(condition.info), "'"});
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

BaseTypeVisitorType TypeResolver::visit(TupleType &type) {
    return &type;
}

BaseTypeVisitorType TypeResolver::visit(TypeofType &type) {
    BaseTypeVisitorType typeof_expr = copy_type(resolve(type.expr.get()).info);
    typeof_expr->is_const = typeof_expr->is_const || type.is_const;
    typeof_expr->is_ref = typeof_expr->is_ref || type.is_ref;
    type_scratch_space.emplace_back(typeof_expr); // This is to make sure the newly synthesized type is managed properly
    return typeof_expr;
}