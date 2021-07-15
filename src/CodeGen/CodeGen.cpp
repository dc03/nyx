/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "CodeGen.hpp"

#include "../Common.hpp"
#include "../ErrorLogger/ErrorLogger.hpp"
#include "../VirtualMachine/Value.hpp"

std::vector<RuntimeModule> Generator::compiled_modules{};

Generator::Generator() {
    for (auto &native : native_functions) {
        natives[native.name] = native;
    }
}

void Generator::begin_scope() {
    scopes.push({});
}

void Generator::end_scope() {
    for (auto begin = scopes.top().crbegin(); begin != scopes.top().crend(); begin++) {
        if ((*begin)->data.primitive == Type::STRING) {
            current_chunk->emit_instruction(Instruction::POP_STRING, 0);
        } else if ((*begin)->data.primitive == Type::LIST && !(*begin)->data.is_ref) {
            current_chunk->emit_instruction(Instruction::POP_LIST, 0);
        } else {
            current_chunk->emit_instruction(Instruction::POP, 0);
        }
    }
    scopes.pop();
}

void Generator::patch_jump(std::size_t jump_idx, std::size_t jump_amount) {
    if (jump_amount >= Chunk::const_long_max) {
        compile_error("Size of jump is greater than that allowed by the instruction set");
        return;
    }

    current_chunk->bytes[jump_idx] |= jump_amount & 0x00ff'ffff;
}

RuntimeModule Generator::compile(Module &module) {
    begin_scope();
    RuntimeModule compiled{};
    current_chunk = &compiled.top_level_code;
    current_module = &module;
    current_compiled = &compiled;

    for (auto &stmt : module.statements) {
        if (stmt != nullptr) {
            compile(stmt.get());
        }
    }

    end_scope();
    return compiled;
}

