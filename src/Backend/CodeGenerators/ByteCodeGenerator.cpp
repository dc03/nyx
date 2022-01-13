/* Copyright (C) 2020-2022  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "Backend/CodeGenerators/ByteCodeGenerator.hpp"

#include "Backend/VirtualMachine/Value.hpp"
#include "Common.hpp"
#include "ErrorLogger/ErrorLogger.hpp"

#include <algorithm>

ByteCodeGenerator::ByteCodeGenerator() {
    for (auto &[name, wrapper] : native_wrappers.get_all_natives()) {
        natives[name] = wrapper->get_native();
    }
}

void ByteCodeGenerator::set_compile_ctx(FrontendContext *compile_ctx_) {
    compile_ctx = compile_ctx_;
}

void ByteCodeGenerator::set_runtime_ctx(BackendContext *runtime_ctx_) {
    runtime_ctx = runtime_ctx_;
}

[[nodiscard]] bool ByteCodeGenerator::contains_destructible_type(const BaseType *type) const noexcept {
    if (type->primitive == Type::LIST) {
        auto *list = dynamic_cast<const ListType *>(type);
        if (list->contained->primitive == Type::LIST || list->contained->primitive == Type::TUPLE) {
            return contains_destructible_type(list->contained.get());
        } else {
            return list->contained->primitive == Type::CLASS;
        }
    } else if (type->primitive == Type::TUPLE) {
        auto *tuple = dynamic_cast<const TupleType *>(type);

        bool has_destructible = false;
        for (auto &type_ : tuple->types) {
            if (type_->primitive == Type::CLASS) {
                has_destructible = true;
            } else if (type_->primitive == Type::LIST || type_->primitive == Type::TUPLE) {
                has_destructible = has_destructible || contains_destructible_type(type_.get());
            }
        }

        return has_destructible;
    } else {
        return false;
    }
}

[[nodiscard]] bool ByteCodeGenerator::aggregate_destructor_already_exists(const BaseType *type) const noexcept {
    assert((type->primitive == Type::LIST || type->primitive == Type::TUPLE) &&
           "Only lists or tuples allowed as aggregate types");

    // Here we do not consider const. This is because regardless of const or not, we need to destroy the list and this
    // also allows reducing the number of functions generated.
    std::string destructor_name = aggregate_destructor_prefix + stringify_short(type, false, true);
    return current_module->functions.find(destructor_name) != current_module->functions.end();
}

void ByteCodeGenerator::generate_list_destructor_loop(const ListType *list) {
    /*
     * Consider that the list has type [Foo]:
     * var x: [Foo] = ...
     * This function effectively generates this while loop:
     *
     * var i = 0
     * var j = size(x)
     * while (i < j) {
     *     x[i].~Foo()
     *     deallocate(x[i])
     *     ++i
     * }
     */
    if (list->contained->primitive == Type::LIST || list->contained->primitive == Type::TUPLE) {
        if (not aggregate_destructor_already_exists(list->contained.get())) {
            generate_aggregate_destructor(list->contained.get());
        }
    }

    std::size_t line = 1;
    current_chunk->emit_constant(Value{0}, line);

    current_chunk->emit_instruction(Instruction::PUSH_NULL, ++line);
    current_chunk->emit_instruction(Instruction::ACCESS_LOCAL_LIST, line);
    emit_operand(0);
    current_chunk->emit_string("size", ++line);
    current_chunk->emit_instruction(Instruction::CALL_NATIVE, line);
    current_chunk->emit_instruction(Instruction::POP, line);

    std::size_t jump_begin = current_chunk->emit_instruction(Instruction::JUMP_FORWARD, line);
    emit_operand(0);

    std::size_t loop_begin = current_chunk->emit_instruction(Instruction::ACCESS_LOCAL_LIST, ++line);
    emit_operand(0);
    current_chunk->emit_instruction(Instruction::ACCESS_LOCAL, line);
    emit_operand(1);
    current_chunk->emit_instruction(Instruction::MOVE_INDEX, line);

    current_chunk->emit_instruction(Instruction::ACCESS_FROM_TOP, ++line);
    emit_operand(1);
    current_chunk->emit_instruction(Instruction::PUSH_NULL, line);
    current_chunk->emit_instruction(Instruction::EQUAL, line);
    current_chunk->emit_instruction(Instruction::NOT, line);
    std::size_t jump = current_chunk->emit_instruction(Instruction::POP_JUMP_IF_FALSE, line);

    if (list->contained->primitive == Type::LIST || list->contained->primitive == Type::TUPLE) {
        emit_aggregate_destructor_call(list->contained.get());
    } else {
        assert(list->contained->primitive == Type::CLASS && "Expected class");
        auto *contained = dynamic_cast<UserDefinedType *>(list->contained.get());
        emit_destructor_call(contained->class_, ++line);
    }
    std::size_t after = current_chunk->emit_instruction(Instruction::POP_LIST, line);

    current_chunk->emit_instruction(Instruction::ACCESS_LOCAL, ++line);
    emit_operand(1);
    current_chunk->emit_constant(Value{1}, line);
    current_chunk->emit_instruction(Instruction::IADD, line);
    current_chunk->emit_instruction(Instruction::ASSIGN_LOCAL, line);
    emit_operand(1);
    current_chunk->emit_instruction(Instruction::POP, line);

    std::size_t condition = current_chunk->emit_instruction(Instruction::ACCESS_LOCAL, ++line);
    emit_operand(1);
    current_chunk->emit_instruction(Instruction::ACCESS_LOCAL, line);
    emit_operand(2);
    current_chunk->emit_instruction(Instruction::LESSER, line);
    std::size_t jump_back = current_chunk->emit_instruction(Instruction::POP_JUMP_BACK_IF_TRUE, line);
    emit_operand(0);

    current_chunk->emit_instruction(Instruction::POP, line);
    current_chunk->emit_instruction(Instruction::POP, line);
    current_chunk->emit_instruction(Instruction::RETURN, ++line);

    patch_jump(jump, after - jump - 1);
    patch_jump(jump_back, jump_back - loop_begin + 1);
    patch_jump(jump_begin, condition - jump_begin - 1);
}

void ByteCodeGenerator::generate_aggregate_destructor(const BaseType *type) {
    assert((type->primitive == Type::LIST || type->primitive == Type::TUPLE) &&
           "Only lists or tuples allowed as aggregate types");

    RuntimeFunction destructor{};
    std::string destructor_name = aggregate_destructor_prefix + stringify_short(type, false, true);
    destructor.name = destructor_name;

    Chunk *previous = std::exchange(current_chunk, &destructor.code);
    if (type->primitive == Type::LIST) {
        auto *list = dynamic_cast<const ListType *>(type);
        generate_list_destructor_loop(list);
    } else if (type->primitive == Type::TUPLE) {
        auto *tuple = dynamic_cast<const TupleType *>(type);

        std::size_t i = 1;
        for (auto &type_ : tuple->types) {
            if (is_trivial_type(type_->primitive)) {
                ++i;
                continue;
            }

            current_chunk->emit_instruction(Instruction::ACCESS_LOCAL_LIST, i);
            emit_operand(0);
            current_chunk->emit_constant(Value{static_cast<int>(i) - 1}, i);
            current_chunk->emit_instruction(Instruction::MOVE_INDEX, i);

            current_chunk->emit_instruction(Instruction::ACCESS_FROM_TOP, i);
            emit_operand(1);
            current_chunk->emit_instruction(Instruction::PUSH_NULL, i);
            current_chunk->emit_instruction(Instruction::EQUAL, i);
            current_chunk->emit_instruction(Instruction::NOT, i);
            std::size_t jump = current_chunk->emit_instruction(Instruction::POP_JUMP_IF_FALSE, i);

            if (type_->primitive == Type::CLASS) {
                emit_destructor_call(dynamic_cast<const UserDefinedType *>(type_.get())->class_, i);
            } else if ((type_->primitive == Type::TUPLE || type_->primitive == Type::LIST) &&
                       contains_destructible_type(type_.get())) {
                if (not aggregate_destructor_already_exists(type_.get())) {
                    generate_aggregate_destructor(type_.get());
                }
                emit_aggregate_destructor_call(type_.get());
            }

            std::size_t after = current_chunk->emit_instruction(Instruction::POP_LIST, i);
            patch_jump(jump, after - jump - 1);
            ++i;
        }

        current_chunk->emit_instruction(Instruction::RETURN, i);
    }
    current_chunk = previous;

    current_compiled->functions[destructor_name] = std::move(destructor);
}

void ByteCodeGenerator::emit_aggregate_destructor_call(const BaseType *type) {
    assert((type->primitive == Type::LIST || type->primitive == Type::TUPLE) &&
           "Only lists or tuples allowed as aggregate types");

    std::size_t line = current_chunk->line_numbers.back().first;
    current_chunk->emit_string(aggregate_destructor_prefix + stringify_short(type, false, true), line);
    current_chunk->emit_instruction(Instruction::LOAD_FUNCTION_SAME_MODULE, line);
    current_chunk->emit_instruction(Instruction::CALL_FUNCTION, line);
}

void ByteCodeGenerator::begin_scope() {
    current_scope_depth++;
}

void ByteCodeGenerator::remove_topmost_scope() {
    while (not scopes.empty() && scopes.back().second == current_scope_depth) {
        scopes.pop_back();
    }
    current_scope_depth--;
}

void ByteCodeGenerator::end_scope() {
    destroy_locals(current_scope_depth);
    remove_topmost_scope();
}

