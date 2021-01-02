#pragma once

/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */

#ifndef PARSER_HPP
#define PARSER_HPP

#include "../AST.hpp"
#include "../Module.hpp"
#include "../Token.hpp"

#include <string_view>
#include <vector>

struct ParsePrecedence {
    enum class of {
        NONE,
        COMMA,      // ,
        ASSIGNMENT, // = += -= *= /= ?:
        LOGIC_OR,   // ||
        LOGIC_AND,  // &&
        BIT_OR,     // |
        BIT_XOR,    // ^
        BIT_AND,    // &
        EQUALITY,   // == !=
        ORDERING,   // > >= < <=
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

    std::size_t current{};
    const std::vector<Token> &tokens{};
    ParseRule rules[static_cast<std::size_t>(TokenType::END_OF_FILE) + 1];

    Module &current_module;
    std::size_t current_module_depth{}; // The depth in the import tree where the parser is currently at
    std::size_t scope_depth{};

    bool in_class{false};
    bool in_loop{false};
    bool in_function{false};
    bool in_switch{false};

    void add_rule(TokenType type, ParseRule rule) noexcept;
    [[nodiscard]] constexpr const ParseRule &get_rule(TokenType type) const noexcept;
    void synchronize();

    template <typename Allocated>
    StmtNode single_token_statement(std::string_view token, bool condition, std::string_view error_message);
    void throw_parse_error(const std::string_view message) const;

  public:
    static std::vector<std::pair<Module, std::size_t>> parsed_modules;

    explicit Parser(const std::vector<Token> &tokens, Module &module, std::size_t current_depth);

    [[nodiscard]] bool is_at_end() const noexcept;
    [[nodiscard]] const Token &previous() const noexcept;
    const Token &advance();
    [[nodiscard]] const Token &peek() const noexcept;
    [[nodiscard]] bool check(TokenType type) const noexcept;
    template <typename... Args>
    bool match(Args... args);
    template <typename... Args>
    void consume(std::string_view message, Args... args);
    template <typename... Args>
    void consume(std::string_view message, const Token &where, Args... args);

    std::vector<StmtNode> program();

    // Expression parsing
    ExprNode parse_precedence(ParsePrecedence::of precedence);
    ExprNode expression();

    ExprNode and_(bool, ExprNode left);
    ExprNode binary(bool, ExprNode left);
    ExprNode call(bool, ExprNode function);
    ExprNode comma(bool, ExprNode left);
    ExprNode dot(bool can_assign, ExprNode left);
    ExprNode grouping(bool);
    ExprNode index(bool, ExprNode object);
    ExprNode literal(bool);
    ExprNode or_(bool, ExprNode left);
    ExprNode scope_access(bool, ExprNode left);
    ExprNode super(bool);
    ExprNode ternary(bool, ExprNode left);
    ExprNode this_expr(bool);
    ExprNode unary(bool);
    ExprNode variable(bool can_assign);

    // Type parsing
    TypeNode type();
    TypeNode list_type(bool is_const, bool is_ref);

    // Statement parsing
    StmtNode declaration();
    StmtNode class_declaration();
    StmtNode function_declaration();
    StmtNode import_statement();
    StmtNode type_declaration();
    StmtNode variable_declaration();

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
};

#endif