void Generator::emit_conversion(NumericConversionType conversion_type, std::size_t line_number) {
    using namespace std::string_literals;
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

void Generator::emit_three_bytes_of(std::size_t value) {
    current_chunk->bytes.back() |= value & 0x00ff'ffff;
}

ExprVisitorType Generator::compile(Expr *expr) {
    return expr->accept(*this);
}

StmtVisitorType Generator::compile(Stmt *stmt) {
    stmt->accept(*this);
}

BaseTypeVisitorType Generator::compile(BaseType *type) {
    return type->accept(*this);
}

ExprVisitorType Generator::visit(AssignExpr &expr) {
    auto compile_right = [&expr, this] {
        compile(expr.value.get());
        if (expr.value->resolved.info->data.is_ref && expr.value->resolved.info->data.primitive != Type::LIST) {
            current_chunk->emit_instruction(Instruction::DEREF, expr.target.line);
        } // As there is no difference between a list and a reference to a list (aside from the tag), there is no need
          // to add an explicit Instruction::DEREF
        if (expr.requires_copy) {
            current_chunk->emit_instruction(Instruction::COPY_LIST, expr.target.line);
        }

        if (expr.conversion_type != NumericConversionType::NONE) {
            emit_conversion(expr.conversion_type, expr.resolved.token.line);
        }
    };

    switch (expr.resolved.token.type) {
        case TokenType::EQUAL:
            compile_right();
            // Assigning to lists needs to be handled separately, as it can involve destroying the list that was
            // previously there in the variable. This code is moved off of the hot path into ASSIGN_LOCAL_LIST and
            // ASSIGN_GLOBAL_LIST
            if (expr.resolved.info->data.primitive == Type::LIST) {
                current_chunk->emit_instruction(expr.target_type == IdentifierType::LOCAL
                                                    ? Instruction::ASSIGN_LOCAL_LIST
                                                    : Instruction::ASSIGN_GLOBAL_LIST,
                    expr.resolved.token.line);
            } else {
                current_chunk->emit_instruction(
                    expr.target_type == IdentifierType::LOCAL ? Instruction::ASSIGN_LOCAL : Instruction::ASSIGN_GLOBAL,
                    expr.resolved.token.line);
            }
            break;
        default: {
            current_chunk->emit_instruction(
                expr.target_type == IdentifierType::LOCAL ? Instruction::ACCESS_LOCAL : Instruction::ACCESS_GLOBAL,
                expr.resolved.token.line);
            emit_three_bytes_of(expr.resolved.stack_slot);
            if (expr.resolved.info->data.is_ref) {
                current_chunk->emit_instruction(Instruction::DEREF, expr.resolved.token.line);
            }
            compile_right();
            switch (expr.resolved.token.type) {
                case TokenType::PLUS_EQUAL:
                    current_chunk->emit_instruction(
                        expr.resolved.info->data.primitive == Type::FLOAT ? Instruction::FADD : Instruction::IADD,
                        expr.resolved.token.line);
                    break;
                case TokenType::MINUS_EQUAL:
                    current_chunk->emit_instruction(
                        expr.resolved.info->data.primitive == Type::FLOAT ? Instruction::FSUB : Instruction::ISUB,
                        expr.resolved.token.line);
                    break;
                case TokenType::STAR_EQUAL:
                    current_chunk->emit_instruction(
                        expr.resolved.info->data.primitive == Type::FLOAT ? Instruction::FMUL : Instruction::IMUL,
                        expr.resolved.token.line);
                    break;
                case TokenType::SLASH_EQUAL:
                    current_chunk->emit_instruction(
                        expr.resolved.info->data.primitive == Type::FLOAT ? Instruction::FDIV : Instruction::IDIV,
                        expr.resolved.token.line);
                    break;
                default: break;
            }
            current_chunk->emit_instruction(Instruction::ASSIGN_LOCAL, expr.resolved.token.line);
            break;
        }
    }

    emit_three_bytes_of(expr.resolved.stack_slot);
    return {};
}

ExprVisitorType Generator::visit(BinaryExpr &expr) {
    compile(expr.left.get());
    if (expr.left->resolved.info->data.is_ref) {
        current_chunk->emit_instruction(Instruction::DEREF, expr.resolved.token.line);
    }

    bool requires_floating = expr.left->resolved.info->data.primitive == Type::FLOAT ||
                             expr.right->resolved.info->data.primitive == Type::FLOAT;

    if (expr.left->resolved.info->data.primitive == Type::INT &&
        expr.right->resolved.info->data.primitive == Type::FLOAT) {
        current_chunk->emit_instruction(Instruction::INT_TO_FLOAT, expr.left->resolved.token.line);
    }

    compile(expr.right.get());
    if (expr.right->resolved.info->data.is_ref) {
        current_chunk->emit_instruction(Instruction::DEREF, expr.resolved.token.line);
    }
    if (expr.left->resolved.info->data.primitive == Type::FLOAT &&
        expr.right->resolved.info->data.primitive == Type::INT) {
        current_chunk->emit_instruction(Instruction::INT_TO_FLOAT, expr.left->resolved.token.line);
    }

    switch (expr.resolved.token.type) {
        case TokenType::LEFT_SHIFT:
            current_chunk->emit_instruction(Instruction::SHIFT_LEFT, expr.resolved.token.line);
            break;
        case TokenType::RIGHT_SHIFT:
            current_chunk->emit_instruction(Instruction::SHIFT_RIGHT, expr.resolved.token.line);
            break;
        case TokenType::BIT_AND: current_chunk->emit_instruction(Instruction::BIT_AND, expr.resolved.token.line); break;
        case TokenType::BIT_OR: current_chunk->emit_instruction(Instruction::BIT_OR, expr.resolved.token.line); break;
        case TokenType::BIT_XOR: current_chunk->emit_instruction(Instruction::BIT_XOR, expr.resolved.token.line); break;
        case TokenType::MODULO:
            current_chunk->emit_instruction(
                requires_floating ? Instruction::FMOD : Instruction::IMOD, expr.resolved.token.line);
            break;

        case TokenType::EQUAL_EQUAL:
            current_chunk->emit_instruction(Instruction::EQUAL, expr.resolved.token.line);
            break;
        case TokenType::GREATER: current_chunk->emit_instruction(Instruction::GREATER, expr.resolved.token.line); break;
        case TokenType::LESS: current_chunk->emit_instruction(Instruction::LESSER, expr.resolved.token.line); break;

        case TokenType::NOT_EQUAL:
            current_chunk->emit_instruction(Instruction::EQUAL, expr.resolved.token.line);
            current_chunk->emit_instruction(Instruction::NOT, expr.resolved.token.line);
            break;
        case TokenType::GREATER_EQUAL:
            current_chunk->emit_instruction(Instruction::LESSER, expr.resolved.token.line);
            current_chunk->emit_instruction(Instruction::NOT, expr.resolved.token.line);
            break;
        case TokenType::LESS_EQUAL:
            current_chunk->emit_instruction(Instruction::GREATER, expr.resolved.token.line);
            current_chunk->emit_instruction(Instruction::NOT, expr.resolved.token.line);
            break;

        case TokenType::PLUS:
            switch (expr.resolved.info->data.primitive) {
                case Type::INT: current_chunk->emit_instruction(Instruction::IADD, expr.resolved.token.line); break;
                case Type::FLOAT: current_chunk->emit_instruction(Instruction::FADD, expr.resolved.token.line); break;
                case Type::STRING:
                    current_chunk->emit_instruction(Instruction::CONCATENATE, expr.resolved.token.line);
                    break;
                default: unreachable();
            }
            break;

        case TokenType::MINUS:
            current_chunk->emit_instruction(
                requires_floating ? Instruction::FSUB : Instruction::ISUB, expr.resolved.token.line);
            break;
        case TokenType::SLASH:
            current_chunk->emit_instruction(
                requires_floating ? Instruction::FDIV : Instruction::IDIV, expr.resolved.token.line);
            break;
        case TokenType::STAR:
            current_chunk->emit_instruction(
                requires_floating ? Instruction::FMUL : Instruction::IMUL, expr.resolved.token.line);
            break;

        case TokenType::DOT_DOT:
        case TokenType::DOT_DOT_EQUAL: break;

        default: error("Bug in parser with illegal token type of expression's operator", expr.resolved.token); break;
    }

    return {};
}

ExprVisitorType Generator::visit(CallExpr &expr) {
    // Emit a null value on the stack just before the parameters of the function which is being called. This will serve
    // as the stack slot in which to save the return value of the function before destroying any arguments or locals.
    current_chunk->emit_instruction(Instruction::PUSH_NULL, expr.resolved.token.line);
    std::size_t i = 0;
    for (auto &arg : expr.args) {
        auto &value = std::get<ExprNode>(arg);
        if (!expr.is_native_call) {
            if (auto &param = expr.function->resolved.func->params[i];
                param.second->data.is_ref && !value->resolved.info->data.is_ref) {
                if (value->type_tag() == NodeType::VariableExpr) {
                    if (dynamic_cast<VariableExpr *>(value.get())->type == IdentifierType::LOCAL) {
                        current_chunk->emit_instruction(Instruction::MAKE_REF_TO_LOCAL, value->resolved.token.line);
                    } else {
                        current_chunk->emit_instruction(Instruction::MAKE_REF_TO_GLOBAL, value->resolved.token.line);
                    }
                } else {
                    current_chunk->emit_instruction(Instruction::MAKE_REF_TO_LOCAL, value->resolved.token.line);
                    // This is a fallback, but I don't think it would ever be triggered
                }
                emit_three_bytes_of(value->resolved.stack_slot);
            } else if (!param.second->data.is_ref && value->resolved.info->data.is_ref) {
                compile(value.get());
                current_chunk->emit_instruction(Instruction::DEREF, value->resolved.token.line);
            } else {
                compile(value.get());
            }
        } else {
            compile(value.get());
        }

        if (std::get<NumericConversionType>(arg) != NumericConversionType::NONE) {
            emit_conversion(std::get<NumericConversionType>(arg), value->resolved.token.line);
        }
        if (std::get<RequiresCopy>(arg)) {
            current_chunk->emit_instruction(Instruction::COPY_LIST, value->resolved.token.line);
        }

        // This ALLOC_AT_LEAST is emitted after the COPY since the list that is copied needs to be resized and not the
        // list that is being copied.
        if (!expr.is_native_call && expr.function->resolved.func->params[i].second->data.primitive == Type::LIST) {
            auto *list = dynamic_cast<ListType *>(expr.function->resolved.func->params[i].second.get());
            if (list->size != nullptr) {
                compile(list->size.get());
                current_chunk->emit_instruction(Instruction::MAKE_LIST, list->size->resolved.token.line);
            }
        }
        i++;
    }
    if (expr.is_native_call) {
        auto *called = dynamic_cast<VariableExpr *>(expr.function.get());
        current_chunk->emit_string(called->name.lexeme, called->name.line);
        current_chunk->emit_instruction(Instruction::CALL_NATIVE, expr.resolved.token.line);
        auto begin = expr.args.crbegin();
        for (; begin != expr.args.crend(); begin++) {
            auto &arg = std::get<ExprNode>(*begin);
            if (arg->resolved.info->data.primitive == Type::LIST && !arg->resolved.is_lvalue &&
                !arg->resolved.info->data.is_ref) {
                current_chunk->emit_instruction(Instruction::POP_LIST, arg->resolved.token.line);
            } else if (arg->resolved.info->data.primitive == Type::STRING) {
                current_chunk->emit_instruction(Instruction::POP_STRING, arg->resolved.token.line);
            } else {
                current_chunk->emit_instruction(Instruction::POP, arg->resolved.token.line);
            }
        }
    } else {
        compile(expr.function.get());
        current_chunk->emit_instruction(Instruction::CALL_FUNCTION, expr.resolved.token.line);
    }
    return {};
}

ExprVisitorType Generator::visit(CommaExpr &expr) {
    auto it = begin(expr.exprs);

    for (auto next = std::next(it); next != end(expr.exprs); it = next, ++next) {
        compile(it->get());
        if ((*it)->resolved.info->data.primitive == Type::STRING) {
            current_chunk->emit_instruction(Instruction::POP_STRING, (*it)->resolved.token.line);
        } else if ((*it)->resolved.info->data.primitive == Type::LIST && !(*it)->resolved.is_lvalue) {
            current_chunk->emit_instruction(Instruction::POP_LIST, (*it)->resolved.token.line);
        } else {
            current_chunk->emit_instruction(Instruction::POP, (*it)->resolved.token.line);
        }
    }

    compile(it->get());
    return {};
}

ExprVisitorType Generator::visit(GetExpr &expr) {
    return {};
}

ExprVisitorType Generator::visit(GroupingExpr &expr) {
    compile(expr.expr.get());
    return {};
}

ExprVisitorType Generator::visit(IndexExpr &expr) {
    compile(expr.object.get());
    compile(expr.index.get());
    if (expr.index->resolved.info->data.is_ref) {
        current_chunk->emit_instruction(Instruction::DEREF, expr.index->resolved.token.line);
    }
    current_chunk->emit_instruction(Instruction::CHECK_INDEX, expr.resolved.token.line);
    current_chunk->emit_instruction(Instruction::INDEX_LIST, expr.resolved.token.line);
    return {};
}

ExprVisitorType Generator::visit(ListExpr &expr) {
    current_chunk->emit_constant(
        Value{dynamic_cast<LiteralExpr *>(expr.type->size.get())->value.to_int()}, expr.bracket.line);
    current_chunk->emit_instruction(Instruction::MAKE_LIST, expr.bracket.line);
    std::size_t stack_slot = 0;
    if (!scopes.empty()) {
        stack_slot = scopes.top().size();
    }
    std::size_t i = 0;
    for (ListExpr::ElementType &element : expr.elements) {
        auto &element_expr = std::get<ExprNode>(element);
        current_chunk->emit_instruction(Instruction::ACCESS_FROM_TOP, expr.resolved.token.line);
        emit_three_bytes_of(1);
        current_chunk->emit_constant(Value{static_cast<int>(i)}, element_expr->resolved.token.line);

        if (!expr.type->contained->data.is_ref) {
            // References have to be conditionally compiled when not binding to a name
            compile(element_expr.get());
            emit_conversion(std::get<NumericConversionType>(element), element_expr->resolved.token.line);
        }

        if (!expr.type->contained->data.is_ref) {
            // Type is not a reference type
            if (element_expr->resolved.info->data.is_ref) {
                current_chunk->emit_instruction(Instruction::DEREF, element_expr->resolved.token.line);
            }
        } else if (element_expr->resolved.is_lvalue) {
            // Type is a reference type
            if (element_expr->type_tag() == NodeType::VariableExpr) {
                auto *bound_var = dynamic_cast<VariableExpr *>(element_expr.get());
                if (bound_var->type == IdentifierType::LOCAL) {
                    current_chunk->emit_instruction(Instruction::MAKE_REF_TO_LOCAL, bound_var->name.line);
                } else if (bound_var->type == IdentifierType::GLOBAL) {
                    current_chunk->emit_instruction(Instruction::MAKE_REF_TO_GLOBAL, bound_var->name.line);
                }
                emit_three_bytes_of(bound_var->resolved.stack_slot);
            }
        } else {
            compile(element_expr.get()); // A reference not binding to an lvalue
            emit_conversion(std::get<NumericConversionType>(element), element_expr->resolved.token.line);
        }

        if (std::get<RequiresCopy>(element)) {
            current_chunk->emit_instruction(Instruction::COPY_LIST, element_expr->resolved.token.line);
        }

        current_chunk->emit_instruction(Instruction::ASSIGN_LIST, element_expr->resolved.token.line);
        current_chunk->emit_instruction(Instruction::POP, element_expr->resolved.token.line);
        i++;
    }
    return {};
}

ExprVisitorType Generator::visit(ListAssignExpr &expr) {
    compile(expr.list.object.get());
    compile(expr.list.index.get());
    if (expr.list.index->resolved.info->data.is_ref) {
        current_chunk->emit_instruction(Instruction::DEREF, expr.list.index->resolved.token.line);
    }
    current_chunk->emit_instruction(Instruction::CHECK_INDEX, expr.resolved.token.line);

    switch (expr.resolved.token.type) {
        case TokenType::EQUAL: {
            compile(expr.value.get());
            if (expr.value->resolved.info->data.is_ref) {
                current_chunk->emit_instruction(Instruction::DEREF, expr.value->resolved.token.line);
            }
            if (expr.requires_copy) {
                current_chunk->emit_instruction(Instruction::COPY_LIST, expr.resolved.token.line);
            }
            current_chunk->emit_instruction(Instruction::ASSIGN_LIST, expr.resolved.token.line);
            break;
        }

        default: {
            compile(expr.list.object.get());
            compile(expr.list.index.get());
            if (expr.list.index->resolved.info->data.is_ref) {
                current_chunk->emit_instruction(Instruction::DEREF, expr.list.index->resolved.token.line);
            }
            // There is no need for a CHECK_INDEX here because that index has already been checked before
            current_chunk->emit_instruction(Instruction::INDEX_LIST, expr.resolved.token.line);

            compile(expr.value.get());

            if (expr.conversion_type != NumericConversionType::NONE) {
                emit_conversion(expr.conversion_type, expr.resolved.token.line);
            }

            Type contained_type = dynamic_cast<ListType *>(expr.list.object->resolved.info)->contained->data.primitive;
            switch (expr.resolved.token.type) {
                case TokenType::PLUS_EQUAL:
                    current_chunk->emit_instruction(
                        contained_type == Type::FLOAT ? Instruction::FADD : Instruction::IADD,
                        expr.resolved.token.line);
                    break;
                case TokenType::MINUS_EQUAL:
                    current_chunk->emit_instruction(
                        contained_type == Type::FLOAT ? Instruction::FSUB : Instruction::ISUB,
                        expr.resolved.token.line);
                    break;
                case TokenType::STAR_EQUAL:
                    current_chunk->emit_instruction(
                        contained_type == Type::FLOAT ? Instruction::FMUL : Instruction::IMUL,
                        expr.resolved.token.line);
                    break;
                case TokenType::SLASH_EQUAL:
                    current_chunk->emit_instruction(
                        contained_type == Type::FLOAT ? Instruction::FDIV : Instruction::IDIV,
                        expr.resolved.token.line);
                    break;
                default: break;
            }

            current_chunk->emit_instruction(Instruction::ASSIGN_LIST, expr.resolved.token.line);
            break;
        }
    }
    return {};
}

ExprVisitorType Generator::visit(LiteralExpr &expr) {
    switch (expr.value.index()) {
        case LiteralValue::tag::INT:
            current_chunk->emit_constant(Value{expr.value.to_int()}, expr.resolved.token.line);
            break;
        case LiteralValue::tag::DOUBLE:
            current_chunk->emit_constant(Value{expr.value.to_double()}, expr.resolved.token.line);
            break;
        case LiteralValue::tag::STRING:
            current_chunk->emit_string(expr.value.to_string(), expr.resolved.token.line);
            break;
        case LiteralValue::tag::BOOL:
            if (expr.value.to_bool()) {
                current_chunk->emit_instruction(Instruction::PUSH_TRUE, expr.resolved.token.line);
            } else {
                current_chunk->emit_instruction(Instruction::PUSH_FALSE, expr.resolved.token.line);
            }
            break;
        case LiteralValue::tag::NULL_:
            current_chunk->emit_instruction(Instruction::PUSH_NULL, expr.resolved.token.line);
            break;
    }
    return {};
}

ExprVisitorType Generator::visit(LogicalExpr &expr) {
    compile(expr.left.get());
    if (expr.left->resolved.info->data.is_ref) {
        current_chunk->emit_instruction(Instruction::DEREF, expr.left->resolved.token.line);
    }
    std::size_t jump_idx{};
    if (expr.resolved.token.type == TokenType::OR) {
        jump_idx = current_chunk->emit_instruction(Instruction::JUMP_IF_TRUE, expr.resolved.token.line);
    } else { // Since || / or short circuits on true, flip the boolean on top of the stack
        jump_idx = current_chunk->emit_instruction(Instruction::JUMP_IF_FALSE, expr.resolved.token.line);
    }
    emit_three_bytes_of(0);
    current_chunk->emit_instruction(Instruction::POP, expr.resolved.token.line);
    compile(expr.right.get());
    std::size_t to_idx = current_chunk->bytes.size();
    patch_jump(jump_idx, to_idx - jump_idx - 1);
    return {};
}

ExprVisitorType Generator::visit(ScopeAccessExpr &expr) {
    return {};
}

ExprVisitorType Generator::visit(ScopeNameExpr &expr) {
    return {};
}

ExprVisitorType Generator::visit(SetExpr &expr) {
    return {};
}

ExprVisitorType Generator::visit(SuperExpr &expr) {
    return {};
}

ExprVisitorType Generator::visit(TernaryExpr &expr) {
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
    if (expr.left->resolved.info->data.is_ref) {
        current_chunk->emit_instruction(Instruction::DEREF, expr.left->resolved.token.line);
    }

    std::size_t condition_jump_idx =
        current_chunk->emit_instruction(Instruction::POP_JUMP_IF_FALSE, expr.resolved.token.line);
    emit_three_bytes_of(0);

    compile(expr.middle.get());

    std::size_t over_false_idx = current_chunk->emit_instruction(Instruction::JUMP_FORWARD, expr.resolved.token.line);
    emit_three_bytes_of(0);
    std::size_t false_to_idx = current_chunk->bytes.size();

    compile(expr.right.get());

    std::size_t true_to_idx = current_chunk->bytes.size();

    patch_jump(condition_jump_idx, false_to_idx - condition_jump_idx - 1);
    patch_jump(over_false_idx, true_to_idx - over_false_idx - 1);
    return {};
}

ExprVisitorType Generator::visit(ThisExpr &expr) {
    return {};
}

ExprVisitorType Generator::visit(UnaryExpr &expr) {
    if (expr.oper.type != TokenType::PLUS_PLUS && expr.oper.type != TokenType::MINUS_MINUS) {
        compile(expr.right.get());
    }
    bool requires_floating = expr.right->resolved.info->data.primitive == Type::FLOAT;
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
                    variable->resolved.token.line);
                emit_three_bytes_of(variable->resolved.stack_slot);

                if (variable->resolved.info->data.primitive == Type::FLOAT) {
                    current_chunk->emit_constant(Value{1.0}, expr.oper.line);
                    current_chunk->emit_instruction(
                        expr.oper.type == TokenType::PLUS_PLUS ? Instruction::FADD : Instruction::FSUB, expr.oper.line);
                } else if (variable->resolved.info->data.primitive == Type::INT) {
                    current_chunk->emit_constant(Value{1}, expr.oper.line);
                    current_chunk->emit_instruction(
                        expr.oper.type == TokenType::PLUS_PLUS ? Instruction::IADD : Instruction::ISUB, expr.oper.line);
                }
                current_chunk->emit_instruction(
                    variable->type == IdentifierType::LOCAL ? Instruction::ASSIGN_LOCAL : Instruction::ASSIGN_GLOBAL,
                    expr.oper.line);
                emit_three_bytes_of(variable->resolved.stack_slot);
            }
            break;
        }
        default: error("Bug in parser with illegal type for unary expression", expr.oper); break;
    }
    return {};
}

