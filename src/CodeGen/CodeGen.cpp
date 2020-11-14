/* See LICENSE at project root for license details */
#include "CodeGen.hpp"

#include "../ErrorLogger/ErrorLogger.hpp"
#include "../VM/Chunk.hpp"

#define unreachable() __builtin_unreachable()

std::deque<RuntimeModule> Generator::compiled_modules{};

RuntimeModule Generator::compile(Module &main_module) {
    RuntimeModule compiled{};
    current_chunk = &compiled.top_level_code;
    current_module = &main_module;

    for (auto &stmt : main_module.statements) {
        if (stmt != nullptr) {
            compile(stmt.get());
        }
    }

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

ExprVisitorType Generator::visit(AccessExpr &expr) {
    return {};
}

ExprVisitorType Generator::visit(AssignExpr &expr) {
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
    return {};
}

ExprVisitorType Generator::visit(GetExpr &expr) {
    return {};
}

ExprVisitorType Generator::visit(GroupingExpr &expr) {
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
        case LiteralValue::BOOL: current_chunk->emit_constant(Value{expr.value.as.boolean}); break;
        case LiteralValue::NULL_: current_chunk->emit_constant(Value{expr.value.as.null}); break;
    }
    return {};
}

ExprVisitorType Generator::visit(LogicalExpr &expr) {
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
    return {};
}

ExprVisitorType Generator::visit(VariableExpr &expr) {
    return {};
}

StmtVisitorType Generator::visit(BlockStmt &stmt) {}

StmtVisitorType Generator::visit(BreakStmt &stmt) {}

StmtVisitorType Generator::visit(ClassStmt &stmt) {}

StmtVisitorType Generator::visit(ContinueStmt &stmt) {}

StmtVisitorType Generator::visit(ExpressionStmt &stmt) {
    compile(stmt.expr.get());
    current_chunk->emit_instruction(Instruction::POP);
}

StmtVisitorType Generator::visit(FunctionStmt &stmt) {}

StmtVisitorType Generator::visit(IfStmt &stmt) {}

StmtVisitorType Generator::visit(ReturnStmt &stmt) {}

StmtVisitorType Generator::visit(SwitchStmt &stmt) {}

StmtVisitorType Generator::visit(TypeStmt &stmt) {}

StmtVisitorType Generator::visit(VarStmt &stmt) {}

StmtVisitorType Generator::visit(WhileStmt &stmt) {}

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
