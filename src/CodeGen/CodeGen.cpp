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
        current_chunk->emit_instruction(Instruction::POP, 0);
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
    current_chunk->emit_instruction(Instruction::ASSIGN_LOCAL, expr.target.line);
    current_chunk->emit_bytes((expr.stack_slot >> 16) & 0xff, (expr.stack_slot >> 8) & 0xff);
    current_chunk->emit_byte(expr.stack_slot & 0xff);
    return {};
}

ExprVisitorType Generator::visit(BinaryExpr &expr) {
    compile(expr.left.get());
    compile(expr.right.get());

    // clang-format off
    switch (expr.oper.type) {
        case TokenType::LEFT_SHIFT:    current_chunk->emit_instruction(Instruction::SHIFT_LEFT, expr.oper.line);  break;
        case TokenType::RIGHT_SHIFT:   current_chunk->emit_instruction(Instruction::SHIFT_RIGHT, expr.oper.line); break;
        case TokenType::BIT_AND:       current_chunk->emit_instruction(Instruction::BIT_AND, expr.oper.line);     break;
        case TokenType::BIT_OR:        current_chunk->emit_instruction(Instruction::BIT_OR, expr.oper.line);      break;
        case TokenType::BIT_XOR:       current_chunk->emit_instruction(Instruction::BIT_XOR, expr.oper.line);     break;
        case TokenType::MODULO:        current_chunk->emit_instruction(Instruction::MOD, expr.oper.line);         break;

        case TokenType::EQUAL_EQUAL:   current_chunk->emit_instruction(Instruction::EQUAL, expr.oper.line);       break;
        case TokenType::GREATER:       current_chunk->emit_instruction(Instruction::GREATER, expr.oper.line);     break;
        case TokenType::LESS:          current_chunk->emit_instruction(Instruction::LESSER, expr.oper.line);      break;

        case TokenType::NOT_EQUAL:
            current_chunk->emit_instruction(Instruction::EQUAL, expr.oper.line);
            current_chunk->emit_instruction(Instruction::NOT, expr.oper.line);
            break;
        case TokenType::GREATER_EQUAL:
            current_chunk->emit_instruction(Instruction::LESSER, expr.oper.line);
            current_chunk->emit_instruction(Instruction::NOT, expr.oper.line);
            break;
        case TokenType::LESS_EQUAL:
            current_chunk->emit_instruction(Instruction::GREATER, expr.oper.line);
            current_chunk->emit_instruction(Instruction::NOT, expr.oper.line);
            break;

        case TokenType::PLUS:
            switch (expr.resolved_type.info->data.type) {
                case Type::INT:
                case Type::FLOAT:
                    current_chunk->emit_instruction(Instruction::ADD, expr.oper.line); break;
                case Type::STRING: current_chunk->emit_instruction(Instruction::CONCAT, expr.oper.line); break;

                default:
                    unreachable();
            }
            break;

        case TokenType::MINUS: current_chunk->emit_instruction(Instruction::SUB, expr.oper.line); break;
        case TokenType::SLASH: current_chunk->emit_instruction(Instruction::DIV, expr.oper.line); break;
        case TokenType::STAR:  current_chunk->emit_instruction(Instruction::MUL, expr.oper.line); break;

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
        current_chunk->emit_instruction(Instruction::POP, 0);
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
        case LiteralValue::INT: current_chunk->emit_constant(Value{expr.value.as.integer}, expr.lexeme.line); break;
        case LiteralValue::DOUBLE: current_chunk->emit_constant(Value{expr.value.as.real}, expr.lexeme.line); break;
        case LiteralValue::STRING: current_chunk->emit_constant(Value{expr.value.as.string}, expr.lexeme.line); break;
        case LiteralValue::BOOL:
            if (expr.value.as.boolean) {
                current_chunk->emit_instruction(Instruction::TRUE, expr.lexeme.line);
            } else {
                current_chunk->emit_instruction(Instruction::FALSE, expr.lexeme.line);
            }
            break;
        case LiteralValue::NULL_: current_chunk->emit_instruction(Instruction::NULL_, expr.lexeme.line); break;
    }
    return {};
}

