/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "Frontend/Parser/TypeResolver.hpp"

#include "Backend/VirtualMachine/Natives.hpp"
#include "Common.hpp"
#include "ErrorLogger/ErrorLogger.hpp"
#include "Frontend/Parser/Parser.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string_view>

struct TypeException : public std::runtime_error {
    explicit TypeException(std::string_view string) : std::runtime_error{std::string{string}} {}
};

class ScopedScopeManager {
    TypeResolver &resolver;

  public:
    explicit ScopedScopeManager(TypeResolver &resolver) : resolver{resolver} { resolver.begin_scope(); }
    ~ScopedScopeManager() { resolver.end_scope(); }
};

TypeResolver::TypeResolver(CompileContext *ctx, Module *module)
    : ctx{ctx}, current_module{module}, type_scratch_space{&module->type_scratch_space} {}

////////////////////////////////////////////////////////////////////////////////

void TypeResolver::warning(const std::vector<std::string> &message, const Token &where) const noexcept {
    ctx->logger.warning(current_module, message, where);
}

void TypeResolver::error(const std::vector<std::string> &message, const Token &where) const noexcept {
    ctx->logger.error(current_module, message, where);
}

void TypeResolver::note(const std::vector<std::string> &message) const noexcept {
    ctx->logger.note(current_module, message);
}

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
            bool initially_const = from_tuple->types[i]->is_const;
            bool initially_ref = from_tuple->types[i]->is_ref;
            if (from_tuple->is_const) {
                add_top_level_const(from_tuple->types[i]);
            }
            if (from_tuple->is_ref) {
                add_top_level_ref(from_tuple->types[i]);
            }
            if (not convertible_to(
                    to_tuple->types[i].get(), from_tuple->types[i].get(), from_lvalue, where, in_initializer)) {
                return false;
            }
            if (not initially_const && from_tuple->is_const) {
                remove_top_level_const(from_tuple->types[i]);
            }
            if (not initially_ref && from_tuple->is_ref) {
                remove_top_level_ref(from_tuple->types[i]);
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

ExprNode TypeResolver::generate_scope_access(ClassStmt *stmt, Token name) {
    if (stmt->module_path != current_module->full_path) {
        ExprNode module{allocate_node(ScopeNameExpr,
            Token{TokenType::STRING_VALUE, stmt->module_path.stem(), stmt->name.line, stmt->name.start, stmt->name.end},
            stmt->module_path, stmt)};
        module->synthesized_attrs.scope_type = ExprSynthesizedAttrs::ScopeAccessType::MODULE;

        ExprNode class_{allocate_node(ScopeAccessExpr, std::move(module), stmt->name)};
        class_->synthesized_attrs.scope_type = ExprSynthesizedAttrs::ScopeAccessType::MODULE_CLASS;
        class_->synthesized_attrs.class_ = stmt;

        ExprNode result{allocate_node(ScopeAccessExpr, std::move(class_), name)};
        result->synthesized_attrs.scope_type = ExprSynthesizedAttrs::ScopeAccessType::CLASS_METHOD;
        result->synthesized_attrs.class_ = stmt;

        return result;
    } else {
        ExprNode class_{allocate_node(ScopeNameExpr, stmt->name, stmt->module_path, stmt)};
        class_->synthesized_attrs.scope_type = ExprSynthesizedAttrs::ScopeAccessType::CLASS;
        class_->synthesized_attrs.class_ = stmt;

        ExprNode result{allocate_node(ScopeAccessExpr, std::move(class_), std::move(name))};
        result->synthesized_attrs.scope_type = ExprSynthesizedAttrs::ScopeAccessType::CLASS_METHOD;
        result->synthesized_attrs.class_ = stmt;

        return result;
    }
}

ClassStmt *TypeResolver::find_class(const std::string &class_name) {
    if (auto class_ = current_module->classes.find(class_name); class_ != current_module->classes.end()) {
        return class_->second;
    }

    return nullptr;
}

FunctionStmt *TypeResolver::find_function(const std::string &function_name) {
    if (auto func = current_module->functions.find(function_name); func != current_module->functions.end()) {
        return func->second;
    }

    return nullptr;
}

ClassStmt::MemberType *TypeResolver::find_member(ClassStmt *class_, const std::string &name) {
    if (class_->member_map.find(name) != class_->member_map.end()) {
        return &class_->members[class_->member_map.at(name)];
    } else {
        return nullptr;
    }
}

ClassStmt::MethodType *TypeResolver::find_method(ClassStmt *class_, const std::string &name) {
    if (class_->method_map.find(name) != class_->method_map.end()) {
        return &class_->methods[class_->method_map.at(name)];
    } else {
        return nullptr;
    }
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
        type_scratch_space->emplace_back(allocate_node(T, type, is_const, is_ref, std::forward<Args>(args)...));
    } else {
        for (TypeNode &existing_type : *type_scratch_space) {
            if (existing_type->primitive == type && existing_type->is_const == is_const &&
                existing_type->is_ref == is_ref) {
                return existing_type.get();
            }
        }
        type_scratch_space->emplace_back(allocate_node(T, type, is_const, is_ref));
    }
    return type_scratch_space->back().get();
}

void TypeResolver::resolve_and_replace_if_typeof(TypeNode &type) {
    if (type->type_tag() == NodeType::TypeofType) {
        resolve(type.get());
        using std::swap;
        type.swap(type_scratch_space->back());
    } else {
        resolve(type.get());
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
            return std::get<ExprNode>(elem)->synthesized_attrs.is_lvalue ||
                   std::get<ExprNode>(elem)->synthesized_attrs.info->is_ref;
        })) {
        // If there is a ListExpr consisting solely of references or lvalues, it can be safely inferred as a list of
        // references if it is being stored in a name with a reference type
        of->type->contained->is_ref = true;
        if (std::any_of(of->elements.cbegin(), of->elements.cend(), [](const ListExpr::ElementType &elem) {
                return std::get<ExprNode>(elem)->synthesized_attrs.info->is_const;
            })) {
            of->type->contained->is_const = true;
            // A list of references has all its elements become const if any one of them are const
        }
    }

    of->type->contained->is_const = of->type->contained->is_const || from->contained->is_const;
    of->type->is_const = of->type->is_const || from->is_const;
    // Infer const-ness

    // Decide if copy is needed after inferring type
    for (ListExpr::ElementType &element : of->elements) {
        if (is_nontrivial_type(of->type->contained->primitive)) {
            if (not of->type->contained->is_ref && std::get<ExprNode>(element)->synthesized_attrs.is_lvalue) {
                std::get<RequiresCopy>(element) = true;
            }
        }
    }
}