ExprVisitorType Generator::visit(VariableExpr &expr) {
    switch (expr.type) {
        case IdentifierType::LOCAL:
        case IdentifierType::GLOBAL:
            if (expr.resolved.stack_slot < Chunk::const_long_max) {
                if (expr.type == IdentifierType::LOCAL) {
                    if (expr.resolved.info->data.primitive == Type::STRING) {
                        current_chunk->emit_instruction(Instruction::ACCESS_LOCAL_STRING, expr.name.line);
                    } else {
                        current_chunk->emit_instruction(Instruction::ACCESS_LOCAL, expr.name.line);
                    }
                } else {
                    if (expr.resolved.info->data.primitive == Type::STRING) {
                        current_chunk->emit_instruction(Instruction::ACCESS_GLOBAL_STRING, expr.name.line);
                    } else {
                        current_chunk->emit_instruction(Instruction::ACCESS_GLOBAL, expr.name.line);
                    }
                }
                emit_three_bytes_of(expr.resolved.stack_slot);
            } else {
                compile_error("Too many variables in current scope");
            }
            return {};
        case IdentifierType::FUNCTION:
            current_chunk->emit_string(expr.name.lexeme, expr.name.line);
            current_chunk->emit_instruction(Instruction::LOAD_FUNCTION, expr.name.line);
            return {};
        case IdentifierType::CLASS: break;
    }
    unreachable();
}

