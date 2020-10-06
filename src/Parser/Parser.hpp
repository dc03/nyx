#pragma once

/* See LICENSE at project root for license details */

#ifndef PARSER_HPP
#  define PARSER_HPP

#include <string_view>
#include <vector>

#include "../AST.hpp"
#include "../Token.hpp"

#define allocate(T, ...) new T{__VA_ARGS__}

struct ParsePrecedence {
    enum of {
        NONE,
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
        COMMA,      // ,
        /* PRIMARY */
    };
};

struct ParseException : public std::invalid_argument {
    Token token{};
    explicit ParseException(Token token, const std::string_view error)
            : std::invalid_argument(std::string{error.begin(), error.end()}), token{std::move(token)} {}
};

class Parser {
    using ExprPrefixParseFn = expr_node_t(Parser::*)(bool can_assign);
    using ExprInfixParseFn = expr_node_t(Parser::*)(bool can_assign, expr_node_t left);

    struct ParseRule {
        ExprPrefixParseFn prefix{};
        ExprInfixParseFn infix{};
        ParsePrecedence::of precedence{};
    };

    std::size_t current{};
    const std::vector<Token> &tokens{};
    ParseRule rules[static_cast<std::size_t>(TokenType::END_OF_FILE) + 1];

    std::vector<ClassStmt*> &classes;
    std::vector<FunctionStmt*> &functions;
    std::vector<VarStmt*> &globals;
    std::size_t scope_depth{};

    bool in_class{false};
    bool in_loop{false};
    bool in_function{false};
    bool in_switch{false};

    void add_rule(TokenType type, ParseRule rule) noexcept;
    [[nodiscard]] constexpr const ParseRule &get_rule(TokenType type) const noexcept;
    void synchronize();

    template <typename Allocated>
    stmt_node_t single_token_statement(std::string_view token, bool condition, std::string_view error_message);
    void sync_and_throw(std::string_view message);

public:
    explicit Parser(const std::vector<Token> &tokens, std::vector<ClassStmt*> &classes,
                    std::vector<FunctionStmt*> &functions, std::vector<VarStmt*> &globals);

    [[nodiscard]] bool is_at_end() const noexcept;
    [[nodiscard]] const Token &previous() const noexcept;
    const Token &advance();
    [[nodiscard]] const Token &peek() const noexcept;
    [[nodiscard]] bool check(TokenType type) const noexcept;
    template <typename ... Args>
    bool match(Args ... args);
    template <typename ... Args>
    void consume(std::string_view message, Args ... args);
    template <typename ... Args>
    void consume(std::string_view message, const Token &where, Args ... args);

    std::vector<stmt_node_t> program();

    // Expression parsing
    expr_node_t parse_precedence(ParsePrecedence::of precedence);
    expr_node_t expression();

    expr_node_t grouping(bool);
    expr_node_t literal(bool);
    expr_node_t super(bool);
    expr_node_t this_expr(bool);
    expr_node_t unary(bool);
    expr_node_t variable(bool can_assign);

    expr_node_t and_(bool, expr_node_t left);
    expr_node_t binary(bool, expr_node_t left);
    expr_node_t call(bool, expr_node_t function);
    expr_node_t comma(bool, expr_node_t left);
    expr_node_t dot(bool can_assign, expr_node_t left);
    expr_node_t index(bool, expr_node_t object);
    expr_node_t or_(bool, expr_node_t left);
    expr_node_t ternary(bool, expr_node_t left);

    // Type parsing
    type_node_t type();
    type_node_t list_type(bool is_const, bool is_ref);

    // Statement parsing
    stmt_node_t declaration();
    stmt_node_t class_declaration();
    stmt_node_t function_declaration();
    stmt_node_t import_statement();
    stmt_node_t type_declaration();
    stmt_node_t variable_declaration();

    stmt_node_t statement();
    stmt_node_t block_statement();
    stmt_node_t break_statement();
    stmt_node_t continue_statement();
    stmt_node_t expression_statement();
    stmt_node_t for_statement();
    stmt_node_t if_statement();
    stmt_node_t return_statement();
    stmt_node_t switch_statement();
    stmt_node_t while_statement();
};

#endif