void ByteCodeGenerator::destroy_locals(std::size_t until_scope) {
    for (auto begin = scopes.crbegin(); begin != scopes.crend() && begin->second >= until_scope; begin++) {
        if (begin->first->primitive == Type::STRING) {
            current_chunk->emit_instruction(Instruction::POP_STRING, 0);
        } else if (is_nontrivial_type(begin->first->primitive) && not begin->first->is_ref) {
            // Emit the call to the destructor
            if (begin->first->primitive == Type::CLASS) {
                auto *class_ = dynamic_cast<const UserDefinedType *>(begin->first)->class_;
                std::size_t line = class_->dtor->name.line;
                std::size_t i = class_->members.size() - 1;

                emit_destructor_call(class_, line);
                for (auto member = class_->members.crbegin(); member != class_->members.crend(); member++) {
                    if (member->first->type->primitive == Type::CLASS) {
                        current_chunk->emit_instruction(Instruction::ACCESS_FROM_TOP, line);
                        emit_operand(1);
                        current_chunk->emit_constant(Value{static_cast<Value::IntType>(i)}, line);
                        current_chunk->emit_instruction(Instruction::INDEX_LIST, line);
                        emit_destructor_call(dynamic_cast<UserDefinedType *>(member->first->type.get())->class_, line);
                        current_chunk->emit_instruction(Instruction::POP, line);
                    }
                }
            } else if (begin->first->primitive == Type::LIST || begin->first->primitive == Type::TUPLE) {
                if (contains_destructible_type(begin->first)) {
                    if (not aggregate_destructor_already_exists(begin->first)) {
                        generate_aggregate_destructor(begin->first);
                    }
                    emit_aggregate_destructor_call(begin->first);
                }
            }
            current_chunk->emit_instruction(Instruction::POP_LIST, 0);
        } else {
            current_chunk->emit_instruction(Instruction::POP, 0);
        }
    }
}

void ByteCodeGenerator::add_to_scope(const BaseType *type) {
    scopes.emplace_back(type, current_scope_depth);
}

void ByteCodeGenerator::patch_jump(std::size_t jump_idx, std::size_t jump_amount) {
    if (jump_amount >= Chunk::const_long_max) {
        compile_ctx->logger.fatal_error({"Size of jump is greater than that allowed by the instruction set"});
        return;
    }

    current_chunk->bytes[jump_idx] |= jump_amount & 0x00ff'ffff;
}

RuntimeModule ByteCodeGenerator::compile(Module &module) {
    begin_scope();
    RuntimeModule compiled{};
    compiled.name = module.name;
    compiled.path = module.full_path;
    current_chunk = &compiled.top_level_code;
    current_module = &module;
    current_compiled = &compiled;

    current_chunk->emit_instruction(Instruction::PUSH_NULL, 0);
    for (auto &stmt : module.statements) {
        if (stmt != nullptr) {
            compile(stmt.get());
        }
    }

    assert(current_scope_depth == 1 && "Need to be at outermost scope to emit teardown code");
    current_chunk = &compiled.teardown_code;
    end_scope();
    current_chunk->emit_instruction(Instruction::POP, 0);
    return compiled;
}

void ByteCodeGenerator::emit_conversion(NumericConversionType conversion_type, std::size_t line_number) {
    switch (conversion_type) {
        case NumericConversionType::FLOAT_TO_INT:
            current_chunk->emit_instruction(Instruction::FLOAT_TO_INT, line_number);
            break;
        case NumericConversionType::INT_TO_FLOAT:
            current_chunk->emit_instruction(Instruction::INT_TO_FLOAT, line_number);
            break;
        default: break;
    }
}

void ByteCodeGenerator::emit_operand(std::size_t value) {
    current_chunk->bytes.back() |= value & 0x00ff'ffff;
}

void ByteCodeGenerator::emit_stack_slot(std::size_t value) {
    emit_operand(value + 1);
}

void ByteCodeGenerator::emit_destructor_call(ClassStmt *class_, std::size_t line) {
    current_chunk->emit_string(mangle_function(*class_->dtor), line);

    // A class can either be compiled in an imported module or within the main module
    // These checks have to be distinct because the main module is tracked separately from imported modules
    if (current_module->full_path == class_->module_path || compile_ctx->main->full_path == class_->module_path) {
        current_chunk->emit_instruction(Instruction::LOAD_FUNCTION_SAME_MODULE, line);
    } else {
        current_chunk->emit_instruction(Instruction::LOAD_FUNCTION_MODULE_INDEX, line);
        assert(runtime_ctx->get_module_path(class_->module_path) != nullptr);
        emit_operand(runtime_ctx->get_module_index_path(class_->module_path));
    }
    current_chunk->emit_instruction(Instruction::CALL_FUNCTION, line);
}

void ByteCodeGenerator::make_ref_to(ExprNode &value) {
    if (value->type_tag() == NodeType::VariableExpr) {
        if (dynamic_cast<VariableExpr *>(value.get())->type == IdentifierType::LOCAL) {
            current_chunk->emit_instruction(Instruction::MAKE_REF_TO_LOCAL, value->synthesized_attrs.token.line);
        } else {
            current_chunk->emit_instruction(Instruction::MAKE_REF_TO_GLOBAL, value->synthesized_attrs.token.line);
        }
        emit_stack_slot(value->synthesized_attrs.stack_slot);
    } else if (value->type_tag() == NodeType::IndexExpr) {
        IndexExpr *list = dynamic_cast<IndexExpr *>(value.get());
        compile(list->object.get());
        compile(list->index.get());
        current_chunk->emit_instruction(Instruction::MAKE_REF_TO_INDEX, value->synthesized_attrs.token.line);
    } else if (value->type_tag() == NodeType::GetExpr) {
        auto *get = dynamic_cast<GetExpr *>(value.get());
        compile(get->object.get());
        if (get->object->synthesized_attrs.info->primitive == Type::TUPLE) {
            Value::IntType index = std::stoi(get->name.lexeme);
            current_chunk->emit_constant(Value{index}, get->name.line);
        } else if (get->object->synthesized_attrs.info->primitive == Type::CLASS) {
            current_chunk->emit_constant(
                Value{get_member_index(get_class(get->object), get->name.lexeme)}, get->name.line);
        }
        current_chunk->emit_instruction(Instruction::MAKE_REF_TO_INDEX, value->synthesized_attrs.token.line);
    } else {
        unreachable();
    }
}

bool ByteCodeGenerator::requires_copy(ExprNode &what, TypeNode &type) {
    return not type->is_ref && (what->synthesized_attrs.is_lvalue || what->synthesized_attrs.info->is_ref);
}

void ByteCodeGenerator::add_vartuple_to_scope(IdentifierTuple::TupleType &tuple) {
    for (auto &elem : tuple) {
        if (elem.index() == IdentifierTuple::IDENT_TUPLE) {
            add_vartuple_to_scope(std::get<IdentifierTuple>(elem).tuple);
        } else {
            auto &decl = std::get<IdentifierTuple::DeclarationDetails>(elem);
            add_to_scope(std::get<TypeNode>(decl).get());
        }
    }
}

std::size_t ByteCodeGenerator::compile_vartuple(IdentifierTuple::TupleType &tuple, TupleType &type) {
    std::size_t count = 0;
    for (std::size_t i = 0; i < tuple.size(); i++) {
        current_chunk->emit_instruction(Instruction::ACCESS_FROM_TOP, current_chunk->line_numbers.back().first);
        emit_operand(count + 1);

        current_chunk->emit_constant(Value{static_cast<int>(i)}, current_chunk->line_numbers.back().first);
        if (type.types[i]->is_ref || type.is_ref) {
            current_chunk->emit_instruction(Instruction::MAKE_REF_TO_INDEX, current_chunk->line_numbers.back().first);
        } else {
            current_chunk->emit_instruction(Instruction::MOVE_INDEX, current_chunk->line_numbers.back().first);
        }

        if (tuple[i].index() == IdentifierTuple::IDENT_TUPLE) {
            count +=
                compile_vartuple(std::get<IdentifierTuple>(tuple[i]).tuple, dynamic_cast<TupleType &>(*type.types[i]));
        } else {
            count += 1;
        }
    }

    for (std::size_t i = 0; i < count; i++) {
        current_chunk->emit_instruction(Instruction::SWAP, current_chunk->line_numbers.back().first);
        emit_operand(count - i);
    }

    current_chunk->emit_instruction(Instruction::POP_LIST, current_chunk->line_numbers.back().first);
    return count;
}

bool ByteCodeGenerator::is_ctor_call(ExprNode &node) {
    if (node->synthesized_attrs.class_ != nullptr) {
        if (node->type_tag() == NodeType::ScopeAccessExpr) {
            return dynamic_cast<ScopeAccessExpr *>(node.get())->name == node->synthesized_attrs.class_->name;
        }
    }
    return false;
}

ClassStmt *ByteCodeGenerator::get_class(ExprNode &node) {
    return node->synthesized_attrs.class_;
}

bool ByteCodeGenerator::suppress_variable_tracking() {
    return std::exchange(variable_tracking_suppressed, true);
}

void ByteCodeGenerator::restore_variable_tracking(bool previous) {
    variable_tracking_suppressed = previous;
}

void ByteCodeGenerator::make_instance(ClassStmt *class_) {
    bool previous = suppress_variable_tracking();

    current_chunk->emit_instruction(Instruction::MAKE_LIST, class_->name.line);
    emit_operand(class_->members.size());

    std::size_t i = 0;
    for (ClassStmt::MemberType &member : class_->members) {
        current_chunk->emit_instruction(Instruction::ACCESS_FROM_TOP, member.first->name.line);
        emit_operand(1);
        current_chunk->emit_constant(Value{static_cast<Value::IntType>(i)}, member.first->name.line);
        compile(member.first.get());
        current_chunk->emit_instruction(Instruction::ASSIGN_LIST, member.first->name.line);
        current_chunk->emit_instruction(Instruction::POP, member.first->name.line);

        i++;
    }

    restore_variable_tracking(previous);
}

Value::IntType ByteCodeGenerator::get_member_index(ClassStmt *stmt, const std::string &name) {
    return stmt->member_map[name];
}

std::string ByteCodeGenerator::mangle_function(FunctionStmt &stmt) {
    if (stmt.class_ != nullptr) {
        return stmt.class_->name.lexeme + "@" + stmt.name.lexeme;
    } else {
        return stmt.name.lexeme;
    }
}