void TypeResolver::infer_tuple_type(TupleExpr *of, TupleType *from) {
    // Cannot make a reference to a TupleExpr
    if (from->is_ref) {
        return;
    }

    for (std::size_t i = 0; i < from->types.size(); i++) {
        auto &expr = std::get<ExprNode>(of->elements[i]);
        if (from->types[i]->primitive == Type::FLOAT && expr->synthesized_attrs.info->primitive == Type::INT) {
            std::get<NumericConversionType>(of->elements[i]) = NumericConversionType::INT_TO_FLOAT;
        } else if (from->types[i]->primitive == Type::INT && expr->synthesized_attrs.info->primitive == Type::FLOAT) {
            std::get<NumericConversionType>(of->elements[i]) = NumericConversionType::FLOAT_TO_INT;
        }

        if (from->types[i]->is_const) {
            add_top_level_const(of->type->types[i]);
        }

        if (from->types[i]->is_ref) {
            add_top_level_ref(of->type->types[i]);
        }

        if (not from->types[i]->is_ref && expr->synthesized_attrs.is_lvalue) {
            std::get<RequiresCopy>(of->elements[i]) = true;
        }

        if (expr->type_tag() == NodeType::TupleExpr && from->types[i]->primitive == Type::TUPLE) {
            auto *inner_tuple = dynamic_cast<TupleExpr *>(expr.get());
            infer_tuple_type(inner_tuple, dynamic_cast<TupleType *>(from->types[i].get()));
            of->type->types[i].reset(copy_type(inner_tuple->type.get()));
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

#define TYPE_METHOD_ALL(name, member, value)                                                                           \
    node->member = value;                                                                                              \
    if (node->primitive == Type::LIST) {                                                                               \
        auto *list = dynamic_cast<ListType *>(node.get());                                                             \
        name(list->contained);                                                                                         \
    } else if (node->primitive == Type::TUPLE) {                                                                       \
        auto *tuple = dynamic_cast<TupleType *>(node.get());                                                           \
        for (TypeNode & type : tuple->types) {                                                                         \
            name(type);                                                                                                \
        }                                                                                                              \
    }

void TypeResolver::remove_all_const(TypeNode &node) {
    TYPE_METHOD_ALL(remove_all_const, is_const, false)
}

void TypeResolver::remove_top_level_const(TypeNode &node) {
    node->is_const = false;
}

void TypeResolver::remove_all_ref(TypeNode &node) {
    TYPE_METHOD_ALL(remove_all_ref, is_ref, false)
}

void TypeResolver::remove_top_level_ref(TypeNode &node) {
    node->is_ref = false;
}

void TypeResolver::add_all_const(TypeNode &node) {
    TYPE_METHOD_ALL(add_all_const, is_const, true)
}

void TypeResolver::add_top_level_const(TypeNode &node) {
    node->is_const = true;
}

void TypeResolver::add_all_ref(TypeNode &node) {
    TYPE_METHOD_ALL(add_all_ref, is_ref, true)
}

void TypeResolver::add_top_level_ref(TypeNode &node) {
    node->is_ref = true;
}

#undef TYPE_METHOD_ALL

ExprVisitorType TypeResolver::check_native_function(
    VariableExpr *function, const Token &oper, std::vector<CallExpr::ArgumentType> &args) {
    const NativeWrapper *native = native_wrappers.get_native(function->name.lexeme);

    if (not native->check_arity(args.size())) {
        error({"Cannot pass ", (args.size() < native->get_arity() ? "less" : "more"), " than ",
                  std::to_string(native->get_arity()), " argument(s) to native function '", native->get_name(), "'"},
            oper);
    }

    for (const auto &arg : args) {
        resolve(std::get<ExprNode>(arg).get());
    }

    if (auto result = native->check_arguments(args); not result.first) {
        error({"[", native->get_name(), "]: ", std::string{result.second}}, oper);
    }

    return ExprVisitorType{native->get_return_type().get(), function->name};
}

bool TypeResolver::match_vartuple_with_type(IdentifierTuple::TupleType &tuple, TupleType &type) {
    if (tuple.size() != type.types.size()) {
        return false;
    }

    for (std::size_t i = 0; i < tuple.size(); i++) {
        if (tuple[i].index() == IdentifierTuple::IDENT_TUPLE) {
            if (type.types[i]->primitive != Type::TUPLE ||
                not match_vartuple_with_type(
                    std::get<IdentifierTuple>(tuple[i]).tuple, dynamic_cast<TupleType &>(*type.types[i]))) {
                return false;
            }
        }
    }

    return true;
}

void TypeResolver::copy_types_into_vartuple(IdentifierTuple::TupleType &tuple, TupleType &type) {
    for (std::size_t i = 0; i < tuple.size(); i++) {
        if (tuple[i].index() == IdentifierTuple::IDENT_TUPLE) {
            copy_types_into_vartuple(
                std::get<IdentifierTuple>(tuple[i]).tuple, dynamic_cast<TupleType &>(*type.types[i]));
        } else {
            auto &decl = std::get<IdentifierTuple::DeclarationDetails>(tuple[i]);
            std::get<TypeNode>(decl) = TypeNode{copy_type(type.types[i].get())};
        }
    }
}

void TypeResolver::add_vartuple_to_stack(IdentifierTuple::TupleType &tuple, std::size_t stack_slot) {
    for (auto &elem : tuple) {
        if (elem.index() == IdentifierTuple::IDENT_TUPLE) {
            add_vartuple_to_stack(std::get<IdentifierTuple>(elem).tuple, stack_slot);
        } else {
            auto &decl = std::get<IdentifierTuple::DeclarationDetails>(elem);
            if (not in_class && std::any_of(values.crbegin(), values.crend(), [this, &decl](const Value &value) {
                    return value.scope_depth == scope_depth && value.lexeme == std::get<Token>(decl).lexeme;
                })) {
                error({"A variable with the same name has already been declared in this scope"}, std::get<Token>(decl));
            } else {
                // TODO: fix use of `nullptr` here
                values.push_back(
                    {std::get<Token>(decl).lexeme, std::get<TypeNode>(decl).get(), scope_depth, nullptr, stack_slot++});
            }
        }
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
            if (it->scope_depth == 0) {
                expr.target_type = IdentifierType::GLOBAL;
            } else {
                expr.target_type = IdentifierType::LOCAL;
            }
            break;
        }
    }

    if (it < values.begin()) {
        error({"No such variable in the current scope"}, expr.target);
        throw TypeException{"No such variable in the current scope"};
    }

    ExprVisitorType value = resolve(expr.value.get());
    if (it->info->is_const) {
        error({"Cannot assign to a const variable"}, expr.synthesized_attrs.token);
    } else if (not convertible_to(it->info, value.info, value.is_lvalue, expr.target, false)) {
        error({"Cannot convert type of value to type of target"}, expr.synthesized_attrs.token);
        note({"Trying to convert from '", stringify(value.info), "' to '", stringify(it->info), "'"});
    } else if (one_of(expr.synthesized_attrs.token.type, TokenType::PLUS_EQUAL, TokenType::MINUS_EQUAL,
                   TokenType::STAR_EQUAL, TokenType::SLASH_EQUAL) &&
               not one_of(it->info->primitive, Type::INT, Type::FLOAT) &&
               not one_of(value.info->primitive, Type::INT, Type::FLOAT)) {
        error({"Expected integral types for compound assignment operator"}, expr.synthesized_attrs.token);
        note({"Trying to assign '", stringify(value.info), "' to '", stringify(it->info), "'"});
        throw TypeException{"Expected integral types for compound assignment operator"};
    } else if (value.info->primitive == Type::FLOAT && it->info->primitive == Type::INT) {
        expr.conversion_type = NumericConversionType::FLOAT_TO_INT;
    } else if (value.info->primitive == Type::INT && it->info->primitive == Type::FLOAT) {
        expr.conversion_type = NumericConversionType::INT_TO_FLOAT;
    }

    if (is_nontrivial_type(value.info->primitive)) {
        if (expr.value->synthesized_attrs.is_lvalue || expr.value->synthesized_attrs.info->is_ref) {
            expr.requires_copy = true;
        }
    }
    // Assignment leads to copy when the primitive is not a trivial one as trivial types are implicitly copied when the
    // values are pushed onto the stack
    expr.synthesized_attrs.info = it->info;
    expr.synthesized_attrs.stack_slot = it->stack_slot;
    return expr.synthesized_attrs;
}

ExprVisitorType TypeResolver::visit(BinaryExpr &expr) {
    ExprVisitorType left_expr = resolve(expr.left.get());
    ExprVisitorType right_expr = resolve(expr.right.get());
    switch (expr.synthesized_attrs.token.type) {
        case TokenType::LEFT_SHIFT:
        case TokenType::RIGHT_SHIFT:
            if (left_expr.info->primitive == Type::LIST) {
                auto *left_type = dynamic_cast<ListType *>(left_expr.info);
                if (expr.synthesized_attrs.token.type == TokenType::LEFT_SHIFT) {
                    if (not convertible_to(left_type->contained.get(), right_expr.info, right_expr.is_lvalue,
                            right_expr.token, false)) {
                        error({"Appended value cannot be converted to type of list"}, expr.synthesized_attrs.token);
                        note({"The list type is '", stringify(left_expr.info), "' and the appended type is '",
                            stringify(right_expr.info), "'"});
                        throw TypeException{"Appended value cannot be converted to type of list"};
                    }
                    return expr.synthesized_attrs = {left_expr.info, expr.synthesized_attrs.token};
                } else if (expr.synthesized_attrs.token.type == TokenType::RIGHT_SHIFT) {
                    if (right_expr.info->primitive != Type::INT) {
                        error({"Expected integral type as amount of elements to pop from list"},
                            expr.synthesized_attrs.token);
                        note({"Received type '", stringify(right_expr.info), "'"});
                        throw TypeException{"Expected integral type as amount of elements to pop from list"};
                    }
                    return expr.synthesized_attrs = {left_expr.info, expr.synthesized_attrs.token};
                }
            }
            [[fallthrough]];
        case TokenType::BIT_AND:
        case TokenType::BIT_OR:
        case TokenType::BIT_XOR:
        case TokenType::MODULO:
            if (left_expr.info->primitive != Type::INT || right_expr.info->primitive != Type::INT) {
                error({"Wrong types of arguments to ",
                          (expr.synthesized_attrs.token.type == TokenType::MODULO ? "modulo" : "binary bitwise"),
                          " operator (expected integral arguments)"},
                    expr.synthesized_attrs.token);
                note({"Received types '", stringify(left_expr.info), "' and '", stringify(right_expr.info), "'"});
            }
            return expr.synthesized_attrs = {left_expr.info, expr.synthesized_attrs.token};
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
                        expr.synthesized_attrs.token);
                    note({"Only types with equivalent primitives can be compared"});
                    note({"Received types '", stringify(left_expr.info), "' and '", stringify(right_expr.info), "'"});
                }
                return expr.synthesized_attrs = {
                           make_new_type<PrimitiveType>(Type::BOOL, true, false), expr.synthesized_attrs.token};
            } else if (one_of(left_expr.info->primitive, Type::BOOL, Type::STRING, Type::NULL_)) {
                if (left_expr.info->primitive != right_expr.info->primitive) {
                    error({"Cannot compare equality of objects of different types"}, expr.synthesized_attrs.token);
                    note(
                        {"Trying to compare '", stringify(left_expr.info), "' and '", stringify(right_expr.info), "'"});
                }
                return expr.synthesized_attrs = {
                           make_new_type<PrimitiveType>(Type::BOOL, true, false), expr.synthesized_attrs.token};
            }
            [[fallthrough]];
        case TokenType::GREATER:
        case TokenType::GREATER_EQUAL:
        case TokenType::LESS:
        case TokenType::LESS_EQUAL:
            if (one_of(left_expr.info->primitive, Type::INT, Type::FLOAT) &&
                one_of(right_expr.info->primitive, Type::INT, Type::FLOAT)) {
                if (left_expr.info->primitive != right_expr.info->primitive) {
                    warning({"Comparison between objects of types int and float"}, expr.synthesized_attrs.token);
                }
                return expr.synthesized_attrs = {
                           make_new_type<PrimitiveType>(Type::BOOL, true, false), expr.synthesized_attrs.token};
            } else if (left_expr.info->primitive == Type::BOOL && right_expr.info->primitive == Type::BOOL) {
                return expr.synthesized_attrs = {
                           make_new_type<PrimitiveType>(Type::BOOL, true, false), expr.synthesized_attrs.token};
            } else {
                error({"Cannot compare objects of incompatible types"}, expr.synthesized_attrs.token);
                note({"Trying to compare '", stringify(left_expr.info), "' and '", stringify(right_expr.info), "'"});
                return expr.synthesized_attrs = {
                           make_new_type<PrimitiveType>(Type::BOOL, true, false), expr.synthesized_attrs.token};
            }
        case TokenType::PLUS:
            if (left_expr.info->primitive == Type::STRING && right_expr.info->primitive == Type::STRING) {
                return expr.synthesized_attrs = {
                           make_new_type<PrimitiveType>(Type::STRING, true, false), expr.synthesized_attrs.token};
            }
            [[fallthrough]];
        case TokenType::MINUS:
        case TokenType::SLASH:
        case TokenType::STAR:
            if (one_of(left_expr.info->primitive, Type::INT, Type::FLOAT) &&
                one_of(right_expr.info->primitive, Type::INT, Type::FLOAT)) {
                if (left_expr.info->primitive == Type::INT && right_expr.info->primitive == Type::INT) {
                    return expr.synthesized_attrs = {
                               make_new_type<PrimitiveType>(Type::INT, true, false), expr.synthesized_attrs.token};
                }
                return expr.synthesized_attrs = {
                           make_new_type<PrimitiveType>(Type::FLOAT, true, false), expr.synthesized_attrs.token};
                // Integral promotion
            } else {
                error(
                    {"Cannot use arithmetic operators on objects of incompatible types"}, expr.synthesized_attrs.token);
                note({"Trying to use '", stringify(left_expr.info), "' and '", stringify(right_expr.info), "'"});
                note({"The operators '+', '-', '/' and '*' currently only work on integral types"});
                throw TypeException{"Cannot use arithmetic operators on objects of incompatible types"};
            }
        case TokenType::DOT_DOT:
        case TokenType::DOT_DOT_EQUAL:
            if (left_expr.info->primitive == Type::INT && right_expr.info->primitive == Type::INT) {
                BaseType *list = make_new_type<ListType>(
                    Type::LIST, true, false, TypeNode{allocate_node(PrimitiveType, Type::INT, false, false)});
                return expr.synthesized_attrs = {list, expr.synthesized_attrs.token};
            } else {
                error({"Ranges can only be created for integral types"}, expr.synthesized_attrs.token);
                note({"Trying to use '", stringify(left_expr.info), "' and '", stringify(right_expr.info),
                    "' as range interval"});
                throw TypeException{"Ranges can only be created for integral types"};
            }
            break;

        default:
            error({"Bug in parser with illegal token type of expression's operator"}, expr.synthesized_attrs.token);
            throw TypeException{"Bug in parser with illegal token type of expression's operator"};
    }
}

