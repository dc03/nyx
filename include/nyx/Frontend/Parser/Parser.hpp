#pragma once

/* Copyright (C) 2020-2022  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef PARSER_HPP
#define PARSER_HPP

#include "ScopedManager.hpp"
#include "nyx/AST/AST.hpp"
#include "nyx/AST/Token.hpp"
#include "nyx/Frontend/FrontendContext.hpp"
#include "nyx/Frontend/Module.hpp"
#include "nyx/Frontend/Scanner/Scanner.hpp"

#include <string_view>
#include <vector>

struct ParsePrecedence {
    enum class of {
        NONE,
        COMMA,      // ,
        ASSIGNMENT, // = += -= *= /= ?:
        TERNARY,    // ?:
        LOGIC_OR,   // ||
        LOGIC_AND,  // &&
        BIT_OR,     // |
        BIT_XOR,    // ^
        BIT_AND,    // &
        EQUALITY,   // == !=
        ORDERING,   // > >= < <=
        RANGE,      // .. ..=
        SHIFT,      // >> <<
        SUM,        // + -
        PRODUCT,    // % / *
        UNARY,      // ! ~ ++ --
        CALL,       // . () []
        PRIMARY
    };
};

class Parser {
    using ExprPrefixParseFn = ExprNode (Parser::*)(bool can_assign);
    using ExprInfixParseFn = ExprNode (Parser::*)(bool can_assign, ExprNode left);

    struct ParseRule {
        ExprPrefixParseFn prefix{};
        ExprInfixParseFn infix{};
        ParsePrecedence::of precedence{};
    };

    FrontendContext *ctx{};

    Scanner *scanner{};
    Token current_token{};
    Token next_token{};

    ParseRule rules[static_cast<std::size_t>(TokenType::END_OF_FILE) + 1];
    void setup_rules() noexcept;

    Module *current_module{};
    std::size_t current_module_depth{}; // The depth in the import tree where the parser is currently at
    std::size_t scope_depth{};

    bool in_class{false};
    bool in_loop{false};
    bool in_function{false};
    bool in_switch{false};

    std::vector<ClassStmt::MethodType> *current_methods{};

    void add_rule(TokenType type, ParseRule rule) noexcept;
    [[nodiscard]] constexpr const ParseRule &get_rule(TokenType type) const noexcept;
    void synchronize();

    void throw_parse_error(const std::string_view message) const;
    void throw_parse_error(const std::string_view message, const Token &where) const;

    [[nodiscard]] bool is_at_end() const noexcept;

    const Token &advance();
    [[nodiscard]] const Token &peek() const noexcept;

    [[nodiscard]] bool check(TokenType type) const noexcept;
    template <typename... Args>
    bool match(Args... args);
    template <typename... Args>
    void consume(std::string_view message, Args... args);
    template <typename... Args>
    void consume(std::string_view message, const Token &where, Args... args);

    template <typename Allocated>
    StmtNode single_token_statement(std::string_view token, bool condition, std::string_view error_message);
    IdentifierTuple ident_tuple();

    void recursively_change_module_depth(std::pair<Module, std::size_t> &module, std::size_t value);

    void warning(const std::vector<std::string> &message, const Token &where) const noexcept;
    void error(const std::vector<std::string> &message, const Token &where) const noexcept;
    void note(const std::vector<std::string> &message) const noexcept;

    enum OptimizationFlag { DEFAULT_OFF, DEFAULT_ON };

    [[nodiscard]] bool has_optimization_flag(const std::string &flag, OptimizationFlag default_) const noexcept;
    ExprNode compute_literal_binary_expr(LiteralExpr &left, const Token &oper, LiteralExpr &right);
    ExprNode compute_literal_unary_expr(LiteralExpr &value, const Token &oper);
    ExprNode compute_literal_ternary_expr(
        LiteralExpr &cond, LiteralExpr &middle, LiteralExpr &right, const Token &oper);
    ExprNode compute_literal_logical_expr(LiteralExpr &left, LiteralExpr &right, const Token &oper);

  public:
    Parser() noexcept = default;
    Parser(FrontendContext *ctx, Scanner *scanner, Module *module, std::size_t current_depth);

    std::vector<StmtNode> program();

    // Expression parsing
    ExprNode parse_precedence(ParsePrecedence::of precedence);
    ExprNode expression();
    ExprNode assignment();

    ExprNode and_(bool, ExprNode left);
    ExprNode binary(bool, ExprNode left);
    ExprNode call(bool, ExprNode function);
    ExprNode comma(bool, ExprNode left);
    ExprNode dot(bool can_assign, ExprNode left);
    ExprNode grouping(bool);
    ExprNode index(bool, ExprNode object);
    ExprNode list(bool);
    ExprNode literal(bool);
    ExprNode move(bool);
    ExprNode or_(bool, ExprNode left);
    ExprNode scope_access(bool, ExprNode left);
    ExprNode super(bool);
    ExprNode ternary(bool, ExprNode left);
    ExprNode this_expr(bool);
    ExprNode tuple(bool);
    ExprNode unary(bool);
    ExprNode variable(bool can_assign);

    // Statement parsing
    StmtNode declaration();
    StmtNode class_declaration();
    StmtNode function_declaration();
    StmtNode import_statement();
    StmtNode type_declaration();
    StmtNode variable_declaration();
    StmtNode vartuple_declaration();

    StmtNode statement();
    StmtNode block_statement();
    StmtNode break_statement();
    StmtNode continue_statement();
    StmtNode expression_statement();
    StmtNode for_statement();
    StmtNode if_statement();
    StmtNode return_statement();
    StmtNode switch_statement();
    StmtNode while_statement();

    // Type parsing
    TypeNode type();
    TypeNode list_type(bool is_const, bool is_ref);
    TypeNode tuple_type(bool is_const, bool is_ref);
};

#endif