std::string ByteCodeGenerator::mangle_scope_access(ScopeAccessExpr &expr) {
    if (expr.scope->type_tag() == NodeType::ScopeAccessExpr) {
        return mangle_scope_access(*dynamic_cast<ScopeAccessExpr *>(expr.scope.get())) + "@" + expr.name.lexeme;
    } else if (expr.scope->type_tag() == NodeType::ScopeNameExpr) {
        return dynamic_cast<ScopeNameExpr *>(expr.scope.get())->name.lexeme + "@" + expr.name.lexeme;
    }

    unreachable();
}

std::string ByteCodeGenerator::mangle_member_access(ClassStmt *class_, std::string &name) {
    return class_->name.lexeme + "@" + name;
}

ExprVisitorType ByteCodeGenerator::compile(Expr *expr) {
    return expr->accept(*this);
}

StmtVisitorType ByteCodeGenerator::compile(Stmt *stmt) {
    stmt->accept(*this);
}

BaseTypeVisitorType ByteCodeGenerator::compile(BaseType *type) {
    return type->accept(*this);
}

ExprVisitorType ByteCodeGenerator::visit(AssignExpr &expr) {
    auto compile_right = [&expr, this] {
        compile(expr.value.get());
        if (expr.value->synthesized_attrs.info->is_ref && expr.value->synthesized_attrs.info->primitive != Type::LIST &&
            expr.value->synthesized_attrs.info->primitive != Type::TUPLE &&
            expr.value->synthesized_attrs.info->primitive != Type::CLASS) {
            current_chunk->emit_instruction(Instruction::DEREF, expr.target.line);
        } // As there is no difference between a list and a reference to a list (aside from the tag), there is no need
          // to add an explicit Instruction::DEREF
        if (expr.requires_copy) {
            current_chunk->emit_instruction(Instruction::COPY_LIST, expr.target.line);
        }

        if (expr.conversion_type != NumericConversionType::NONE) {
            emit_conversion(expr.conversion_type, expr.synthesized_attrs.token.line);
        }
    };

    switch (expr.synthesized_attrs.token.type) {
        case TokenType::EQUAL:
            compile_right();
            // Assigning to lists needs to be handled separately, as it can involve destroying the list that was
            // previously there in the variable. This code is moved off of the hot path into ASSIGN_LOCAL_LIST and
            // ASSIGN_GLOBAL_LIST
            if (is_nontrivial_type(expr.synthesized_attrs.info->primitive)) {
                current_chunk->emit_instruction(expr.target_type == IdentifierType::LOCAL
                                                    ? Instruction::ASSIGN_LOCAL_LIST
                                                    : Instruction::ASSIGN_GLOBAL_LIST,
                    expr.synthesized_attrs.token.line);
            } else {
                current_chunk->emit_instruction(
                    expr.target_type == IdentifierType::LOCAL ? Instruction::ASSIGN_LOCAL : Instruction::ASSIGN_GLOBAL,
                    expr.synthesized_attrs.token.line);
            }
            break;
        default: {
            current_chunk->emit_instruction(
                expr.target_type == IdentifierType::LOCAL ? Instruction::ACCESS_LOCAL : Instruction::ACCESS_GLOBAL,
                expr.synthesized_attrs.token.line);
            emit_stack_slot(expr.synthesized_attrs.stack_slot);
            if (expr.synthesized_attrs.info->is_ref) {
                current_chunk->emit_instruction(Instruction::DEREF, expr.synthesized_attrs.token.line);
            }
            compile_right();
            switch (expr.synthesized_attrs.token.type) {
                case TokenType::PLUS_EQUAL:
                    current_chunk->emit_instruction(
                        expr.synthesized_attrs.info->primitive == Type::FLOAT ? Instruction::FADD : Instruction::IADD,
                        expr.synthesized_attrs.token.line);
                    break;
                case TokenType::MINUS_EQUAL:
                    current_chunk->emit_instruction(
                        expr.synthesized_attrs.info->primitive == Type::FLOAT ? Instruction::FSUB : Instruction::ISUB,
                        expr.synthesized_attrs.token.line);
                    break;
                case TokenType::STAR_EQUAL:
                    current_chunk->emit_instruction(
                        expr.synthesized_attrs.info->primitive == Type::FLOAT ? Instruction::FMUL : Instruction::IMUL,
                        expr.synthesized_attrs.token.line);
                    break;
                case TokenType::SLASH_EQUAL:
                    current_chunk->emit_instruction(
                        expr.synthesized_attrs.info->primitive == Type::FLOAT ? Instruction::FDIV : Instruction::IDIV,
                        expr.synthesized_attrs.token.line);
                    break;
                default: break;
            }
            current_chunk->emit_instruction(
                expr.target_type == IdentifierType::LOCAL ? Instruction::ASSIGN_LOCAL : Instruction::ASSIGN_GLOBAL,
                expr.synthesized_attrs.token.line);
            break;
        }
    }

    emit_stack_slot(expr.synthesized_attrs.stack_slot);
    return {};
}

ExprVisitorType ByteCodeGenerator::visit(BinaryExpr &expr) {
    bool requires_floating = expr.left->synthesized_attrs.info->primitive == Type::FLOAT ||
                             expr.right->synthesized_attrs.info->primitive == Type::FLOAT;

    auto compile_left = [&expr, this] {
        compile(expr.left.get());
        if (expr.left->synthesized_attrs.info->is_ref) {
            current_chunk->emit_instruction(Instruction::DEREF, expr.synthesized_attrs.token.line);
        }

        if (expr.left->synthesized_attrs.info->primitive == Type::INT &&
            expr.right->synthesized_attrs.info->primitive == Type::FLOAT) {
            current_chunk->emit_instruction(Instruction::INT_TO_FLOAT, expr.left->synthesized_attrs.token.line);
        }
    };

    auto compile_right = [&expr, this] {
        compile(expr.right.get());
        if (expr.right->synthesized_attrs.info->is_ref) {
            current_chunk->emit_instruction(Instruction::DEREF, expr.synthesized_attrs.token.line);
        }
        if (expr.left->synthesized_attrs.info->primitive == Type::FLOAT &&
            expr.right->synthesized_attrs.info->primitive == Type::INT) {
            current_chunk->emit_instruction(Instruction::INT_TO_FLOAT, expr.left->synthesized_attrs.token.line);
        }
    };

    if (expr.synthesized_attrs.token.type != TokenType::DOT_DOT &&
        expr.synthesized_attrs.token.type != TokenType::DOT_DOT_EQUAL) {
        compile_left();
        compile_right();
    }

    switch (expr.synthesized_attrs.token.type) {
        case TokenType::LEFT_SHIFT:
            if (expr.left->synthesized_attrs.info->primitive == Type::LIST) {
                current_chunk->emit_instruction(Instruction::APPEND_LIST, expr.synthesized_attrs.token.line);
            } else {
                current_chunk->emit_instruction(Instruction::SHIFT_LEFT, expr.synthesized_attrs.token.line);
            }
            break;
        case TokenType::RIGHT_SHIFT:
            if (expr.left->synthesized_attrs.info->primitive == Type::LIST) {
                current_chunk->emit_instruction(Instruction::POP_FROM_LIST, expr.synthesized_attrs.token.line);
            } else {
                current_chunk->emit_instruction(Instruction::SHIFT_RIGHT, expr.synthesized_attrs.token.line);
            }
            break;
        case TokenType::BIT_AND:
            current_chunk->emit_instruction(Instruction::BIT_AND, expr.synthesized_attrs.token.line);
            break;
        case TokenType::BIT_OR:
            current_chunk->emit_instruction(Instruction::BIT_OR, expr.synthesized_attrs.token.line);
            break;
        case TokenType::BIT_XOR:
            current_chunk->emit_instruction(Instruction::BIT_XOR, expr.synthesized_attrs.token.line);
            break;
        case TokenType::MODULO:
            current_chunk->emit_instruction(
                requires_floating ? Instruction::FMOD : Instruction::IMOD, expr.synthesized_attrs.token.line);
            break;

        case TokenType::EQUAL_EQUAL:
            if (expr.left->synthesized_attrs.info->primitive == Type::LIST ||
                expr.left->synthesized_attrs.info->primitive == Type::TUPLE ||
                expr.left->synthesized_attrs.info->primitive == Type::STRING) {
                current_chunk->emit_instruction(Instruction::EQUAL_SL, expr.synthesized_attrs.token.line);
            } else {
                current_chunk->emit_instruction(Instruction::EQUAL, expr.synthesized_attrs.token.line);
            }
            break;
        case TokenType::GREATER:
            current_chunk->emit_instruction(Instruction::GREATER, expr.synthesized_attrs.token.line);
            break;
        case TokenType::LESS:
            current_chunk->emit_instruction(Instruction::LESSER, expr.synthesized_attrs.token.line);
            break;

        case TokenType::NOT_EQUAL:
            if (expr.left->synthesized_attrs.info->primitive == Type::LIST ||
                expr.left->synthesized_attrs.info->primitive == Type::TUPLE ||
                expr.left->synthesized_attrs.info->primitive == Type::STRING) {
                current_chunk->emit_instruction(Instruction::EQUAL_SL, expr.synthesized_attrs.token.line);
            } else {
                current_chunk->emit_instruction(Instruction::EQUAL, expr.synthesized_attrs.token.line);
            }
            current_chunk->emit_instruction(Instruction::NOT, expr.synthesized_attrs.token.line);
            break;
        case TokenType::GREATER_EQUAL:
            current_chunk->emit_instruction(Instruction::LESSER, expr.synthesized_attrs.token.line);
            current_chunk->emit_instruction(Instruction::NOT, expr.synthesized_attrs.token.line);
            break;
        case TokenType::LESS_EQUAL:
            current_chunk->emit_instruction(Instruction::GREATER, expr.synthesized_attrs.token.line);
            current_chunk->emit_instruction(Instruction::NOT, expr.synthesized_attrs.token.line);
            break;

        case TokenType::PLUS:
            switch (expr.synthesized_attrs.info->primitive) {
                case Type::INT:
                    current_chunk->emit_instruction(Instruction::IADD, expr.synthesized_attrs.token.line);
                    break;
                case Type::FLOAT:
                    current_chunk->emit_instruction(Instruction::FADD, expr.synthesized_attrs.token.line);
                    break;
                case Type::STRING:
                    current_chunk->emit_instruction(Instruction::CONCATENATE, expr.synthesized_attrs.token.line);
                    break;
                default: unreachable();
            }
            break;

        case TokenType::MINUS:
            current_chunk->emit_instruction(
                requires_floating ? Instruction::FSUB : Instruction::ISUB, expr.synthesized_attrs.token.line);
            break;
        case TokenType::SLASH:
            current_chunk->emit_instruction(
                requires_floating ? Instruction::FDIV : Instruction::IDIV, expr.synthesized_attrs.token.line);
            break;
        case TokenType::STAR:
            current_chunk->emit_instruction(
                requires_floating ? Instruction::FMUL : Instruction::IMUL, expr.synthesized_attrs.token.line);
            break;

        case TokenType::DOT_DOT:
        case TokenType::DOT_DOT_EQUAL: {
            current_chunk->emit_instruction(Instruction::MAKE_LIST, expr.synthesized_attrs.token.line);
            emit_operand(0);
            compile_left();
            compile_right();
            /* This is effectively an unrolled while loop of the kind:
             *
             * {
             *      var list = []
             *      var x = left_expr
             *      var y = right_expr
             *      while (x < y) or while (x <= y) {
             *          list.append(x)
             *          x = x + 1
             *      }
             *      list
             * }
             */
            std::size_t jump_to_cond =
                current_chunk->emit_instruction(Instruction::JUMP_FORWARD, expr.synthesized_attrs.token.line);
            emit_operand(0);

            // list.append(x)
            std::size_t jump_back =
                current_chunk->emit_instruction(Instruction::ACCESS_FROM_TOP, expr.synthesized_attrs.token.line);
            emit_operand(3);
            current_chunk->emit_instruction(Instruction::ACCESS_FROM_TOP, expr.synthesized_attrs.token.line);
            emit_operand(3);
            current_chunk->emit_instruction(Instruction::APPEND_LIST, expr.synthesized_attrs.token.line);
            current_chunk->emit_instruction(Instruction::POP, expr.synthesized_attrs.token.line);

            // x = x + 1
            current_chunk->emit_instruction(Instruction::ACCESS_FROM_TOP, expr.synthesized_attrs.token.line);
            emit_operand(2);
            current_chunk->emit_constant(Value{1}, expr.synthesized_attrs.token.line);
            current_chunk->emit_instruction(Instruction::IADD, expr.synthesized_attrs.token.line);
            current_chunk->emit_instruction(Instruction::ASSIGN_FROM_TOP, expr.synthesized_attrs.token.line);
            emit_operand(3);
            current_chunk->emit_instruction(Instruction::POP, expr.synthesized_attrs.token.line);

            // Emit x < y or !(x > y) depending on .. or ..=
            std::size_t loop_cond =
                current_chunk->emit_instruction(Instruction::ACCESS_FROM_TOP, expr.synthesized_attrs.token.line);
            emit_operand(2);
            current_chunk->emit_instruction(Instruction::ACCESS_FROM_TOP, expr.synthesized_attrs.token.line);
            emit_operand(2);
            if (expr.synthesized_attrs.token.type == TokenType::DOT_DOT) {
                current_chunk->emit_instruction(Instruction::LESSER, expr.synthesized_attrs.token.line);
            } else {
                current_chunk->emit_instruction(Instruction::GREATER, expr.synthesized_attrs.token.line);
                current_chunk->emit_instruction(Instruction::NOT, expr.synthesized_attrs.token.line);
            }

            // Jump back to the start of the loop
            std::size_t loop_end =
                current_chunk->emit_instruction(Instruction::POP_JUMP_BACK_IF_TRUE, expr.synthesized_attrs.token.line);
            emit_operand(0);
            current_chunk->emit_instruction(Instruction::POP, expr.synthesized_attrs.token.line);
            current_chunk->emit_instruction(Instruction::POP, expr.synthesized_attrs.token.line);

            patch_jump(jump_to_cond, loop_cond - jump_to_cond - 1);
            patch_jump(loop_end, loop_end - jump_back + 1);
            break;
        }

        default:
            compile_ctx->logger.error(current_module,
                {"Bug in parser with illegal token type of expression's operator"}, expr.synthesized_attrs.token);
            break;
    }

    return {};
}