ExprVisitorType TypeResolver::visit(CallExpr &expr) {
    if (expr.function->type_tag() == NodeType::VariableExpr) {
        auto *function = dynamic_cast<VariableExpr *>(expr.function.get());
        if (native_wrappers.is_native(function->name.lexeme)) {
            expr.is_native_call = true;
            return expr.synthesized_attrs = check_native_function(function, expr.synthesized_attrs.token, expr.args);
        }
    }

    ExprVisitorType function = resolve(expr.function.get());

    FunctionStmt *called = function.func;
    ClassStmt *class_ = function.class_;

    if (expr.function->type_tag() == NodeType::GetExpr) {
        auto *get = dynamic_cast<GetExpr *>(expr.function.get());

        if (get->object->synthesized_attrs.class_ != nullptr) {
            class_ = get->object->synthesized_attrs.class_;
            auto *method = find_method(class_, get->name.lexeme);

            if (method != nullptr) {
                called = method->first.get();
                ExprNode &object = get->object;

                expr.args.insert(expr.args.begin(), {std::move(object), NumericConversionType::NONE, false});
                expr.function = generate_scope_access(class_, get->name);
                expr.function->synthesized_attrs.func = called;
                expr.function->synthesized_attrs.class_ = function.class_;
            }
        }
    } else if (expr.function->type_tag() == NodeType::VariableExpr) {
        // Determine if a constructor is being called
        if (function.class_ != nullptr && called == function.class_->ctor) {
            // Constructors of type `X(...)` need to be converted into `X::X(...)`
            expr.function = generate_scope_access(function.class_, called->name);
            expr.function->synthesized_attrs.func = called;
            expr.function->synthesized_attrs.class_ = function.class_;
        }
    }

    if (function.scope_type == ExprSynthesizedAttrs::ScopeAccessType::MODULE_CLASS) {
        assert(expr.function->type_tag() == NodeType::ScopeAccessExpr);
        expr.function = generate_scope_access(class_, class_->name);
        expr.function->synthesized_attrs.func = class_->ctor;
        expr.function->synthesized_attrs.class_ = class_;
        expr.function->synthesized_attrs.scope_type = ExprSynthesizedAttrs::ScopeAccessType::MODULE_CLASS;
        called = class_->ctor;
    }

    if (called->params.size() != expr.args.size()) {
        error({"Number of arguments passed to function must match the number of parameters"},
            expr.synthesized_attrs.token);
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
        if (is_nontrivial_type(param.second->primitive)) {
            if (param.second->is_ref) {
                std::get<RequiresCopy>(expr.args[i]) = false; // A reference binding to anything does not need a copy
            } else if (argument.is_lvalue) {
                std::get<RequiresCopy>(expr.args[i]) =
                    true; // A copy is made when initializing from an lvalue without a reference
            }
        }
    }

    return expr.synthesized_attrs = {called->return_type.get(), called, class_, expr.synthesized_attrs.token};
}