ExprVisitorType Generator::visit(LogicalExpr &expr) {
    compile(expr.left.get());
    if (expr.oper.type == TokenType::OR) {
        current_chunk->emit_instruction(Instruction::NOT, expr.oper.line);
    } // Since || / or short circuits on true, flip the boolean on top of the stack
    std::size_t jump_idx = current_chunk->emit_instruction(Instruction::JUMP_IF_FALSE, expr.oper.line);
    current_chunk->emit_bytes(0, 0);
    current_chunk->emit_byte(0);
    current_chunk->emit_instruction(Instruction::POP, expr.oper.line);
    compile(expr.right.get());
    std::size_t to_idx = current_chunk->bytes.size();
    patch_jump(jump_idx, to_idx - jump_idx - 4);
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
     * FALSE
     * POP_JUMP_IF_FALSE		| offset = +6   ----------------+
     * CONST_SHORT	        -> 0 | value = 1                |
     * JUMP_FORWARD		| offset = +2, jump to = 13   --+--+
     * CONST_SHORT	        -> 1 | value = 2    <-----------+  |
     * POP  <------------------------------------------------------+
     * HALT
     */
    compile(expr.left.get());

    std::size_t condition_jump_idx =
        current_chunk->emit_instruction(Instruction::POP_JUMP_IF_FALSE, expr.question.line);
    current_chunk->emit_bytes(0, 0);
    current_chunk->emit_byte(0);

    compile(expr.middle.get());

    std::size_t over_false_idx = current_chunk->emit_instruction(Instruction::JUMP_FORWARD, expr.question.line);
    current_chunk->emit_bytes(0, 0);
    current_chunk->emit_byte(0);
    std::size_t false_to_idx = current_chunk->bytes.size();

    compile(expr.right.get());

    std::size_t true_to_idx = current_chunk->bytes.size();

    patch_jump(condition_jump_idx, false_to_idx - condition_jump_idx - 4);
    patch_jump(over_false_idx, true_to_idx - over_false_idx - 4);
    return {};
}

ExprVisitorType Generator::visit(ThisExpr &expr) {
    return {};
}

ExprVisitorType Generator::visit(UnaryExpr &expr) {
    compile(expr.right.get());
    switch (expr.oper.type) {
        case TokenType::BIT_NOT: current_chunk->emit_instruction(Instruction::BIT_NOT, expr.oper.line); break;
        case TokenType::NOT: current_chunk->emit_instruction(Instruction::NOT, expr.oper.line); break;
        case TokenType::MINUS: current_chunk->emit_instruction(Instruction::NEGATE, expr.oper.line); break;
        default: error("Bug in parser with illegal type for unary expression", expr.oper); break;
    }
    return {};
}