ExprVisitorType ByteCodeGenerator::visit(CallExpr &expr) {
    if (is_ctor_call(expr.function)) {
        ClassStmt *class_ = get_class(expr.function);
        make_instance(class_);
    } else {
        // Emit a null value on the stack just before the parameters of the function which is being called. This will
        // serve as the stack slot in which to save the return value of the function before destroying any arguments or
        // locals.
        current_chunk->emit_instruction(Instruction::PUSH_NULL, expr.synthesized_attrs.token.line);
    }
    std::size_t i = 0;
    for (auto &arg : expr.args) {
        auto &value = std::get<ExprNode>(arg);
        if (not expr.is_native_call) {
            auto &param = expr.function->synthesized_attrs.func->params[i];
            if (param.first.index() == FunctionStmt::IDENT_TUPLE) {
                compile(value.get());
                if (requires_copy(value, param.second)) {
                    current_chunk->emit_instruction(Instruction::COPY_LIST, current_chunk->line_numbers.back().first);
                }
                compile_vartuple(
                    std::get<IdentifierTuple>(param.first).tuple, dynamic_cast<TupleType &>(*param.second));
            } else if (param.second->is_ref && not value->synthesized_attrs.info->is_ref) {
                make_ref_to(value);
            } else if (not param.second->is_ref && value->synthesized_attrs.info->is_ref) {
                compile(value.get());
                current_chunk->emit_instruction(Instruction::DEREF, value->synthesized_attrs.token.line);
            } else {
                compile(value.get());
            }
        } else {
            compile(value.get());
        }

        if (std::get<NumericConversionType>(arg) != NumericConversionType::NONE) {
            emit_conversion(std::get<NumericConversionType>(arg), value->synthesized_attrs.token.line);
        }
        if (std::get<RequiresCopy>(arg)) {
            current_chunk->emit_instruction(Instruction::COPY_LIST, value->synthesized_attrs.token.line);
        }
        i++;
    }
    if (expr.is_native_call) {
        auto *called = dynamic_cast<VariableExpr *>(expr.function.get());
        current_chunk->emit_string(called->name.lexeme, called->name.line);
        current_chunk->emit_instruction(Instruction::CALL_NATIVE, expr.synthesized_attrs.token.line);
        auto begin = expr.args.crbegin();
        for (; begin != expr.args.crend(); begin++) {
            auto &arg = std::get<ExprNode>(*begin);
            if (is_nontrivial_type(arg->synthesized_attrs.info->primitive) && not arg->synthesized_attrs.is_lvalue &&
                not arg->synthesized_attrs.info->is_ref) {
                if (contains_destructible_type(arg->synthesized_attrs.info)) {
                    if (not aggregate_destructor_already_exists(arg->synthesized_attrs.info)) {
                        generate_aggregate_destructor(arg->synthesized_attrs.info);
                    }
                    emit_aggregate_destructor_call(arg->synthesized_attrs.info);
                }
                current_chunk->emit_instruction(Instruction::POP_LIST, arg->synthesized_attrs.token.line);
            } else if (arg->synthesized_attrs.info->primitive == Type::STRING) {
                current_chunk->emit_instruction(Instruction::POP_STRING, arg->synthesized_attrs.token.line);
            } else {
                current_chunk->emit_instruction(Instruction::POP, arg->synthesized_attrs.token.line);
            }
        }
    } else {
        compile(expr.function.get());
        current_chunk->emit_instruction(Instruction::CALL_FUNCTION, expr.synthesized_attrs.token.line);
    }
    return {};
}

ExprVisitorType ByteCodeGenerator::visit(CommaExpr &expr) {
    auto it = begin(expr.exprs);

    for (auto next = std::next(it); next != end(expr.exprs); it = next, ++next) {
        compile(it->get());
        if ((*it)->synthesized_attrs.info->primitive == Type::STRING) {
            current_chunk->emit_instruction(Instruction::POP_STRING, (*it)->synthesized_attrs.token.line);
        } else if (is_nontrivial_type((*it)->synthesized_attrs.info->primitive) &&
                   not(*it)->synthesized_attrs.is_lvalue) {
            current_chunk->emit_instruction(Instruction::POP_LIST, (*it)->synthesized_attrs.token.line);
        } else {
            current_chunk->emit_instruction(Instruction::POP, (*it)->synthesized_attrs.token.line);
        }
    }

    compile(it->get());
    return {};
}

ExprVisitorType ByteCodeGenerator::visit(GetExpr &expr) {
    if (expr.object->synthesized_attrs.info->primitive == Type::TUPLE && expr.name.type == TokenType::INT_VALUE) {
        compile(expr.object.get());

        if (not expr.object->synthesized_attrs.is_lvalue) {
            current_chunk->emit_instruction(Instruction::ACCESS_FROM_TOP, expr.name.line);
            emit_operand(1);
        }

        current_chunk->emit_constant(Value{std::stoi(expr.name.lexeme)}, expr.name.line);
        current_chunk->emit_instruction(Instruction::INDEX_LIST, expr.synthesized_attrs.token.line);

        if (not expr.object->synthesized_attrs.is_lvalue) {
            current_chunk->emit_instruction(Instruction::SWAP, expr.name.line);
            emit_operand(1);
            current_chunk->emit_instruction(Instruction::POP_LIST, expr.name.line);
        }
    } else if (expr.object->synthesized_attrs.info->primitive == Type::CLASS &&
               expr.name.type == TokenType::IDENTIFIER) {
        compile(expr.object.get());

        if (not expr.object->synthesized_attrs.is_lvalue) {
            current_chunk->emit_instruction(Instruction::ACCESS_FROM_TOP, expr.name.line);
            emit_operand(1);
        }

        current_chunk->emit_constant(
            Value{get_member_index(expr.object->synthesized_attrs.class_, expr.name.lexeme)}, expr.name.line);
        current_chunk->emit_instruction(Instruction::INDEX_LIST, expr.synthesized_attrs.token.line);

        if (not expr.object->synthesized_attrs.is_lvalue) {
            current_chunk->emit_instruction(Instruction::SWAP, expr.name.line);
            emit_operand(1);
            emit_destructor_call(expr.object->synthesized_attrs.class_, expr.name.line);
            current_chunk->emit_instruction(Instruction::POP_LIST, expr.name.line);
        }
    }
    return {};
}