ExprVisitorType TypeResolver::visit(CommaExpr &expr) {
    auto it = begin(expr.exprs);

    for (auto next = std::next(it); next != end(expr.exprs); it = next, ++next)
        resolve(it->get());

    return expr.synthesized_attrs = resolve(it->get());
}

ExprVisitorType TypeResolver::resolve_class_access(ExprVisitorType &object, const Token &name) {
    ClassStmt *accessed_type = object.class_;

    auto *member = find_member(accessed_type, name.lexeme);
    if (member != nullptr) {
        if (not in_class || (in_class && current_class->name != accessed_type->name)) {
            if (member->second == VisibilityType::PROTECTED) {
                error({"Cannot access protected member outside class"}, name);
            } else if (member->second == VisibilityType::PRIVATE) {
                error({"Cannot access private member outside class"}, name);
            }
        }

        auto *type = copy_type(member->first->type.get());
        type->is_const = type->is_const || object.info->is_const;
        ExprVisitorType info{type, name, object.is_lvalue};

        type_scratch_space->emplace_back(type); // Make sure there's no memory leaks

        if (member->first->type->type_tag() == NodeType::UserDefinedType) {
            info.class_ = dynamic_cast<UserDefinedType *>(member->first->type.get())->class_;
        }
        return info;
    }

    auto *method = find_method(accessed_type, name.lexeme);
    if (method != nullptr) {
        if ((method->second == VisibilityType::PUBLIC) || (in_class && current_class->name == accessed_type->name)) {
            return {make_new_type<PrimitiveType>(Type::FUNCTION, true, false), method->first.get(), name};
        } else if (method->second == VisibilityType::PROTECTED) {
            error({"Cannot access protected method outside class"}, name);
        } else if (method->second == VisibilityType::PRIVATE) {
            error({"Cannot access private method outside class"}, name);
        }
        return {make_new_type<PrimitiveType>(Type::FUNCTION, true, false), method->first.get(), name};
    }

    error({"No such attribute exists in the class"}, name);
    throw TypeException{"No such attribute exists in the class"};
}

ExprVisitorType TypeResolver::visit(GetExpr &expr) {
    ExprVisitorType object = resolve(expr.object.get());
    if (expr.object->synthesized_attrs.info->primitive == Type::TUPLE && expr.name.type == TokenType::INT_VALUE) {
        int index = std::stoi(expr.name.lexeme); // Get the 0 in x.0
        auto *tuple = dynamic_cast<TupleType *>(expr.object->synthesized_attrs.info);
        if (index >= static_cast<int>(tuple->types.size())) {
            error({"Tuple index out of range"}, expr.name);
            note({"Tuple holds '", std::to_string(tuple->types.size()), "' elements, but given index is '",
                std::to_string(index), "'"});
            throw TypeException{"Tuple index out of range"};
        }

        if (tuple->types[index]->primitive == Type::CLASS) {
            return expr.synthesized_attrs = {tuple->types[index].get(),
                       dynamic_cast<UserDefinedType *>(tuple->types[index].get())->class_, expr.name, object.is_lvalue};
        } else {
            return expr.synthesized_attrs = {tuple->types[index].get(), expr.name, object.is_lvalue};
        }
    } else if (expr.object->synthesized_attrs.info->primitive == Type::CLASS &&
               expr.name.type == TokenType::IDENTIFIER) {
        return expr.synthesized_attrs = resolve_class_access(object, expr.name);
    } else if (expr.object->synthesized_attrs.info->primitive == Type::TUPLE) {
        error({"Expected integer to access tuple type"}, expr.name);
        throw TypeException{"Expected integer to access tuple type"};
    } else if (expr.object->synthesized_attrs.info->primitive == Type::CLASS) {
        error({"Expected name of member to access in class"}, expr.name);
        throw TypeException{"Expected name of member to access in class"};
    } else {
        error({"Expected tuple or class type to access member of"}, expr.object->synthesized_attrs.token);
        note({"Received type '", stringify(expr.object->synthesized_attrs.info), "'"});
        throw TypeException{"Expected tuple or class type to access member of"};
    }
}

ExprVisitorType TypeResolver::visit(GroupingExpr &expr) {
    expr.synthesized_attrs = resolve(expr.expr.get());
    expr.type.reset(copy_type(expr.synthesized_attrs.info));
    expr.type->is_ref = false;
    expr.synthesized_attrs.info = expr.type.get();
    expr.synthesized_attrs.is_lvalue = false;
    return expr.synthesized_attrs;
}