ExprVisitorType Generator::visit(VariableExpr &expr) {
    if (expr.stack_slot < Chunk::const_short_max) {
        current_chunk->emit_instruction(Instruction::ACCESS_LOCAL_SHORT, expr.name.line);
        current_chunk->emit_integer(expr.stack_slot);
    } else if (expr.stack_slot < Chunk::const_long_max) {
        current_chunk->emit_instruction(Instruction::ACCESS_LOCAL_LONG, expr.name.line);
        current_chunk->emit_integer(expr.stack_slot);
    } else {
        compile_error("Too many variables in current scope");
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
    std::size_t break_idx = current_chunk->emit_instruction(Instruction::JUMP_FORWARD, stmt.keyword.line);
    current_chunk->emit_bytes(0, 0);
    current_chunk->emit_byte(0);
    break_stmts.top().push_back(break_idx);
}

StmtVisitorType Generator::visit(ClassStmt &stmt) {}

StmtVisitorType Generator::visit(ContinueStmt &stmt) {
    std::size_t continue_idx = current_chunk->emit_instruction(Instruction::JUMP_FORWARD, stmt.keyword.line);
    current_chunk->emit_bytes(0, 0);
    current_chunk->emit_byte(0);
    continue_stmts.top().push_back(continue_idx);
}

StmtVisitorType Generator::visit(ExpressionStmt &stmt) {
    compile(stmt.expr.get());
    current_chunk->emit_instruction(Instruction::POP, 0);
}

StmtVisitorType Generator::visit(FunctionStmt &stmt) {}

StmtVisitorType Generator::visit(IfStmt &stmt) {
    compile(stmt.condition.get());
    std::size_t jump_idx = current_chunk->emit_instruction(Instruction::POP_JUMP_IF_FALSE, stmt.keyword.line);
    current_chunk->emit_bytes(0, 0);
    current_chunk->emit_byte(0);
    // Reserve three bytes for the offset
    compile(stmt.thenBranch.get());

    std::size_t over_else = current_chunk->emit_instruction(Instruction::JUMP_FORWARD, stmt.keyword.line);
    current_chunk->emit_bytes(0, 0);
    current_chunk->emit_byte(0);

    std::size_t before_else = current_chunk->bytes.size();
    patch_jump(jump_idx, before_else - jump_idx - 4);
    /*
     * The -4 because:
     *
     * POP_JUMP_IF_FALSE
     * BYTE1 -+
     * BYTE2  |-> The three bytes for the offset from the POP_JUMP_IF_FALSE instruction to the else statement
     * BYTE3 -+
     * ... <- This is the body of the if statement (The ip will be here when the jump happens, but `jump_idx` will be
     *                                              considered for POP_JUMP_IF_FALSE)
     * JUMP_FORWARD
     * BYTE1
     * BYTE2
     * BYTE3 <- This will be where `before_else` is considered
     * ... (The else statement, if it exists)
     * BYTE <- This is where the JUMP_FORWARD will jump to
     */
    if (stmt.elseBranch != nullptr) {
        compile(stmt.elseBranch.get());
    }

    std::size_t after_else = current_chunk->bytes.size();
    patch_jump(over_else, after_else - over_else - 4);
}

StmtVisitorType Generator::visit(ReturnStmt &stmt) {}

StmtVisitorType Generator::visit(SwitchStmt &stmt) {}

StmtVisitorType Generator::visit(TypeStmt &stmt) {}

StmtVisitorType Generator::visit(VarStmt &stmt) {
    if (stmt.initializer != nullptr) {
        compile(stmt.initializer.get());
    } else {
        current_chunk->emit_instruction(Instruction::NULL_, stmt.name.line);
    }
    scopes.top() += 1;
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
     *   CONST_SHORT               -> 0 | value = 0
     *   JUMP_FORWARD              | offset = +15  ---+
     *   ACCESS_LOCAL_SHORT        -> 0  <------------+---+  ) - These two instructions are the body of the loop
     *   POP                                          |   |  )
     *   ACCESS_LOCAL_SHORT        -> 0               |   |  } - These five instructions are the increment
     *   CONST_SHORT               -> 1 | value = 1   |   |  }
     *   ADD                                          |   |  }
     *   ASSIGN_LOCAL              | assign to 0      |   |  }
     *   POP                                          |   |  }
     *   ACCESS_LOCAL_SHORT        -> 0   <-----------+   |  ] - These three instructions are the condition
     *   CONST_SHORT               -> 2 | value = 5       |  ]
     *   LESSER                                           |  ]
     *   POP_JUMP_BACK_IF_TRUE         | offset = -24  -------+
     *   POP
     *   HALT
     *
     *   From this, the control flow should be obvious. I have tried to mirror
     *   what gcc generates for a loop in C.
     */
    break_stmts.emplace();
    continue_stmts.emplace();

    std::size_t jump_begin_idx = current_chunk->emit_instruction(Instruction::JUMP_FORWARD, stmt.keyword.line);
    current_chunk->emit_bytes(0, 0);
    current_chunk->emit_byte(0);

    std::size_t loop_back_idx = current_chunk->bytes.size();
    compile(stmt.body.get());

    std::size_t increment_idx = current_chunk->bytes.size();
    if (stmt.increment != nullptr) {
        compile(stmt.increment.get());
    }

    std::size_t condition_idx = current_chunk->bytes.size();
    compile(stmt.condition.get());

    std::size_t jump_back_idx = current_chunk->emit_instruction(Instruction::POP_JUMP_BACK_IF_TRUE, stmt.keyword.line);
    current_chunk->emit_bytes(0, 0);
    current_chunk->emit_byte(0);

    std::size_t loop_end_idx = current_chunk->bytes.size();

    patch_jump(jump_back_idx, jump_back_idx - loop_back_idx + 4);
    patch_jump(jump_begin_idx, condition_idx - jump_begin_idx - 4);

    for (std::size_t continue_idx : continue_stmts.top()) {
        patch_jump(continue_idx, increment_idx - continue_idx - 4);
    }

    for (std::size_t break_idx : break_stmts.top()) {
        patch_jump(break_idx, loop_end_idx - break_idx - 3);
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
    return {};
}

BaseTypeVisitorType Generator::visit(TypeofType &type) {
    return {};
}