ExprVisitorType ByteCodeGenerator::visit(GroupingExpr &expr) {
    compile(expr.expr.get());
    if (expr.expr->synthesized_attrs.info->is_ref) {
        current_chunk->emit_instruction(Instruction::DEREF, expr.synthesized_attrs.token.line);
    }
    return {};
}

ExprVisitorType ByteCodeGenerator::visit(IndexExpr &expr) {
    compile(expr.object.get());

    if (not expr.object->synthesized_attrs.is_lvalue) {
        current_chunk->emit_instruction(Instruction::ACCESS_FROM_TOP, expr.synthesized_attrs.token.line);
        emit_operand(1);
    }

    compile(expr.index.get());
    if (expr.index->synthesized_attrs.info->is_ref) {
        current_chunk->emit_instruction(Instruction::DEREF, expr.index->synthesized_attrs.token.line);
    }
    if (expr.object->synthesized_attrs.info->primitive == Type::LIST) {
        current_chunk->emit_instruction(Instruction::CHECK_LIST_INDEX, expr.synthesized_attrs.token.line);
        current_chunk->emit_instruction(Instruction::INDEX_LIST, expr.synthesized_attrs.token.line);
    } else if (expr.object->synthesized_attrs.info->primitive == Type::STRING) {
        current_chunk->emit_instruction(Instruction::CHECK_STRING_INDEX, expr.synthesized_attrs.token.line);
        current_chunk->emit_instruction(Instruction::INDEX_STRING, expr.synthesized_attrs.token.line);
    }

    if (not expr.object->synthesized_attrs.is_lvalue) {
        current_chunk->emit_instruction(Instruction::SWAP, expr.synthesized_attrs.token.line);
        emit_operand(1);
        current_chunk->emit_instruction(Instruction::POP_LIST, expr.synthesized_attrs.token.line);
    }
    return {};
}

ExprVisitorType ByteCodeGenerator::visit(ListExpr &expr) {
    current_chunk->emit_instruction(Instruction::MAKE_LIST, expr.bracket.line);
    emit_operand(expr.elements.size());

    std::size_t i = 0;
    for (ListExpr::ElementType &element : expr.elements) {
        auto &element_expr = std::get<ExprNode>(element);
        current_chunk->emit_instruction(Instruction::ACCESS_FROM_TOP, expr.synthesized_attrs.token.line);
        emit_operand(1);
        current_chunk->emit_constant(Value{static_cast<Value::IntType>(i)}, element_expr->synthesized_attrs.token.line);

        if (not expr.type->contained->is_ref) {
            // References have to be conditionally compiled when not binding to a name
            compile(element_expr.get());
            emit_conversion(std::get<NumericConversionType>(element), element_expr->synthesized_attrs.token.line);
        }

        if (not expr.type->contained->is_ref) {
            // Type is not a reference type
            if (element_expr->synthesized_attrs.info->is_ref) {
                current_chunk->emit_instruction(Instruction::DEREF, element_expr->synthesized_attrs.token.line);
            }
        } else if (element_expr->synthesized_attrs.is_lvalue) {
            // Type is a reference type
            make_ref_to(element_expr);
        } else {
            compile(element_expr.get()); // A reference not binding to an lvalue
            emit_conversion(std::get<NumericConversionType>(element), element_expr->synthesized_attrs.token.line);
        }

        if (std::get<RequiresCopy>(element)) {
            current_chunk->emit_instruction(Instruction::COPY_LIST, element_expr->synthesized_attrs.token.line);
        }

        current_chunk->emit_instruction(Instruction::ASSIGN_LIST, element_expr->synthesized_attrs.token.line);
        current_chunk->emit_instruction(Instruction::POP, element_expr->synthesized_attrs.token.line);
        i++;
    }
    return {};
}

ExprVisitorType ByteCodeGenerator::visit(ListAssignExpr &expr) {
    compile(expr.list.object.get());
    compile(expr.list.index.get());
    if (expr.list.index->synthesized_attrs.info->is_ref) {
        current_chunk->emit_instruction(Instruction::DEREF, expr.list.index->synthesized_attrs.token.line);
    }
    current_chunk->emit_instruction(Instruction::CHECK_LIST_INDEX, expr.synthesized_attrs.token.line);

    switch (expr.synthesized_attrs.token.type) {
        case TokenType::EQUAL: {
            compile(expr.value.get());
            if (expr.value->synthesized_attrs.info->is_ref) {
                current_chunk->emit_instruction(Instruction::DEREF, expr.value->synthesized_attrs.token.line);
            }
            if (expr.requires_copy) {
                current_chunk->emit_instruction(Instruction::COPY_LIST, expr.synthesized_attrs.token.line);
            }
            current_chunk->emit_instruction(Instruction::ASSIGN_LIST, expr.synthesized_attrs.token.line);
            break;
        }

        default: {
            compile(expr.list.object.get());
            compile(expr.list.index.get());
            if (expr.list.index->synthesized_attrs.info->is_ref) {
                current_chunk->emit_instruction(Instruction::DEREF, expr.list.index->synthesized_attrs.token.line);
            }
            // There is no need for a CHECK_INDEX here because that index has already been checked before
            current_chunk->emit_instruction(Instruction::INDEX_LIST, expr.synthesized_attrs.token.line);

            compile(expr.value.get());

            if (expr.conversion_type != NumericConversionType::NONE) {
                emit_conversion(expr.conversion_type, expr.synthesized_attrs.token.line);
            }

            Type contained_type =
                dynamic_cast<ListType *>(expr.list.object->synthesized_attrs.info)->contained->primitive;
            switch (expr.synthesized_attrs.token.type) {
                case TokenType::PLUS_EQUAL:
                    current_chunk->emit_instruction(
                        contained_type == Type::FLOAT ? Instruction::FADD : Instruction::IADD,
                        expr.synthesized_attrs.token.line);
                    break;
                case TokenType::MINUS_EQUAL:
                    current_chunk->emit_instruction(
                        contained_type == Type::FLOAT ? Instruction::FSUB : Instruction::ISUB,
                        expr.synthesized_attrs.token.line);
                    break;
                case TokenType::STAR_EQUAL:
                    current_chunk->emit_instruction(
                        contained_type == Type::FLOAT ? Instruction::FMUL : Instruction::IMUL,
                        expr.synthesized_attrs.token.line);
                    break;
                case TokenType::SLASH_EQUAL:
                    current_chunk->emit_instruction(
                        contained_type == Type::FLOAT ? Instruction::FDIV : Instruction::IDIV,
                        expr.synthesized_attrs.token.line);
                    break;
                default: break;
            }

            current_chunk->emit_instruction(Instruction::ASSIGN_LIST, expr.synthesized_attrs.token.line);
            break;
        }
    }
    return {};
}

ExprVisitorType ByteCodeGenerator::visit(ListRepeatExpr &expr) {
    auto &element = std::get<ExprNode>(expr.expr);
    auto &quantity = std::get<ExprNode>(expr.quantity);

    std::size_t line = quantity->synthesized_attrs.token.line;
    std::size_t line2 = element->synthesized_attrs.token.line;

    if (is_nontrivial_type(element->synthesized_attrs.info)) {
        current_chunk->emit_instruction(Instruction::MAKE_LIST, expr.bracket.line);
        emit_operand(0);
        compile(quantity.get());
        emit_conversion(std::get<NumericConversionType>(expr.quantity), line);
        current_chunk->emit_constant(Value{0}, expr.bracket.line);

        // Generate a while-loop to fill in the list
        std::size_t jump_begin = current_chunk->emit_instruction(Instruction::JUMP_FORWARD, line);
        emit_operand(0);

        std::size_t loop_begin = current_chunk->emit_instruction(Instruction::ACCESS_FROM_TOP, line);
        emit_operand(3);

        compile(element.get());
        current_chunk->emit_instruction(Instruction::APPEND_LIST, line);
        current_chunk->emit_instruction(Instruction::POP, line);
        current_chunk->emit_instruction(Instruction::ACCESS_FROM_TOP, line);
        emit_operand(1);
        current_chunk->emit_constant(Value{1}, line);
        current_chunk->emit_instruction(Instruction::IADD, line);
        current_chunk->emit_instruction(Instruction::ASSIGN_FROM_TOP, line);
        emit_operand(2);
        current_chunk->emit_instruction(Instruction::POP, line);

        std::size_t condition = current_chunk->emit_instruction(Instruction::ACCESS_FROM_TOP, line2);
        emit_operand(1);
        current_chunk->emit_instruction(Instruction::ACCESS_FROM_TOP, line2);
        emit_operand(3);
        current_chunk->emit_instruction(Instruction::LESSER, line);
        std::size_t jump_back = current_chunk->emit_instruction(Instruction::POP_JUMP_BACK_IF_TRUE, line2);
        emit_operand(0);

        current_chunk->emit_instruction(Instruction::POP, line2);
        current_chunk->emit_instruction(Instruction::POP, line2);

        patch_jump(jump_back, jump_back - loop_begin + 1);
        patch_jump(jump_begin, condition - jump_begin - 1);
    } else {
        current_chunk->emit_instruction(Instruction::MAKE_LIST, expr.bracket.line);
        emit_operand(0);
        current_chunk->emit_instruction(Instruction::PUSH_NULL, line);
        current_chunk->emit_instruction(Instruction::PUSH_NULL, line);
        current_chunk->emit_instruction(Instruction::ACCESS_FROM_TOP, line);
        emit_operand(3);
        compile(quantity.get());
        emit_conversion(std::get<NumericConversionType>(expr.quantity), line);
        current_chunk->emit_string("%resize_list_trivial", line);
        current_chunk->emit_instruction(Instruction::CALL_NATIVE, line);
        current_chunk->emit_instruction(Instruction::POP, line);
        current_chunk->emit_instruction(Instruction::POP, line);
        current_chunk->emit_instruction(Instruction::POP, line);

        current_chunk->emit_instruction(Instruction::ACCESS_FROM_TOP, line2);
        emit_operand(2);
        compile(element.get());
        emit_conversion(std::get<NumericConversionType>(expr.expr), line2);
        current_chunk->emit_string("fill_trivial", line2);
        current_chunk->emit_instruction(Instruction::CALL_NATIVE, line2);
        current_chunk->emit_instruction(Instruction::POP, line);
        current_chunk->emit_instruction(Instruction::POP, line);
        current_chunk->emit_instruction(Instruction::POP, line);
    }

    return {};
}