StmtVisitorType Generator::visit(BlockStmt &stmt) {
    begin_scope();
    for (auto &statement : stmt.stmts) {
        compile(statement.get());
    }
    end_scope();
}

StmtVisitorType Generator::visit(BreakStmt &stmt) {
    std::size_t break_idx = current_chunk->emit_instruction(Instruction::JUMP_FORWARD, stmt.keyword.line);
    emit_three_bytes_of(0);
    break_stmts.top().push_back(break_idx);
}

StmtVisitorType Generator::visit(ClassStmt &stmt) {}

StmtVisitorType Generator::visit(ContinueStmt &stmt) {
    std::size_t continue_idx = current_chunk->emit_instruction(Instruction::JUMP_FORWARD, stmt.keyword.line);
    emit_three_bytes_of(0);
    continue_stmts.top().push_back(continue_idx);
}

StmtVisitorType Generator::visit(ExpressionStmt &stmt) {
    compile(stmt.expr.get());
    if (stmt.expr->resolved.info->data.primitive == Type::STRING) {
        current_chunk->emit_instruction(Instruction::POP_STRING, current_chunk->line_numbers.back().first);
    } else if (stmt.expr->resolved.info->data.primitive == Type::LIST) {
        current_chunk->emit_instruction(Instruction::POP_LIST, current_chunk->line_numbers.back().first);
    } else {
        current_chunk->emit_instruction(Instruction::POP, current_chunk->line_numbers.back().first);
    }
}

