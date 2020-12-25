/* Copyright (C) 2020  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "CodeGen.hpp"

#include "../Common.hpp"
#include "../ErrorLogger/ErrorLogger.hpp"
#include "../VM/Chunk.hpp"

std::vector<RuntimeModule> Generator::compiled_modules{};

void Generator::begin_scope() {
    scopes.push(0);
}

void Generator::end_scope() {
    for (std::size_t begin = scopes.top(); begin > 0; begin--) {
        current_chunk->emit_instruction(Instruction::POP);
    }
    scopes.pop();
}

void Generator::patch_jump(std::size_t jump_idx, std::size_t jump_amount) {
    if (jump_amount >= Chunk::const_long_max) {
        compile_error("Size of jump is greater than that allowed by the instruction set");
        return;
    }

    current_chunk->bytes[jump_idx + 1] = (jump_amount >> 16) & 0xff;
    current_chunk->bytes[jump_idx + 2] = (jump_amount >> 8) & 0xff;
    current_chunk->bytes[jump_idx + 3] = jump_amount & 0xff;
}

RuntimeModule Generator::compile(Module &module) {
    begin_scope();
    RuntimeModule compiled{};
    current_chunk = &compiled.top_level_code;
    current_module = &module;

    for (auto &stmt : module.statements) {
        if (stmt != nullptr) {
            compile(stmt.get());
        }
    }

    end_scope();
    return compiled;
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
    compile(expr.value.get());
    current_chunk->emit_instruction(Instruction::ASSIGN_LOCAL);
    current_chunk->emit_bytes((expr.stack_slot >> 16) & 0xff, (expr.stack_slot >> 8) & 0xff);
    current_chunk->emit_byte(expr.stack_slot & 0xff);
    return {};
}

ExprVisitorType Generator::visit(BinaryExpr &expr) {
    compile(expr.left.get());
    compile(expr.right.get());

    // clang-format off
    switch (expr.oper.type) {
        case TokenType::LEFT_SHIFT:    current_chunk->emit_instruction(Instruction::SHIFT_LEFT);  break;
        case TokenType::RIGHT_SHIFT:   current_chunk->emit_instruction(Instruction::SHIFT_RIGHT); break;
        case TokenType::BIT_AND:       current_chunk->emit_instruction(Instruction::BIT_AND);     break;
        case TokenType::BIT_OR:        current_chunk->emit_instruction(Instruction::BIT_OR);      break;
        case TokenType::BIT_XOR:       current_chunk->emit_instruction(Instruction::BIT_XOR);     break;
        case TokenType::MODULO:        current_chunk->emit_instruction(Instruction::MOD);         break;

        case TokenType::EQUAL_EQUAL:   current_chunk->emit_instruction(Instruction::EQUAL);       break;
        case TokenType::GREATER:       current_chunk->emit_instruction(Instruction::GREATER);     break;
        case TokenType::LESS:          current_chunk->emit_instruction(Instruction::LESSER);      break;

        case TokenType::NOT_EQUAL:
            current_chunk->emit_instruction(Instruction::EQUAL);
            current_chunk->emit_instruction(Instruction::NOT);
            break;
        case TokenType::GREATER_EQUAL:
            current_chunk->emit_instruction(Instruction::LESSER);
            current_chunk->emit_instruction(Instruction::NOT);
            break;
        case TokenType::LESS_EQUAL:
            current_chunk->emit_instruction(Instruction::GREATER);
            current_chunk->emit_instruction(Instruction::NOT);
            break;

        case TokenType::PLUS:
            switch (expr.resolved_type.info->data.type) {
                case Type::INT:
                case Type::FLOAT:
                    current_chunk->emit_instruction(Instruction::ADD); break;
                case Type::STRING: current_chunk->emit_instruction(Instruction::CONCAT); break;

                default:
                    unreachable();
            }
            break;

        case TokenType::MINUS: current_chunk->emit_instruction(Instruction::SUB); break;
        case TokenType::SLASH: current_chunk->emit_instruction(Instruction::DIV); break;
        case TokenType::STAR:  current_chunk->emit_instruction(Instruction::MUL); break;

        default:
            error("Bug in parser with illegal token type of expression's operator", expr.oper);
            break;
    }
    // clang-format on

    return {};
}

ExprVisitorType Generator::visit(CallExpr &expr) {
    return {};
}

ExprVisitorType Generator::visit(CommaExpr &expr) {
    auto it = begin(expr.exprs);

    for (auto next = std::next(it); next != end(expr.exprs); it = next, ++next) {
        compile(it->get());
        current_chunk->emit_instruction(Instruction::POP);
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
    return {};
}

ExprVisitorType Generator::visit(LiteralExpr &expr) {
    switch (expr.value.tag) {
        case LiteralValue::INT: current_chunk->emit_constant(Value{expr.value.as.integer}); break;
        case LiteralValue::DOUBLE: current_chunk->emit_constant(Value{expr.value.as.real}); break;
        case LiteralValue::STRING: current_chunk->emit_constant(Value{expr.value.as.string}); break;
        case LiteralValue::BOOL:
            if (expr.value.as.boolean) {
                current_chunk->emit_instruction(Instruction::TRUE);
            } else {
                current_chunk->emit_instruction(Instruction::FALSE);
            }
            break;
        case LiteralValue::NULL_: current_chunk->emit_instruction(Instruction::NULL_); break;
    }
    return {};
}

ExprVisitorType Generator::visit(LogicalExpr &expr) {
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
    return {};
}

ExprVisitorType Generator::visit(ThisExpr &expr) {
    return {};
}

ExprVisitorType Generator::visit(UnaryExpr &expr) {
    compile(expr.right.get());
    switch (expr.oper.type) {
        case TokenType::BIT_NOT: current_chunk->emit_instruction(Instruction::BIT_NOT); break;
        case TokenType::NOT: current_chunk->emit_instruction(Instruction::NOT); break;
        case TokenType::MINUS: current_chunk->emit_instruction(Instruction::NEGATE); break;
        default: error("Bug in parser with illegal type for unary expression", expr.oper); break;
    }
    return {};
}

ExprVisitorType Generator::visit(VariableExpr &expr) {
    if (expr.stack_slot < Chunk::const_short_max) {
        current_chunk->emit_instruction(Instruction::ACCESS_LOCAL_SHORT);
        current_chunk->emit_byte(expr.stack_slot & 0xff);
    } else if (current_chunk->constants.size() < Chunk::const_long_max) {
        current_chunk->emit_instruction(Instruction::ACCESS_LOCAL_LONG);
        std::size_t constant = expr.stack_slot;
        current_chunk->emit_bytes((constant >> 16) & 0xff, (constant >> 8) & 0xff);
        current_chunk->emit_byte(constant & 0xff);
    }
    return {};
}

StmtVisitorType Generator::visit(BlockStmt &stmt) {
    begin_scope();
    for (auto &statement : stmt.stmts) {
        compile(statement.get());
    }
    end_scope();
}

StmtVisitorType Generator::visit(BreakStmt &stmt) {
    std::size_t break_idx = current_chunk->emit_instruction(Instruction::JUMP_FORWARD);
    current_chunk->emit_bytes(0, 0);
    current_chunk->emit_byte(0);
    break_stmts.top().push_back(break_idx);
}

StmtVisitorType Generator::visit(ClassStmt &stmt) {}

StmtVisitorType Generator::visit(ContinueStmt &stmt) {}

StmtVisitorType Generator::visit(ExpressionStmt &stmt) {
    compile(stmt.expr.get());
    current_chunk->emit_instruction(Instruction::POP);
}

StmtVisitorType Generator::visit(FunctionStmt &stmt) {}

StmtVisitorType Generator::visit(IfStmt &stmt) {
    compile(stmt.condition.get());
    std::size_t jump_idx = current_chunk->emit_instruction(Instruction::JUMP_IF_FALSE);
    current_chunk->emit_bytes(0, 0);
    current_chunk->emit_byte(0);
    // Reserve three bytes for the offset
    current_chunk->emit_instruction(Instruction::POP);
    compile(stmt.thenBranch.get());
    std::size_t before_else = current_chunk->emit_instruction(Instruction::POP);
    patch_jump(jump_idx, before_else - jump_idx - 4);
    /*
     * The -4 because:
     * JUMP_IF_FALSE
     * BYTE1 -+
     * BYTE2  |-> The three bytes for the offset from the JUMP_IF_FALSE instruction to the second POP
     * BYTE3 -+
     * POP <- The ip will be here when the jump happens, but `jump_idx` will be considered for JUMP_IF_FALSE
     * ... <- This is the body of the if statement
     * POP <- This will be where `before_else` is considered
     */
    if (stmt.elseBranch != nullptr) {
        compile(stmt.elseBranch.get());
    }
}