ExprVisitorType ByteCodeGenerator::visit(LiteralExpr &expr) {
    switch (expr.value.index()) {
        case LiteralValue::tag::INT:
            current_chunk->emit_constant(Value{expr.value.to_int()}, expr.synthesized_attrs.token.line);
            break;
        case LiteralValue::tag::DOUBLE:
            current_chunk->emit_constant(Value{expr.value.to_double()}, expr.synthesized_attrs.token.line);
            break;
        case LiteralValue::tag::STRING:
            current_chunk->emit_string(expr.value.to_string(), expr.synthesized_attrs.token.line);
            break;
        case LiteralValue::tag::BOOL:
            if (expr.value.to_bool()) {
                current_chunk->emit_instruction(Instruction::PUSH_TRUE, expr.synthesized_attrs.token.line);
            } else {
                current_chunk->emit_instruction(Instruction::PUSH_FALSE, expr.synthesized_attrs.token.line);
            }
            break;
        case LiteralValue::tag::NULL_:
            current_chunk->emit_instruction(Instruction::PUSH_NULL, expr.synthesized_attrs.token.line);
            break;
    }
    return {};
}

ExprVisitorType ByteCodeGenerator::visit(LogicalExpr &expr) {
    compile(expr.left.get());
    if (expr.left->synthesized_attrs.info->is_ref) {
        current_chunk->emit_instruction(Instruction::DEREF, expr.left->synthesized_attrs.token.line);
    }
    std::size_t jump_idx{};
    if (expr.synthesized_attrs.token.type == TokenType::OR) {
        jump_idx = current_chunk->emit_instruction(Instruction::JUMP_IF_TRUE, expr.synthesized_attrs.token.line);
    } else { // Since || / or short circuits on true, flip the boolean on top of the stack
        jump_idx = current_chunk->emit_instruction(Instruction::JUMP_IF_FALSE, expr.synthesized_attrs.token.line);
    }
    emit_operand(0);
    current_chunk->emit_instruction(Instruction::POP, expr.synthesized_attrs.token.line);
    compile(expr.right.get());
    std::size_t to_idx = current_chunk->bytes.size();
    patch_jump(jump_idx, to_idx - jump_idx - 1);
    return {};
}

ExprVisitorType ByteCodeGenerator::visit(MoveExpr &expr) {
    if (expr.expr->type_tag() == NodeType::VariableExpr) {
        if (dynamic_cast<VariableExpr *>(expr.expr.get())->type == IdentifierType::LOCAL) {
            current_chunk->emit_instruction(Instruction::MOVE_LOCAL, expr.synthesized_attrs.token.line);
        } else {
            current_chunk->emit_instruction(Instruction::MOVE_GLOBAL, expr.synthesized_attrs.token.line);
        }
        emit_stack_slot(expr.expr->synthesized_attrs.stack_slot);
    }
    return {};
}

ExprVisitorType ByteCodeGenerator::visit(ScopeAccessExpr &expr) {
    if (expr.scope->synthesized_attrs.scope_type == ExprSynthesizedAttrs::ScopeAccessType::MODULE_CLASS) {
        assert(expr.scope->type_tag() == NodeType::ScopeAccessExpr);
        auto *access = dynamic_cast<ScopeAccessExpr *>(expr.scope.get());
        assert(access->scope->type_tag() == NodeType::ScopeNameExpr && "Only X::Y::Z allowed for now");
        auto *module = dynamic_cast<ScopeNameExpr *>(access->scope.get());
        ClassStmt *class_ = access->synthesized_attrs.class_;

        current_chunk->emit_string(mangle_member_access(class_, expr.name.lexeme), expr.synthesized_attrs.token.line);
        current_chunk->emit_instruction(
            Instruction::LOAD_FUNCTION_MODULE_INDEX, expr.scope->synthesized_attrs.token.line);

        assert(runtime_ctx->get_module_path(module->module_path) != nullptr);

        emit_operand(runtime_ctx->get_module_index_path(module->module_path));
    } else if (expr.scope->synthesized_attrs.scope_type == ExprSynthesizedAttrs::ScopeAccessType::MODULE) {
        current_chunk->emit_string(expr.name.lexeme, expr.name.line);
        current_chunk->emit_instruction(
            Instruction::LOAD_FUNCTION_MODULE_INDEX, expr.scope->synthesized_attrs.token.line);

        assert(expr.scope->type_tag() == NodeType::ScopeNameExpr);
        auto *module = dynamic_cast<ScopeNameExpr *>(expr.scope.get());

        assert(runtime_ctx->get_module_path(module->module_path) != nullptr);

        emit_operand(runtime_ctx->get_module_index_path(module->module_path));
    } else if (expr.scope->synthesized_attrs.scope_type == ExprSynthesizedAttrs::ScopeAccessType::CLASS) {
        assert(expr.scope->type_tag() == NodeType::ScopeNameExpr);
        current_chunk->emit_string(mangle_scope_access(expr), expr.synthesized_attrs.token.line);
        if (expr.synthesized_attrs.class_->module_path == current_module->full_path) {
            current_chunk->emit_instruction(Instruction::LOAD_FUNCTION_SAME_MODULE, expr.synthesized_attrs.token.line);
        } else {
            current_chunk->emit_instruction(Instruction::LOAD_FUNCTION_MODULE_INDEX, expr.synthesized_attrs.token.line);
            emit_operand(runtime_ctx->get_module_index_path(expr.synthesized_attrs.class_->module_path));
        }
    } else {
        unreachable();
    }
    return {};
}

ExprVisitorType ByteCodeGenerator::visit(ScopeNameExpr &expr) {
    return {};
}

ExprVisitorType ByteCodeGenerator::visit(SetExpr &expr) {
    if (expr.object->synthesized_attrs.info->primitive == Type::TUPLE && expr.name.type == TokenType::INT_VALUE) {
        compile(expr.object.get());
        current_chunk->emit_constant(Value{std::stoi(expr.name.lexeme)}, expr.name.line);
        compile(expr.value.get());
        current_chunk->emit_instruction(Instruction::ASSIGN_LIST, expr.name.line);
    } else if (expr.object->synthesized_attrs.info->primitive == Type::CLASS &&
               expr.name.type == TokenType::IDENTIFIER) {
        compile(expr.object.get());
        current_chunk->emit_constant(
            Value{get_member_index(expr.object->synthesized_attrs.class_, expr.name.lexeme)}, expr.name.line);
        compile(expr.value.get());
        current_chunk->emit_instruction(Instruction::ASSIGN_LIST, expr.synthesized_attrs.token.line);
    }
    return {};
}

ExprVisitorType ByteCodeGenerator::visit(SuperExpr &expr) {
    return {};
}

ExprVisitorType ByteCodeGenerator::visit(TernaryExpr &expr) {
    /*
     * Consider `false ? 1 : 2`
     *
     * This will compile to
     *
     * PUSH_FALSE
     * POP_JUMP_IF_FALSE       | offset = +12 bytes, jump to = 16 ---+
     * CONSTANT                -> 0 | value = 1                      |
     * JUMP_FORWARD            | offset = +8 bytes, jump to = 20 ----+-+
     * CONSTANT                -> 1 | value = 2 <--------------------+ |
     * POP  <----------------------------------------------------------+
     * HALT
     */
    compile(expr.left.get());
    if (expr.left->synthesized_attrs.info->is_ref) {
        current_chunk->emit_instruction(Instruction::DEREF, expr.left->synthesized_attrs.token.line);
    }

    std::size_t condition_jump_idx =
        current_chunk->emit_instruction(Instruction::POP_JUMP_IF_FALSE, expr.synthesized_attrs.token.line);
    emit_operand(0);

    compile(expr.middle.get());

    std::size_t over_false_idx =
        current_chunk->emit_instruction(Instruction::JUMP_FORWARD, expr.synthesized_attrs.token.line);
    emit_operand(0);
    std::size_t false_to_idx = current_chunk->bytes.size();

    compile(expr.right.get());

    std::size_t true_to_idx = current_chunk->bytes.size();

    patch_jump(condition_jump_idx, false_to_idx - condition_jump_idx - 1);
    patch_jump(over_false_idx, true_to_idx - over_false_idx - 1);
    return {};
}

ExprVisitorType ByteCodeGenerator::visit(ThisExpr &expr) {
    current_chunk->emit_instruction(Instruction::ACCESS_LOCAL_LIST, expr.keyword.line);
    emit_operand(0);
    return {};
}