StmtVisitorType Generator::visit(FunctionStmt &stmt) {
    begin_scope();
    RuntimeFunction function{};
    function.arity = stmt.params.size();
    function.name = stmt.name.lexeme;

    current_chunk = &function.code;
    compile(stmt.body.get());

    end_scope();
    for (auto begin = stmt.params.crbegin(); begin != stmt.params.crend(); begin++) {
        if (begin->second->data.primitive == Type::STRING) {
            current_chunk->emit_instruction(Instruction::POP_STRING, 0);
        } else if (begin->second->data.primitive == Type::LIST && !begin->second->data.is_ref) {
            current_chunk->emit_instruction(Instruction::POP_LIST, 0);
        } else {
            current_chunk->emit_instruction(Instruction::POP, 0);
        }
    }

    if (stmt.return_type->data.primitive != Type::NULL_) {
        if (auto *body = dynamic_cast<BlockStmt *>(stmt.body.get());
            (!body->stmts.empty() && body->stmts.back()->type_tag() != NodeType::ReturnStmt) || body->stmts.empty()) {
            current_chunk->emit_instruction(Instruction::TRAP_RETURN, stmt.name.line);
        }
    }

    current_compiled->functions[stmt.name.lexeme] = std::move(function);
    current_chunk = &current_compiled->top_level_code;
}