ExprVisitorType TypeResolver::visit(IndexExpr &expr) {
    ExprVisitorType list = resolve(expr.object.get());
    // I think calling a string a list is fair since its technically just a list of chars

    ExprVisitorType index = resolve(expr.index.get());

    if (index.info->primitive != Type::INT) {
        error({"Expected integral type for index"}, expr.synthesized_attrs.token);
        throw TypeException{"Expected integral type for index"};
    }

    if (list.info->primitive == Type::LIST) {
        auto *contained_type = dynamic_cast<ListType *>(list.info)->contained.get();
        bool is_lvalue = expr.object->synthesized_attrs.is_lvalue || expr.object->synthesized_attrs.info->is_ref;
        if (contained_type->primitive == Type::CLASS) {
            return expr.synthesized_attrs = {contained_type, dynamic_cast<UserDefinedType *>(contained_type)->class_,
                       expr.synthesized_attrs.token, is_lvalue};
        } else {
            return expr.synthesized_attrs = {contained_type, expr.synthesized_attrs.token, is_lvalue};
        }
    } else if (list.info->primitive == Type::STRING) {
        return expr.synthesized_attrs = {
                   list.info, expr.synthesized_attrs.token, false}; // For now, strings are immutable.
    } else {
        error({"Expected list or string type for indexing"}, expr.synthesized_attrs.token);
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

    expr.type.reset(allocate_node(ListType, Type::LIST, false, false,
        TypeNode{copy_type(resolve(std::get<ExprNode>(*expr.elements.begin()).get()).info)}));

    for (std::size_t i = 1; i < expr.elements.size(); i++) {
        resolve(std::get<ExprNode>(expr.elements[i]).get()); // Will store the type info in the 'resolved' class member
    }

    if (std::none_of(expr.elements.begin(), expr.elements.end(),
            [](const ListExpr::ElementType &x) { return std::get<ExprNode>(x)->synthesized_attrs.info->is_ref; })) {
        // If there are no references in the list, the individual elements can safely be made non-const
        expr.type->contained->is_const = false;
    }

    for (ListExpr::ElementType &element : expr.elements) {
        if (std::get<ExprNode>(element)->synthesized_attrs.info->primitive == Type::INT &&
            expr.type->contained->primitive == Type::FLOAT) {
            std::get<NumericConversionType>(element) = NumericConversionType::INT_TO_FLOAT;
        } else if (std::get<ExprNode>(element)->synthesized_attrs.info->primitive == Type::FLOAT &&
                   expr.type->contained->primitive == Type::INT) {
            std::get<NumericConversionType>(element) = NumericConversionType::FLOAT_TO_INT;
        }

        // Converting to non-ref from any non-trivial type (i.e. list) regardless of ref-ness requires a copy
        if (not expr.type->contained->is_ref && is_nontrivial_type(expr.type->contained->primitive) &&
            std::get<ExprNode>(element)->synthesized_attrs.is_lvalue) {
            std::get<RequiresCopy>(element) = true;
        }
    }
    return expr.synthesized_attrs = {expr.type.get(), expr.bracket, false};
}

ExprVisitorType TypeResolver::visit(ListAssignExpr &expr) {
    ExprVisitorType contained = resolve(&expr.list);
    ExprVisitorType value = resolve(expr.value.get());

    if (expr.list.object->synthesized_attrs.info->primitive == Type::STRING) {
        error({"Strings are immutable and non-assignable"}, expr.synthesized_attrs.token);
        throw TypeException{"Strings are immutable and non-assignable"};
    }

    if (not(expr.list.synthesized_attrs.is_lvalue || expr.list.synthesized_attrs.info->is_ref)) {
        error({"Cannot assign to non-lvalue or non-ref list"}, expr.synthesized_attrs.token);
        note({"Only variables or references can be assigned to"});
        throw TypeException{"Cannot assign to non-lvalue or non-ref list"};
    }

    if (not contained.is_lvalue) {
        error({"Cannot assign to non-lvalue element"}, expr.synthesized_attrs.token);
        note({"String elements are non-assignable"});
        throw TypeException{"Cannot assign to non-lvalue element"};
    }

    if (contained.info->is_const) {
        error({"Cannot assign to constant value"}, expr.synthesized_attrs.token);
        note({"Trying to assign to '", stringify(contained.info), "'"});
        throw TypeException{"Cannot assign to constant value"};
    } else if (expr.list.object->synthesized_attrs.info->is_const) {
        error({"Cannot assign to constant list"}, expr.synthesized_attrs.token);
        note({"Trying to assign to '", stringify(contained.info), "'"});
        throw TypeException{"Cannot assign to constant list"};
    } else if (one_of(expr.synthesized_attrs.token.type, TokenType::PLUS_EQUAL, TokenType::MINUS_EQUAL,
                   TokenType::STAR_EQUAL, TokenType::SLASH_EQUAL) &&
               not one_of(contained.info->primitive, Type::INT, Type::FLOAT) &&
               not one_of(value.info->primitive, Type::INT, Type::FLOAT)) {
        error({"Expected integral types for compound assignment operator"}, expr.synthesized_attrs.token);
        note({"Received types '", stringify(contained.info), "' and '", stringify(value.info), "'"});
        throw TypeException{"Expected integral types for compound assignment operator"};
    } else if (not convertible_to(contained.info, value.info, value.is_lvalue, expr.synthesized_attrs.token, false)) {
        error({"Cannot convert from contained type of list to type being assigned"}, expr.synthesized_attrs.token);
        note({"Trying to assign to '", stringify(contained.info), "' from '", stringify(value.info), "'"});
        throw TypeException{"Cannot convert from contained type of list to type being assigned"};
    } else if (value.info->primitive == Type::FLOAT && contained.info->primitive == Type::INT) {
        expr.conversion_type = NumericConversionType::FLOAT_TO_INT;
    } else if (value.info->primitive == Type::INT && contained.info->primitive == Type::FLOAT) {
        expr.conversion_type = NumericConversionType::INT_TO_FLOAT;
    }

    return expr.synthesized_attrs = {contained.info, expr.synthesized_attrs.token, false};
}

ExprVisitorType TypeResolver::visit(LiteralExpr &expr) {
    switch (expr.value.index()) {
        case LiteralValue::tag::INT:
        case LiteralValue::tag::DOUBLE:
        case LiteralValue::tag::STRING:
        case LiteralValue::tag::BOOL:
        case LiteralValue::tag::NULL_: return expr.synthesized_attrs = {expr.type.get(), expr.synthesized_attrs.token};

        default:
            error({"Bug in parser with illegal type for literal value"}, expr.synthesized_attrs.token);
            throw TypeException{"Bug in parser with illegal type for literal value"};
    }
}

ExprVisitorType TypeResolver::visit(LogicalExpr &expr) {
    resolve(expr.left.get());
    resolve(expr.right.get());
    return expr.synthesized_attrs = {
               make_new_type<PrimitiveType>(Type::BOOL, true, false), expr.synthesized_attrs.token};
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
            expr.expr->synthesized_attrs.token);
        note({"Trying to move type '", stringify(right.info), "'"});
        throw TypeException{"Cannot move a constant value"};
    }
    return expr.synthesized_attrs = {right.info, expr.synthesized_attrs.token, false};
}