ExprVisitorType ByteCodeGenerator::visit(TupleExpr &expr) {
    current_chunk->emit_instruction(Instruction::MAKE_LIST, expr.synthesized_attrs.token.line);
    emit_operand(expr.elements.size());

    std::size_t i = 0;
    for (auto &element : expr.elements) {
        auto &elem_expr = std::get<ExprNode>(element);

        current_chunk->emit_instruction(Instruction::ACCESS_FROM_TOP, expr.synthesized_attrs.token.line);
        emit_operand(1);
        current_chunk->emit_constant(Value{static_cast<Value::IntType>(i)}, elem_expr->synthesized_attrs.token.line);

        if (expr.type->types[i]->is_ref && elem_expr->synthesized_attrs.is_lvalue) {
            if (elem_expr->type_tag() == NodeType::VariableExpr) {
                auto *var = dynamic_cast<VariableExpr *>(elem_expr.get());
                current_chunk->emit_instruction(var->type == IdentifierType::LOCAL ? Instruction::MAKE_REF_TO_LOCAL
                                                                                   : Instruction::MAKE_REF_TO_GLOBAL,
                    var->name.line);
                emit_stack_slot(var->synthesized_attrs.stack_slot);
            } else if (elem_expr->type_tag() == NodeType::IndexExpr) {
                auto *list = dynamic_cast<IndexExpr *>(elem_expr.get());
                compile(list->object.get());
                compile(list->index.get());
                current_chunk->emit_instruction(Instruction::MAKE_REF_TO_INDEX, list->synthesized_attrs.token.line);
            } else if (elem_expr->type_tag() == NodeType::GetExpr) {
                auto *get = dynamic_cast<GetExpr *>(elem_expr.get());
                compile(get->object.get());
                if (get->object->synthesized_attrs.info->primitive == Type::TUPLE) {
                    Value::IntType index = std::stoi(get->name.lexeme);
                    current_chunk->emit_constant(Value{index}, get->name.line);
                } else if (get->object->synthesized_attrs.info->primitive == Type::CLASS) {
                    current_chunk->emit_constant(
                        Value{get_member_index(get_class(get->object), get->name.lexeme)}, get->name.line);
                }
                current_chunk->emit_instruction(Instruction::MAKE_REF_TO_INDEX, get->synthesized_attrs.token.line);
            }
        } else {
            compile(elem_expr.get());
        }

        if (std::get<RequiresCopy>(element)) {
            current_chunk->emit_instruction(Instruction::COPY_LIST, expr.synthesized_attrs.token.line);
        }
        if (std::get<NumericConversionType>(element) != NumericConversionType::NONE) {
            emit_conversion(std::get<NumericConversionType>(element), elem_expr->synthesized_attrs.token.line);
        }
        current_chunk->emit_instruction(Instruction::ASSIGN_LIST, expr.synthesized_attrs.token.line);
        current_chunk->emit_instruction(Instruction::POP, expr.synthesized_attrs.token.line);
        i++;
    }
    return {};
}

ExprVisitorType ByteCodeGenerator::visit(UnaryExpr &expr) {
    if (expr.oper.type != TokenType::PLUS_PLUS && expr.oper.type != TokenType::MINUS_MINUS) {
        compile(expr.right.get());
    }
    bool requires_floating = expr.right->synthesized_attrs.info->primitive == Type::FLOAT;
    switch (expr.oper.type) {
        case TokenType::BIT_NOT: current_chunk->emit_instruction(Instruction::BIT_NOT, expr.oper.line); break;
        case TokenType::NOT: current_chunk->emit_instruction(Instruction::NOT, expr.oper.line); break;
        case TokenType::MINUS:
            current_chunk->emit_instruction(requires_floating ? Instruction::FNEG : Instruction::INEG, expr.oper.line);
            break;
        case TokenType::PLUS_PLUS:
        case TokenType::MINUS_MINUS: {
            if (expr.right->type_tag() == NodeType::VariableExpr) {
                auto *variable = dynamic_cast<VariableExpr *>(expr.right.get());

                current_chunk->emit_instruction(
                    variable->type == IdentifierType::LOCAL ? Instruction::ACCESS_LOCAL : Instruction::ACCESS_GLOBAL,
                    variable->synthesized_attrs.token.line);
                emit_stack_slot(variable->synthesized_attrs.stack_slot);

                if (variable->synthesized_attrs.info->primitive == Type::FLOAT) {
                    current_chunk->emit_constant(Value{1.0}, expr.oper.line);
                    current_chunk->emit_instruction(
                        expr.oper.type == TokenType::PLUS_PLUS ? Instruction::FADD : Instruction::FSUB, expr.oper.line);
                } else if (variable->synthesized_attrs.info->primitive == Type::INT) {
                    current_chunk->emit_constant(Value{1}, expr.oper.line);
                    current_chunk->emit_instruction(
                        expr.oper.type == TokenType::PLUS_PLUS ? Instruction::IADD : Instruction::ISUB, expr.oper.line);
                }
                current_chunk->emit_instruction(
                    variable->type == IdentifierType::LOCAL ? Instruction::ASSIGN_LOCAL : Instruction::ASSIGN_GLOBAL,
                    expr.oper.line);
                emit_stack_slot(variable->synthesized_attrs.stack_slot);
            }
            break;
        }
        default:
            compile_ctx->logger.error(
                current_module, {"Bug in parser with illegal type for unary expression"}, expr.oper);
            break;
    }
    return {};
}

ExprVisitorType ByteCodeGenerator::visit(VariableExpr &expr) {
    switch (expr.type) {
        case IdentifierType::LOCAL:
        case IdentifierType::GLOBAL:
            if (expr.synthesized_attrs.stack_slot < Chunk::const_long_max) {
                if (expr.type == IdentifierType::LOCAL) {
                    if (is_nontrivial_type(expr.synthesized_attrs.info->primitive)) {
                        current_chunk->emit_instruction(Instruction::ACCESS_LOCAL_LIST, expr.name.line);
                    } else {
                        current_chunk->emit_instruction(Instruction::ACCESS_LOCAL, expr.name.line);
                    }
                } else {
                    if (is_nontrivial_type(expr.synthesized_attrs.info->primitive)) {
                        current_chunk->emit_instruction(Instruction::ACCESS_GLOBAL_LIST, expr.name.line);
                    } else {
                        current_chunk->emit_instruction(Instruction::ACCESS_GLOBAL, expr.name.line);
                    }
                }
                emit_stack_slot(expr.synthesized_attrs.stack_slot);
            } else {
                compile_ctx->logger.fatal_error({"Too many variables in current scope"});
            }
            return {};
        case IdentifierType::FUNCTION:
            current_chunk->emit_string(expr.name.lexeme, expr.name.line);
            current_chunk->emit_instruction(Instruction::LOAD_FUNCTION_SAME_MODULE, expr.name.line);
            return {};
        case IdentifierType::CLASS: break;
    }
    unreachable();
}

StmtVisitorType ByteCodeGenerator::visit(BlockStmt &stmt) {
    begin_scope();
    for (auto &statement : stmt.stmts) {
        compile(statement.get());
        // Statements written after a return statement will never execute anyway, so it's better to not emit anything
        // after one
        if (statement->type_tag() == NodeType::ReturnStmt) {
            // Note that a ReturnStmt automatically destroys the locals on the stack, so there's no need to call
            // destroy_locals() through end_scope() and instead just directly call remove_topmost_scope() and return
            remove_topmost_scope();
            return;
        }
    }
    end_scope();
}

StmtVisitorType ByteCodeGenerator::visit(BreakStmt &stmt) {
    std::size_t break_idx = current_chunk->emit_instruction(Instruction::JUMP_FORWARD, stmt.keyword.line);
    emit_operand(0);
    break_stmts.top().push_back(break_idx);
}

StmtVisitorType ByteCodeGenerator::visit(ClassStmt &stmt) {
    for (auto &method : stmt.methods) {
        compile(method.first.get());
    }
}

StmtVisitorType ByteCodeGenerator::visit(ContinueStmt &stmt) {
    std::size_t continue_idx = current_chunk->emit_instruction(Instruction::JUMP_FORWARD, stmt.keyword.line);
    emit_operand(0);
    continue_stmts.top().push_back(continue_idx);
}

StmtVisitorType ByteCodeGenerator::visit(ExpressionStmt &stmt) {
    compile(stmt.expr.get());
    if (stmt.expr->synthesized_attrs.info->primitive == Type::STRING) {
        current_chunk->emit_instruction(Instruction::POP_STRING, current_chunk->line_numbers.back().first);
    } else if (is_nontrivial_type(stmt.expr->synthesized_attrs.info->primitive)) {
        current_chunk->emit_instruction(Instruction::POP_LIST, current_chunk->line_numbers.back().first);
    } else {
        current_chunk->emit_instruction(Instruction::POP, current_chunk->line_numbers.back().first);
    }
}

StmtVisitorType ByteCodeGenerator::visit(FunctionStmt &stmt) {
    begin_scope();
    RuntimeFunction function{};
    for (FunctionStmt::ParameterType &param : stmt.params) {
        if (param.first.index() == FunctionStmt::IDENT_TUPLE) {
            function.arity += vartuple_size(std::get<IdentifierTuple>(param.first).tuple);
        } else {
            function.arity += 1;
        }
    }

    function.name = mangle_function(stmt);

    for (auto &param : stmt.params) {
        if (param.first.index() == FunctionStmt::IDENT_TUPLE) {
            add_vartuple_to_scope(std::get<IdentifierTuple>(param.first).tuple);
        } else {
            add_to_scope(param.second.get());
        }
    }

    current_chunk = &function.code;
    compile(stmt.body.get());

    remove_topmost_scope();

    if (stmt.return_type->primitive != Type::NULL_) {
        if (auto *body = dynamic_cast<BlockStmt *>(stmt.body.get());
            (not body->stmts.empty() && body->stmts.back()->type_tag() != NodeType::ReturnStmt) ||
            body->stmts.empty()) {
            current_chunk->emit_instruction(Instruction::TRAP_RETURN, stmt.name.line);
        }
    }

    current_compiled->functions[mangle_function(stmt)] = std::move(function);
    current_chunk = &current_compiled->top_level_code;
}