StmtVisitorType Generator::visit(IfStmt &stmt) {
    compile(stmt.condition.get());
    if (stmt.condition->resolved.info->data.is_ref) {
        current_chunk->emit_instruction(Instruction::DEREF, stmt.condition->resolved.token.line);
    }
    std::size_t jump_idx = current_chunk->emit_instruction(Instruction::POP_JUMP_IF_FALSE, stmt.keyword.line);
    emit_three_bytes_of(0);
    // Reserve three bytes for the offset
    compile(stmt.thenBranch.get());

    std::size_t over_else = 0;
    if (stmt.elseBranch != nullptr) {
        over_else = current_chunk->emit_instruction(Instruction::JUMP_FORWARD, stmt.keyword.line);
        emit_three_bytes_of(0);
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

StmtVisitorType Generator::visit(ReturnStmt &stmt) {
    if (stmt.value != nullptr) {
        compile(stmt.value.get());
        if (auto &return_type = stmt.function->return_type;
            return_type->data.primitive == Type::LIST && !return_type->data.is_ref) {
            current_chunk->emit_instruction(Instruction::COPY_LIST, stmt.keyword.line);
        }
    } else {
        current_chunk->emit_instruction(Instruction::PUSH_NULL, stmt.keyword.line);
    }

    current_chunk->emit_instruction(Instruction::RETURN, stmt.keyword.line);
    emit_three_bytes_of(stmt.locals_popped);
}

StmtVisitorType Generator::visit(SwitchStmt &stmt) {
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
    if (stmt.condition->resolved.info->data.is_ref) {
        current_chunk->emit_instruction(Instruction::DEREF, stmt.condition->resolved.token.line);
    }
    std::vector<std::size_t> jumps{};
    std::size_t default_jump{};
    for (auto &case_ : stmt.cases) {
        compile(case_.first.get());
        jumps.push_back(
            current_chunk->emit_instruction(Instruction::POP_JUMP_IF_EQUAL, current_chunk->line_numbers.back().first));
        emit_three_bytes_of(0);
    }
    if (stmt.default_case != nullptr) {
        default_jump = current_chunk->emit_instruction(Instruction::JUMP_FORWARD, 0);
        emit_three_bytes_of(0);
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

StmtVisitorType Generator::visit(TypeStmt &stmt) {}

std::size_t Generator::recursively_compile_size(ListType *list) {
    auto compile_size = [this, &list] {
        if (list->size != nullptr) {
            compile(list->size.get());
            if (list->size->resolved.info->data.is_ref) {
                current_chunk->emit_instruction(Instruction::DEREF, list->size->resolved.token.line);
            }
        } else {
            current_chunk->emit_constant(Value{1}, 0);
        }
    };

    if (list->contained->data.primitive == Type::LIST) {
        std::size_t inner = recursively_compile_size(dynamic_cast<ListType *>(list->contained.get()));
        compile_size();
        return inner + 1;
    } else {
        compile(list);
        compile_size();
        return 1;
    }
}

StmtVisitorType Generator::visit(VarStmt &stmt) {
    if (stmt.type->data.primitive == Type::LIST && stmt.initializer == nullptr) {
        auto *list = dynamic_cast<ListType *>(stmt.type.get());
        if (list->size != nullptr) {
            compile(list->size.get());
            if (list->size->resolved.info->data.is_ref) {
                current_chunk->emit_instruction(Instruction::DEREF, list->size->resolved.token.line);
            }
        } else {
            current_chunk->emit_constant(Value{0}, stmt.name.line);
        }
        current_chunk->emit_instruction(Instruction::MAKE_LIST, stmt.name.line);
    } else if (stmt.initializer != nullptr) {
        if (stmt.type->data.is_ref && !stmt.initializer->resolved.info->data.is_ref &&
            stmt.initializer->type_tag() == NodeType::VariableExpr) {
            if (dynamic_cast<VariableExpr *>(stmt.initializer.get())->type == IdentifierType::LOCAL) {
                current_chunk->emit_instruction(Instruction::MAKE_REF_TO_LOCAL, stmt.name.line);
            } else {
                current_chunk->emit_instruction(Instruction::MAKE_REF_TO_GLOBAL, stmt.name.line);
            }
            emit_three_bytes_of(stmt.initializer->resolved.stack_slot);
        } else {
            compile(stmt.initializer.get());
            if (stmt.initializer->resolved.info->data.is_ref && !stmt.type->data.is_ref &&
                stmt.initializer->resolved.info->data.primitive != Type::LIST) {
                current_chunk->emit_instruction(Instruction::DEREF, stmt.name.line);
            }

            if (stmt.conversion_type != NumericConversionType::NONE) {
                emit_conversion(stmt.conversion_type, stmt.name.line);
            }
        }
        if (stmt.requires_copy) {
            current_chunk->emit_instruction(Instruction::COPY_LIST, stmt.name.line);
        }
    } else {
        current_chunk->emit_instruction(Instruction::PUSH_NULL, stmt.name.line);
    }
    scopes.top().push_back(stmt.type.get());
}

StmtVisitorType Generator::visit(WhileStmt &stmt) {
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
    emit_three_bytes_of(0);

    std::size_t loop_back_idx = current_chunk->bytes.size();
    compile(stmt.body.get());

    std::size_t increment_idx = current_chunk->bytes.size();
    if (stmt.increment != nullptr) {
        compile(stmt.increment.get());
    }

    std::size_t condition_idx = current_chunk->bytes.size();
    compile(stmt.condition.get());
    if (stmt.condition->resolved.info->data.is_ref) {
        current_chunk->emit_instruction(Instruction::DEREF, stmt.condition->resolved.token.line);
    }

    std::size_t jump_back_idx = current_chunk->emit_instruction(Instruction::POP_JUMP_BACK_IF_TRUE, stmt.keyword.line);
    emit_three_bytes_of(0);

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

BaseTypeVisitorType Generator::visit(PrimitiveType &type) {
    return {};
}

BaseTypeVisitorType Generator::visit(UserDefinedType &type) {
    return {};
}

BaseTypeVisitorType Generator::visit(ListType &type) {
    if (type.size != nullptr) {
        compile(type.size.get());
    } else {
        current_chunk->emit_constant(Value{0}, type.size->resolved.token.line);
    }
    current_chunk->emit_instruction(Instruction::MAKE_LIST, type.size->resolved.token.line);
    return {};
}

BaseTypeVisitorType Generator::visit(TypeofType &type) {
    return {};
}