ExprVisitorType TypeResolver::visit(ScopeAccessExpr &expr) {
    ExprVisitorType left = resolve(expr.scope.get());

    switch (left.scope_type) {
        case ExprSynthesizedAttrs::ScopeAccessType::CLASS:
        case ExprSynthesizedAttrs::ScopeAccessType::MODULE_CLASS: {
            auto *method = find_method(left.class_, expr.name.lexeme);
            if (method != nullptr) {
                return expr.synthesized_attrs = {make_new_type<PrimitiveType>(Type::FUNCTION, true, false),
                           method->first.get(), left.class_, expr.synthesized_attrs.token, false,
                           ExprSynthesizedAttrs::ScopeAccessType::CLASS_METHOD};
            }

            error({"No such method exists in the class"}, expr.name);
            throw TypeException{"No such method exists in the class"};
        }

        case ExprSynthesizedAttrs::ScopeAccessType::MODULE: {
            auto &module = ctx->parsed_modules[left.module_index].first;
            if (auto class_ = module.classes.find(expr.name.lexeme); class_ != module.classes.end()) {
                return expr.synthesized_attrs = {make_new_type<PrimitiveType>(Type::CLASS, true, false),
                           class_->second->ctor, class_->second, expr.synthesized_attrs.token, false,
                           ExprSynthesizedAttrs::ScopeAccessType::MODULE_CLASS};
            }

            if (auto func = module.functions.find(expr.name.lexeme); func != module.functions.end()) {
                return expr.synthesized_attrs = {make_new_type<PrimitiveType>(Type::FUNCTION, true, false),
                           func->second, expr.synthesized_attrs.token, false,
                           ExprSynthesizedAttrs::ScopeAccessType::MODULE_FUNCTION};
            }
        }
            error({"No such function/class exists in the module"}, expr.name);
            throw TypeException{"No such function/class exists in the module"};

        case ExprSynthesizedAttrs::ScopeAccessType::NONE:
        case ExprSynthesizedAttrs::ScopeAccessType::MODULE_FUNCTION:
        default:
            error({"No such module/class exists in the current global scope"}, expr.name);
            throw TypeException{"No such module/class exists in the current global scope"};
    }
}

ExprVisitorType TypeResolver::visit(ScopeNameExpr &expr) {
    for (std::size_t i{0}; i < ctx->parsed_modules.size(); i++) {
        if (ctx->parsed_modules[i].first.name == expr.name.lexeme) {
            expr.module_path = ctx->parsed_modules[i].first.full_path;
            return expr.synthesized_attrs = {make_new_type<PrimitiveType>(Type::MODULE, true, false), i,
                       expr.synthesized_attrs.token, ExprSynthesizedAttrs::ScopeAccessType::MODULE};
        }
    }

    if (ClassStmt *class_ = find_class(expr.name.lexeme); class_ != nullptr) {
        expr.module_path = current_module->full_path;
        expr.class_ = class_;
        return expr.synthesized_attrs = {make_new_type<PrimitiveType>(Type::CLASS, true, false), class_->ctor, class_,
                   expr.synthesized_attrs.token, false, ExprSynthesizedAttrs::ScopeAccessType::CLASS};
    }

    error({"No such scope exists with the given name"}, expr.name);
    throw TypeException{"No such scope exists with the given name"};
}

ExprVisitorType TypeResolver::visit(SetExpr &expr) {
    ExprVisitorType object = resolve(expr.object.get());
    ExprVisitorType value_type = resolve(expr.value.get());

    if (object.info->primitive == Type::TUPLE && expr.name.type == TokenType::INT_VALUE) {
        int index = std::stoi(expr.name.lexeme); // Get the 0 in x.0
        auto *tuple = dynamic_cast<TupleType *>(expr.object->synthesized_attrs.info);
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
            note({"Trying to assign '", stringify(expr.value->synthesized_attrs.info), "' to '",
                stringify(assigned_type), "'"});
            throw TypeException{"Cannot assign to const tuple member"};
        } else if (expr.object->synthesized_attrs.info->is_const) {
            error({"Cannot assign to const tuple"}, expr.name);
            note({"Trying to assign to '", stringify(expr.object->synthesized_attrs.info), "'"});
            throw TypeException{"Cannot assign to const tuple member"};
        } else if (not convertible_to(assigned_type, expr.value->synthesized_attrs.info,
                       expr.value->synthesized_attrs.is_lvalue, expr.name, false)) {
            error({"Cannot convert type of value to type of target"}, expr.synthesized_attrs.token);
            note({"Trying to convert from '", stringify(expr.value->synthesized_attrs.info), "' to '",
                stringify(assigned_type), "'"});
            throw TypeException{"Cannot convert type of value to type of target"};
        }

        if (assigned_type->primitive == Type::FLOAT && value_type.info->primitive == Type::INT) {
            expr.conversion_type = NumericConversionType::INT_TO_FLOAT;
        } else if (assigned_type->primitive == Type::INT && value_type.info->primitive == Type::FLOAT) {
            expr.conversion_type = NumericConversionType::FLOAT_TO_INT;
        }

        expr.requires_copy = is_nontrivial_type(value_type.info->primitive);
        return expr.synthesized_attrs = {assigned_type, expr.name};
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

        expr.requires_copy = is_nontrivial_type(value_type.info->primitive); // Similar case to AssignExpr
        return expr.synthesized_attrs = {attribute_type.info, expr.synthesized_attrs.token};
    } else if (expr.object->synthesized_attrs.info->primitive == Type::TUPLE) {
        error({"Expected integer to access tuple type"}, expr.name);
        throw TypeException{"Expected integer to access tuple type"};
    } else if (expr.object->synthesized_attrs.info->primitive == Type::CLASS) {
        error({"Expected name of member to access in class"}, expr.name);
        throw TypeException{"Expected name of member to access in class"};
    } else {
        error({"Expected tuple or class type to access member of"}, expr.object->synthesized_attrs.token);
        note({"Received type '", stringify(expr.object->synthesized_attrs.info), "'"});
        throw TypeException{"Expected tuple or class type to access member of"};
    }
}

ExprVisitorType TypeResolver::visit(SuperExpr &expr) {
    // TODO: Implement me
    expr.synthesized_attrs.token = expr.keyword;
    throw TypeException{"Super expressions/inheritance not implemented yet"};
}

ExprVisitorType TypeResolver::visit(TernaryExpr &expr) {
    ExprVisitorType left = resolve(expr.left.get());
    ExprVisitorType middle = resolve(expr.middle.get());
    ExprVisitorType right = resolve(expr.right.get());

    if (not convertible_to(middle.info, right.info, right.is_lvalue, expr.synthesized_attrs.token, false) &&
        not convertible_to(right.info, middle.info, right.is_lvalue, expr.synthesized_attrs.token, false)) {
        error(
            {"Expected equivalent expression types for branches of ternary expression"}, expr.synthesized_attrs.token);
        note({"Received types '", stringify(middle.info), "' for middle expression and '", stringify(right.info),
            "' for right expression"});
    }

    return expr.synthesized_attrs = {middle.info, expr.synthesized_attrs.token};
}

ExprVisitorType TypeResolver::visit(ThisExpr &expr) {
    if (not in_ctor && not in_dtor) {
        error({"Cannot use 'this' keyword outside a class's constructor or destructor"}, expr.keyword);
        throw TypeException{"Cannot use 'this' keyword outside a class's constructor or destructor"};
    }
    return expr.synthesized_attrs = {
               make_new_type<UserDefinedType>(Type::CLASS, false, false, current_class->name, nullptr), current_class,
               expr.keyword};
}