StmtVisitorType Generator::visit(ReturnStmt &stmt) {}

StmtVisitorType Generator::visit(SwitchStmt &stmt) {}

StmtVisitorType Generator::visit(TypeStmt &stmt) {}

StmtVisitorType Generator::visit(VarStmt &stmt) {
    if (stmt.initializer != nullptr) {
        compile(stmt.initializer.get());
    } else {
        current_chunk->emit_instruction(Instruction::NULL_);
    }
    scopes.top() += 1;
}

StmtVisitorType Generator::visit(WhileStmt &stmt) {
    break_stmts.emplace();
    std::size_t loop_start = current_chunk->bytes.size();
    compile(stmt.condition.get());

    std::size_t exit_jump_idx = current_chunk->emit_instruction(Instruction::JUMP_IF_FALSE);
    current_chunk->emit_bytes(0, 0);
    current_chunk->emit_byte(0);
    current_chunk->emit_instruction(Instruction::POP);

    compile(stmt.body.get());

    std::size_t loop_back_idx = current_chunk->emit_instruction(Instruction::JUMP_BACKWARD);
    current_chunk->emit_bytes(0, 0);
    current_chunk->emit_byte(0);

    std::size_t loop_end = current_chunk->emit_instruction(Instruction::POP);

    patch_jump(exit_jump_idx, loop_end - exit_jump_idx - 4);
    patch_jump(loop_back_idx, loop_back_idx - loop_start + 4);
    // In this case it will be +4 because 3 additional bytes, i.e the offset have to be jumped back over

    for (std::size_t break_idx : break_stmts.top()) {
        patch_jump(break_idx, loop_end - break_idx - 3); // -4 as described before, +1 to jump after the POP instruction
    }
    break_stmts.pop();
}

BaseTypeVisitorType Generator::visit(PrimitiveType &type) {
    return {};
}

BaseTypeVisitorType Generator::visit(UserDefinedType &type) {
    return {};
}

BaseTypeVisitorType Generator::visit(ListType &type) {
    return {};
}

BaseTypeVisitorType Generator::visit(TypeofType &type) {
    return {};
}