StmtVisitorType ByteCodeGenerator::visit(IfStmt &stmt) {
    compile(stmt.condition.get());
    if (stmt.condition->synthesized_attrs.info->is_ref) {
        current_chunk->emit_instruction(Instruction::DEREF, stmt.condition->synthesized_attrs.token.line);
    }
    std::size_t jump_idx = current_chunk->emit_instruction(Instruction::POP_JUMP_IF_FALSE, stmt.keyword.line);
    emit_operand(0);
    // Reserve three bytes for the offset
    compile(stmt.thenBranch.get());

    std::size_t over_else = 0;
    if (stmt.elseBranch != nullptr) {
        over_else = current_chunk->emit_instruction(Instruction::JUMP_FORWARD, stmt.keyword.line);
        emit_operand(0);
    }

    std::size_t before_else = current_chunk->bytes.size();
    patch_jump(jump_idx, before_else - jump_idx - 1);
    /*
     * The -1 because:
     *
     * POP_JUMP_IF_FALSE
     * BYTE1 -+
     * BYTE2  |-> The three byte offset from the POP_JUMP_IF_FALSE instruction to the else statement
     * BYTE3 -+
     * ... <- This is the body of the if statement (The ip will be here when the jump happens, but `jump_idx` will be
     *                                              considered for POP_JUMP_IF_FALSE)
     * JUMP_FORWARD
     * BYTE1
     * BYTE2
     * BYTE3 <- This will be where `before_else` is considered
     * ... (The else statement, if it exists)
     * INSTRUCTION <- This is where the JUMP_FORWARD will jump to
     */
    if (stmt.elseBranch != nullptr) {
        compile(stmt.elseBranch.get());

        std::size_t after_else = current_chunk->bytes.size();
        patch_jump(over_else, after_else - over_else - 1);
    }
}

StmtVisitorType ByteCodeGenerator::visit(ReturnStmt &stmt) {
    if (stmt.value != nullptr) {
        compile(stmt.value.get());
        if (auto &return_type = stmt.function->return_type; is_nontrivial_type(return_type->primitive) &&
                                                            not return_type->is_ref &&
                                                            stmt.value->synthesized_attrs.is_lvalue) {
            current_chunk->emit_instruction(Instruction::COPY_LIST, stmt.keyword.line);
        }
    } else {
        current_chunk->emit_instruction(Instruction::PUSH_NULL, stmt.keyword.line);
    }

    if (not is_constructor(stmt.function) && not is_destructor(stmt.function)) {
        current_chunk->emit_instruction(Instruction::ASSIGN_LOCAL, stmt.keyword.line);
        emit_operand(0);
    }
    current_chunk->emit_instruction(Instruction::POP, stmt.keyword.line);

    destroy_locals(stmt.function->scope_depth + 1);

    current_chunk->emit_instruction(Instruction::RETURN, stmt.keyword.line);
    emit_operand(stmt.locals_popped);
}

StmtVisitorType ByteCodeGenerator::visit(SwitchStmt &stmt) {
    /*
     * Consider the following code:
     *
     * switch (1) {
     *     case 1: 1
     *     case 2: { 5; break; }
     *     default: 6
     * }
     *
     * This will compile to:
     *
     * CONSTANT          -> 0 | value = 1
     * CONSTANT          -> 1 | value = 1
     * POP_JUMP_IF_EQUAL | offset = +16 bytes, jump to = 24 ---+ <- case 1:
     * CONSTANT          -> 2 | value = 2                      |
     * POP_JUMP_IF_EQUAL | offset = +16 bytes, jump to = 32 ---+-+ <- case 2:
     * JUMP_FORWARD      | offset = +24 bytes, jump to = 44 ---+-+-+ <- default:
     * CONSTANT          -> 3 | value = 1 <--------------------+ | |
     * POP                                                       | |
     * CONSTANT          -> 4 | value = 5 <----------------------+ |
     * POP                                                         |
     * JUMP_FORWARD      | offset = +12 bytes, jump to = 52 -------+-+ <- The break statement
     * CONSTANT          -> 5 | value = 6 <------------------------+ |
     * POP <---------------------------------------------------------+
     * HALT
     *
     */
    break_stmts.emplace();
    compile(stmt.condition.get());
    if (stmt.condition->synthesized_attrs.info->is_ref) {
        current_chunk->emit_instruction(Instruction::DEREF, stmt.condition->synthesized_attrs.token.line);
    }
    std::vector<std::size_t> jumps{};
    std::size_t default_jump{};
    for (auto &case_ : stmt.cases) {
        compile(case_.first.get());
        jumps.push_back(
            current_chunk->emit_instruction(Instruction::POP_JUMP_IF_EQUAL, current_chunk->line_numbers.back().first));
        emit_operand(0);
    }
    if (stmt.default_case != nullptr) {
        default_jump = current_chunk->emit_instruction(Instruction::JUMP_FORWARD, 0);
        emit_operand(0);
    }

    std::size_t i = 0;
    for (auto &case_ : stmt.cases) {
        std::size_t jump_to = current_chunk->bytes.size();
        patch_jump(jumps[i], jump_to - jumps[i] - 1);
        compile(case_.second.get());
        i++;
    }
    if (stmt.default_case != nullptr) {
        std::size_t jump_to = current_chunk->bytes.size();
        patch_jump(default_jump, jump_to - default_jump - 1);
        compile(stmt.default_case.get());
    }

    for (std::size_t break_stmt : break_stmts.top()) {
        std::size_t jump_to = current_chunk->bytes.size();
        patch_jump(break_stmt, jump_to - break_stmt - 1);
    }

    break_stmts.pop();
}

StmtVisitorType ByteCodeGenerator::visit(TypeStmt &stmt) {}

StmtVisitorType ByteCodeGenerator::visit(VarStmt &stmt) {
    if (stmt.type->is_ref && not stmt.initializer->synthesized_attrs.info->is_ref) {
        make_ref_to(stmt.initializer);
    } else {
        compile(stmt.initializer.get());
        if (stmt.initializer->synthesized_attrs.info->is_ref && not stmt.type->is_ref &&
            stmt.initializer->synthesized_attrs.info->primitive != Type::LIST &&
            stmt.initializer->synthesized_attrs.info->primitive != Type::TUPLE &&
            stmt.initializer->synthesized_attrs.info->primitive != Type::CLASS) {
            current_chunk->emit_instruction(Instruction::DEREF, stmt.name.line);
        }

        if (stmt.conversion_type != NumericConversionType::NONE) {
            emit_conversion(stmt.conversion_type, stmt.name.line);
        }
    }
    if (stmt.requires_copy) {
        current_chunk->emit_instruction(Instruction::COPY_LIST, stmt.name.line);
    }
    if (not variable_tracking_suppressed) {
        add_to_scope(stmt.type.get());
    }
}

StmtVisitorType ByteCodeGenerator::visit(VarTupleStmt &stmt) {
    compile(stmt.initializer.get());
    if (requires_copy(stmt.initializer, stmt.type)) {
        current_chunk->emit_instruction(Instruction::COPY_LIST, stmt.token.line);
    }
    compile_vartuple(stmt.names.tuple, dynamic_cast<TupleType &>(*stmt.type));

    if (not variable_tracking_suppressed) {
        add_vartuple_to_scope(stmt.names.tuple);
    }
}

StmtVisitorType ByteCodeGenerator::visit(WhileStmt &stmt) {
    /*
     * Taking an example of
     * for (var i = 0; i < 5; i = i + 1) {
     *      i
     * }
     * This code will parse into
     * {
     *      var i = 0;
     *      while(i < 5) {
     *          i
     *      }
     * }
     * (and the i = i + 1 is stored separately)
     *
     * This will compile to
     *
     * CONSTANT              -> 0 | value = 0
     * JUMP_FORWARD          | offset = +32 bytes, jump to = 36 -+
     * ACCESS_LOCAL          | access local 0 <------------------+-+ ) - These two instructions are the body of the loop
     * POP                                                       | | )
     * ACCESS_LOCAL          | access local 0                    | | } - These five instructions are the increment
     * CONSTANT              -> 1 | value = 1                    | | }
     * IADD                                                      | | }
     * ASSIGN_LOCAL          | assign to local 0                 | | }
     * POP                                                       | | }
     * ACCESS_LOCAL          | access local 0 <------------------+ | ] - These three instructions are the condition
     * CONSTANT              -> 2 | value = 5                      | ]
     * LESSER                                                      | ]
     * POP_JUMP_BACK_IF_TRUE | offset = -40 bytes, jump to = 8 ----+
     * POP
     * HALT
     *
     *   From this, the control flow should be obvious. I have tried to mirror
     *   what gcc generates for a loop in C.
     */
    break_stmts.emplace();
    continue_stmts.emplace();

    std::size_t jump_begin_idx = current_chunk->emit_instruction(Instruction::JUMP_FORWARD, stmt.keyword.line);
    emit_operand(0);

    std::size_t loop_back_idx = current_chunk->bytes.size();
    compile(stmt.body.get());

    std::size_t increment_idx = current_chunk->bytes.size();
    if (stmt.increment != nullptr) {
        compile(stmt.increment.get());
    }

    std::size_t condition_idx = current_chunk->bytes.size();
    compile(stmt.condition.get());
    if (stmt.condition->synthesized_attrs.info->is_ref) {
        current_chunk->emit_instruction(Instruction::DEREF, stmt.condition->synthesized_attrs.token.line);
    }

    std::size_t jump_back_idx = current_chunk->emit_instruction(Instruction::POP_JUMP_BACK_IF_TRUE, stmt.keyword.line);
    emit_operand(0);

    std::size_t loop_end_idx = current_chunk->bytes.size();

    patch_jump(jump_back_idx, jump_back_idx - loop_back_idx + 1);
    patch_jump(jump_begin_idx, condition_idx - jump_begin_idx - 1);

    for (std::size_t continue_idx : continue_stmts.top()) {
        patch_jump(continue_idx, increment_idx - continue_idx - 1);
    }

    for (std::size_t break_idx : break_stmts.top()) {
        patch_jump(break_idx, loop_end_idx - break_idx - 1);
    }

    continue_stmts.pop();
    break_stmts.pop();
}

BaseTypeVisitorType ByteCodeGenerator::visit(PrimitiveType &type) {
    return {};
}

BaseTypeVisitorType ByteCodeGenerator::visit(UserDefinedType &type) {
    return {};
}

BaseTypeVisitorType ByteCodeGenerator::visit(ListType &type) {
    return {};
}

BaseTypeVisitorType ByteCodeGenerator::visit(TupleType &type) {
    return {};
}

BaseTypeVisitorType ByteCodeGenerator::visit(TypeofType &type) {
    return {};
}