ExprVisitorType TypeResolver::visit(TupleExpr &expr) {
    expr.type.reset(allocate_node(TupleType, Type::TUPLE, false, false, {}));

    for (auto &element : expr.elements) {
        resolve(std::get<ExprNode>(element).get());
        expr.type->types.emplace_back(copy_type(std::get<ExprNode>(element)->synthesized_attrs.info));
    }

    return expr.synthesized_attrs = {expr.type.get(), expr.brace, false};
}

ExprVisitorType TypeResolver::visit(UnaryExpr &expr) {
    ExprVisitorType right = resolve(expr.right.get());
    switch (expr.oper.type) {
        case TokenType::BIT_NOT:
            if (right.info->primitive != Type::INT) {
                error({"Wrong type of argument to bitwise unary operator (expected integral argument)"}, expr.oper);
                note({"Received operand of type '", stringify(right.info), "'"});
            }
            return expr.synthesized_attrs = {
                       make_new_type<PrimitiveType>(Type::INT, true, false), expr.synthesized_attrs.token};
        case TokenType::NOT:
            if (one_of(right.info->primitive, Type::CLASS, Type::LIST, Type::NULL_)) {
                error({"Wrong type of argument to logical not operator"}, expr.oper);
                note({"Received operand of type '", stringify(right.info), "'"});
            }
            return expr.synthesized_attrs = {
                       make_new_type<PrimitiveType>(Type::BOOL, true, false), expr.synthesized_attrs.token};
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
            return expr.synthesized_attrs = {right.info, expr.oper};
        case TokenType::MINUS:
        case TokenType::PLUS:
            if (not one_of(right.info->primitive, Type::INT, Type::FLOAT)) {
                error({"Expected integral or floating point argument to operator"}, expr.oper);
                note({"Received operand of type '", stringify(right.info), "'"});
                return expr.synthesized_attrs = {
                           make_new_type<PrimitiveType>(Type::INT, true, false), expr.synthesized_attrs.token};
            }
            return expr.synthesized_attrs = {right.info, expr.synthesized_attrs.token};

        default:
            error({"Bug in parser with illegal type for unary expression"}, expr.oper);
            throw TypeException{"Bug in parser with illegal type for unary expression"};
    }
}

ExprVisitorType TypeResolver::visit(VariableExpr &expr) {
    if (native_wrappers.is_native(expr.name.lexeme)) {
        error({"Cannot use native function as an expression"}, expr.name);
        throw TypeException{"Cannot use native function as an expression"};
    }

    for (auto it = values.end() - 1; not values.empty() && it >= values.begin(); it--) {
        if (it->lexeme == expr.name.lexeme) {
            if (it->scope_depth == 0) {
                expr.type = IdentifierType::GLOBAL;
            } else {
                expr.type = IdentifierType::LOCAL;
            }
            expr.synthesized_attrs = {it->info, it->class_, expr.synthesized_attrs.token, true};
            expr.synthesized_attrs.stack_slot = it->stack_slot;
            return expr.synthesized_attrs;
        }
    }

    if (FunctionStmt *func = find_function(expr.name.lexeme); func != nullptr) {
        expr.type = IdentifierType::FUNCTION;
        return expr.synthesized_attrs = {
                   make_new_type<PrimitiveType>(Type::FUNCTION, true, false), func, expr.synthesized_attrs.token};
    }

    if (ClassStmt *class_ = find_class(expr.name.lexeme); class_ != nullptr) {
        expr.type = IdentifierType::CLASS;
        return expr.synthesized_attrs = {
                   make_new_type<UserDefinedType>(Type::CLASS, true, false, class_->name, nullptr), class_->ctor,
                   class_, expr.synthesized_attrs.token};
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
    ScopedManager class_manager{in_class, true};
    ScopedManager current_class_manager{current_class, &stmt};

    // Creation of the implicit constructor
    if (stmt.ctor == nullptr) {
        stmt.ctor = allocate_node(FunctionStmt, stmt.name,
            TypeNode{allocate_node(UserDefinedType, Type::CLASS, false, false, stmt.name, nullptr)}, {},
            StmtNode{allocate_node(BlockStmt, {})}, {}, values.empty() ? 0 : values.crbegin()->scope_depth, &stmt);
        StmtNode return_stmt{allocate_node(ReturnStmt, stmt.name, nullptr, 0, stmt.ctor)};
        dynamic_cast<BlockStmt *>(stmt.ctor->body.get())->stmts.emplace_back(std::move(return_stmt));

        stmt.methods.emplace_back(std::unique_ptr<FunctionStmt>{stmt.ctor}, VisibilityType::PUBLIC);
    }

    // Creation of the implicit destructor
    if (stmt.dtor == nullptr) {
        Token name = stmt.name;
        name.lexeme = "~" + name.lexeme;
        stmt.dtor = allocate_node(FunctionStmt, std::move(name),
            TypeNode{allocate_node(PrimitiveType, Type::NULL_, false, false)}, {},
            StmtNode{allocate_node(BlockStmt, {})}, {}, values.empty() ? 0 : values.crbegin()->scope_depth, &stmt);
        StmtNode return_stmt{allocate_node(ReturnStmt, stmt.name, nullptr, 0, stmt.dtor)};
        dynamic_cast<BlockStmt *>(stmt.dtor->body.get())->stmts.emplace_back(std::move(return_stmt));

        stmt.methods.emplace_back(std::unique_ptr<FunctionStmt>{stmt.dtor}, VisibilityType::PUBLIC);
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
    ScopedManager function_manager{in_function, true};
    ScopedManager current_function_manager{current_function, &stmt};

    bool throwaway{};
    bool is_in_ctor = current_class != nullptr && stmt.name == current_class->name;
    bool is_in_dtor = current_class != nullptr && stmt.name.lexeme[0] == '~' &&
                      (stmt.name.lexeme.substr(1) == current_class->name.lexeme);

    ScopedManager special_func_manager{is_in_ctor ? in_ctor : (is_in_dtor ? in_dtor : throwaway), true};

    if (not values.empty()) {
        stmt.scope_depth = values.crbegin()->scope_depth + 1;
    } else {
        stmt.scope_depth = 1;
    }

    resolve_and_replace_if_typeof(stmt.return_type);

    stmt.class_ = current_class;

    if (in_class && current_class->ctor == &stmt) {
        if (stmt.return_type->primitive != Type::CLASS || stmt.return_type->is_const || stmt.return_type->is_ref ||
            current_class != dynamic_cast<UserDefinedType *>(stmt.return_type.get())->class_) {
            error({"A constructor needs to have a return type of the same name as the class"}, stmt.name);
            note({"The return type is '", stringify(stmt.return_type.get()), "'"});
            if (stmt.return_type->is_const) {
                note({"The return type cannot be a constant type"});
            }
            if (stmt.return_type->is_ref) {
                note({"The return type cannot be a reference type"});
            }
            throw TypeException{"A constructor needs to have a return type of the same name as the class"};
        }
    } else if (in_class && current_class->dtor == &stmt) {
        if (stmt.return_type->primitive != Type::NULL_) {
            error({"A destructor can only have null return type"}, stmt.name);
            note({"The return type is '", stringify(stmt.return_type.get()), "'"});
            throw TypeException{"A destructor can only have null return type"};
        }
    }

    std::size_t i = 0;
    for (auto &param : stmt.params) {
        ClassStmt *param_class = nullptr;
        resolve_and_replace_if_typeof(param.second);

        if (param.second->type_tag() == NodeType::UserDefinedType) {
            param_class = dynamic_cast<UserDefinedType *>(param.second.get())->class_;
            if (param_class == nullptr) {
                error({"No such module/class exists in the current global scope"}, stmt.name);
                throw TypeException{"No such module/class exists in the current global scope"};
            }
        }

        if (param.first.index() == FunctionStmt::IDENT_TUPLE) {
            auto &ident_tuple = std::get<IdentifierTuple>(param.first);

            if (param.second->primitive != Type::TUPLE) {
                error({"Expected tuple type for var-tuple declaration"}, stmt.name);
                note({"Received type '", stringify(param.second.get()), "'"});
            } else if (not match_vartuple_with_type(ident_tuple.tuple, dynamic_cast<TupleType &>(*param.second))) {
                error({"Var-tuple declaration does not match type"}, stmt.name);
                throw TypeException{"Var-tuple declaration does not match type"};
            }

            copy_types_into_vartuple(ident_tuple.tuple, dynamic_cast<TupleType &>(*param.second));
            add_vartuple_to_stack(ident_tuple.tuple, i);
            i += vartuple_size(ident_tuple.tuple);
        } else {
            values.push_back(
                {std::get<Token>(param.first).lexeme, param.second.get(), scope_depth + 1, param_class, i++});
        }
    }

    if (auto *body = dynamic_cast<BlockStmt *>(stmt.body.get());
        (not body->stmts.empty() && body->stmts.back()->type_tag() != NodeType::ReturnStmt) || body->stmts.empty()) {
        // TODO: also for constructors and destructors
        if (stmt.return_type->primitive == Type::NULL_ || is_constructor(&stmt) || is_destructor(&stmt)) {
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
    if ((in_ctor || in_dtor) && stmt.value != nullptr) {
        error({"Cannot have non-trivial return statement in ", in_ctor ? "constructor" : "destructor"}, stmt.keyword);
        note({"Returned value's type is '", stringify(stmt.value->synthesized_attrs.info), "'"});
        throw TypeException{"Cannot have non-trivial return statement in constructor/destructor"};
    } else if (not in_ctor && not in_dtor) {
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
    }

    stmt.locals_popped = std::count_if(values.crbegin(), values.crend(),
        [this](const TypeResolver::Value &x) { return x.scope_depth >= current_function->scope_depth; });
    stmt.function = current_function;
    current_function->return_stmts.push_back(&stmt);
}

StmtVisitorType TypeResolver::visit(SwitchStmt &stmt) {
    ScopedManager switch_manager{in_switch, true};

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

    ExprVisitorType initializer = resolve(stmt.initializer.get());
    QualifiedTypeInfo type = nullptr;
    bool originally_typeless = stmt.type == nullptr;
    if (stmt.type == nullptr) {
        stmt.type = TypeNode{copy_type(initializer.info)};
        type = stmt.type.get();
    } else {
        resolve_and_replace_if_typeof(stmt.type);
        type = resolve(stmt.type.get());
    }
    switch (stmt.keyword.type) {
        case TokenType::VAR:
            // If there is a var statement without a specified type that is not binding to a reference it is
            // automatically non-const
            if (originally_typeless) {
                remove_all_const(stmt.type);
                remove_all_ref(stmt.type);
            }
            break;
        case TokenType::CONST: add_all_const(stmt.type); break;
        case TokenType::REF: add_top_level_ref(stmt.type); break;
        default: break;
    }

    // Infer some more information about the type of the list expression from the type of the variable if needed
    //  If a variable is defined as `var x: [ref int] = [a, b, c]`, then the list needs to be inferred as a list of
    //  [ref int], not as [int]
    if (stmt.initializer->type_tag() == NodeType::ListExpr && stmt.type->primitive == Type::LIST) {
        infer_list_type(dynamic_cast<ListExpr *>(stmt.initializer.get()), dynamic_cast<ListType *>(stmt.type.get()));
    } else if (stmt.initializer->type_tag() == NodeType::TupleExpr && stmt.type->primitive == Type::TUPLE) {
        infer_tuple_type(dynamic_cast<TupleExpr *>(stmt.initializer.get()), dynamic_cast<TupleType *>(stmt.type.get()));
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

    if (is_nontrivial_type(stmt.type->primitive)) {
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
}

StmtVisitorType TypeResolver::visit(VarTupleStmt &stmt) {
    ExprVisitorType initializer = resolve(stmt.initializer.get());
    QualifiedTypeInfo type = nullptr;
    bool originally_typeless = stmt.type == nullptr;
    if (stmt.type == nullptr) {
        stmt.type = TypeNode{copy_type(initializer.info)};
        type = stmt.type.get();
    } else {
        resolve_and_replace_if_typeof(stmt.type);
        type = resolve(stmt.type.get());
    }

    switch (stmt.keyword.type) {
        case TokenType::VAR:
            if (originally_typeless) {
                remove_all_const(stmt.type);
                remove_all_ref(stmt.type);
            }
            break;
        case TokenType::CONST: add_all_const(stmt.type); break;
        case TokenType::REF: add_all_ref(stmt.type); break;

        default: break;
    }

    if (stmt.type->primitive != Type::TUPLE) {
        error({"Expected tuple type for var-tuple declaration"}, stmt.token);
        note({"Received type '", stringify(stmt.type.get()), "'"});
        throw TypeException{"Expected tuple type for var-tuple declaration"};
    } else if (not match_vartuple_with_type(stmt.names.tuple, dynamic_cast<TupleType &>(*stmt.type))) {
        error({"Var-tuple declaration does not match type"}, stmt.keyword);
        throw TypeException{"Var-tuple declaration does not match type"};
    }

    copy_types_into_vartuple(stmt.names.tuple, dynamic_cast<TupleType &>(*stmt.type));

    if (not convertible_to(type, initializer.info, initializer.is_lvalue, stmt.token, true)) {
        error({"Cannot convert from type of initializer to type of var-tuple"}, stmt.token);
        note({"Trying to convert to '", stringify(type), "' from '", stringify(initializer.info), "'"});
        throw TypeException{"Cannot convert from type of initializer to type of var-tuple"};
    }

    if (not in_class || in_function) {
        add_vartuple_to_stack(stmt.names.tuple, (values.empty() ? 0 : values.back().stack_slot + 1));
    }
}

StmtVisitorType TypeResolver::visit(WhileStmt &stmt) {
    ScopedManager loop_manager{in_loop, true};

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
    type.class_ = find_class(type.name.lexeme);
    return &type;
}

BaseTypeVisitorType TypeResolver::visit(ListType &type) {
    resolve_and_replace_if_typeof(type.contained);
    resolve(type.contained.get());
    return &type;
}

BaseTypeVisitorType TypeResolver::visit(TupleType &type) {
    for (TypeNode &elem : type.types) {
        resolve_and_replace_if_typeof(elem);
        resolve(elem.get());
    }
    return &type;
}

BaseTypeVisitorType TypeResolver::visit(TypeofType &type) {
    BaseTypeVisitorType typeof_expr = copy_type(resolve(type.expr.get()).info);
    typeof_expr->is_const = typeof_expr->is_const || type.is_const;
    typeof_expr->is_ref = typeof_expr->is_ref || type.is_ref;
    type_scratch_space->emplace_back(
        typeof_expr); // This is to make sure the newly synthesized type is managed properly
    return typeof_expr;
}