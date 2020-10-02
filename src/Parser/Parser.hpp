#pragma once

/* See LICENSE at project root for license details */

#ifndef PARSER_HPP
#  define PARSER_HPP

#include <array>
#include <functional>
#include <string_view>
#include <utility>
#include <vector>

#include "../AST.hpp"
#include "../ErrorLogger/ErrorLogger.hpp"
#include "../Token.hpp"

#define allocate(T, ...) new T{__VA_ARGS__}

struct ParsePrecedence {
    enum {
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
        SHIFT,   // >> <<
        SUM,        // + -
        PRODUCT,    // % / *
        UNARY,      // ! ~ ++ --
        CALL,       // . ()
        PRIMARY
    };
};

using precedence_t = decltype(ParsePrecedence::NONE);

template <typename T>
class Parser {
    using ExprPrefixParseFn = expr_node_t<T>(Parser::*)(bool can_assign);
    using ExprInfixParseFn = expr_node_t<T>(Parser::*)(bool can_assign, expr_node_t<T> left);

    struct ParseRule {
        ExprPrefixParseFn prefix{};
        ExprInfixParseFn infix{};
        precedence_t precedence{};
    };

    std::size_t current{};
    const std::vector<Token> &tokens{};
    ParseRule rules[static_cast<std::size_t>(TokenType::END_OF_FILE) + 1];

    void add_rule(TokenType type, ParseRule rule) noexcept;
    [[nodiscard]] constexpr const ParseRule &get_rule(TokenType type) const noexcept;

  public:
    explicit Parser(const std::vector<Token> &tokens);

    constexpr bool is_at_end() const noexcept;
    [[nodiscard]] const Token &previous() const noexcept;
    const Token &advance();
    [[nodiscard]] const Token &peek() const noexcept;
    [[nodiscard]] bool check(TokenType type) const noexcept;
    template <typename ... Args>
    bool match(Args ... args);
    template <typename ... Args>
    bool consume(const std::string_view message, Args ... args);

    expr_node_t<T> parse_precedence(precedence_t precedence);
    expr_node_t<T> expression();

    expr_node_t<T> number(bool);
    expr_node_t<T> binary(bool, expr_node_t<T> left);
    expr_node_t<T> grouping(bool);
};

template <typename T>
inline void Parser<T>::add_rule(TokenType type, ParseRule rule) noexcept {
    rules[static_cast<std::size_t>(type)] = rule;
}

template <typename T>
[[nodiscard]] inline constexpr const typename Parser<T>::ParseRule &
Parser<T>::get_rule(TokenType type) const noexcept {
    return rules[static_cast<std::size_t>(type)];
}

template <typename T>
inline Parser<T>::Parser(const std::vector<Token> &tokens): tokens{tokens} {
    add_rule(TokenType::COMMA,         {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::EQUAL,         {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::PLUS_EQUAL,    {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::MINUS_EQUAL,   {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::STAR_EQUAL,    {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::SLASH_EQUAL,   {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::QUESTION,      {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::COLON,         {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::LOGIC_OR,      {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::LOGIC_AND,     {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::BIT_OR,        {nullptr, &Parser<T>::binary, ParsePrecedence::BIT_OR});
    add_rule(TokenType::BIT_XOR,       {nullptr, &Parser<T>::binary, ParsePrecedence::BIT_XOR});
    add_rule(TokenType::BIT_AND,       {nullptr, &Parser<T>::binary, ParsePrecedence::BIT_AND});
    add_rule(TokenType::NOT_EQUAL,     {nullptr, &Parser<T>::binary, ParsePrecedence::EQUALITY});
    add_rule(TokenType::EQUAL_EQUAL,   {nullptr, &Parser<T>::binary, ParsePrecedence::EQUALITY});
    add_rule(TokenType::GREATER,       {nullptr, &Parser<T>::binary, ParsePrecedence::ORDERING});
    add_rule(TokenType::GREATER_EQUAL, {nullptr, &Parser<T>::binary, ParsePrecedence::ORDERING});
    add_rule(TokenType::LESS,          {nullptr, &Parser<T>::binary, ParsePrecedence::ORDERING});
    add_rule(TokenType::LESS_EQUAL,    {nullptr, &Parser<T>::binary, ParsePrecedence::ORDERING});
    add_rule(TokenType::RIGHT_SHIFT,   {nullptr, &Parser<T>::binary, ParsePrecedence::SHIFT});
    add_rule(TokenType::LEFT_SHIFT,    {nullptr, &Parser<T>::binary, ParsePrecedence::SHIFT});
    add_rule(TokenType::MINUS,         {nullptr, &Parser<T>::binary, ParsePrecedence::SUM});
    add_rule(TokenType::PLUS,          {nullptr, &Parser<T>::binary, ParsePrecedence::SUM});
    add_rule(TokenType::MODULO,        {nullptr, &Parser<T>::binary, ParsePrecedence::PRODUCT});
    add_rule(TokenType::SLASH,         {nullptr, &Parser<T>::binary, ParsePrecedence::PRODUCT});
    add_rule(TokenType::STAR,          {nullptr, &Parser<T>::binary, ParsePrecedence::PRODUCT});
    add_rule(TokenType::NOT,           {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::BIT_NOT,       {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::PLUS_PLUS,     {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::MINUS_MINUS,   {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::DOT,           {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::LEFT_PAREN,    {&Parser<T>::grouping, nullptr, ParsePrecedence::CALL});
    add_rule(TokenType::RIGHT_PAREN,   {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::LEFT_INDEX,    {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::RIGHT_INDEX,   {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::LEFT_BRACE,    {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::RIGHT_BRACE,   {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::SEMICOLON,     {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::ARROW,         {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::IDENTIFIER,    {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::STRING_VALUE,  {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::INT_VALUE,     {&Parser<T>::number, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::FLOAT_VALUE,   {&Parser<T>::number, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::AND,           {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::BREAK,         {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::CLASS,         {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::CONST,         {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::CONTINUE,      {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::ELSE,          {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::FALSE,         {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::FLOAT,         {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::FN,            {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::FOR,           {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::IF,            {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::IMPORT,        {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::INT,           {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::NULL_,         {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::OR,            {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::PROTECTED,     {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::PRIVATE,       {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::PUBLIC,        {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::REF,           {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::RETURN,        {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::STRING,        {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::TRUE,          {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::TYPE,          {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::TYPEOF,        {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::VAL,           {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::VAR,           {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::WHILE,         {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::NONE,          {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::END_OF_LINE,   {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::END_OF_FILE,   {nullptr, nullptr, ParsePrecedence::NONE});
}

template <typename T>
constexpr bool Parser<T>::is_at_end() const noexcept {
    return current >= tokens.size();
}

template <typename T>
[[nodiscard]] inline const Token &Parser<T>::previous() const noexcept {
    return tokens[current - 1];
}

template <typename T>
inline const Token &Parser<T>::advance() {
    if (is_at_end()) {
        error("Found unexpected EOF while parsing", previous().line);
    }

    current++;
    return tokens[current - 1];
}

template <typename T>
[[nodiscard]] inline const Token &Parser<T>::peek() const noexcept {
    return tokens[current];
}

template <typename T>
[[nodiscard]] inline bool Parser<T>::check(TokenType type) const noexcept {
    return peek().type == type;
}

template <typename T>
template <typename ... Args>
inline bool Parser<T>::match(Args ... args) {
    std::array arr{std::forward<Args>(args)...};

    for (auto &&arg : arr) {
        if (!check(arg)) {
            return false;
        }
    }

    advance();
    return true;
}

template <typename T>
template <typename ... Args>
inline bool Parser<T>::consume(const std::string_view message, Args ... args) {
    if (!match(args...)) {
        error(message, peek().line);
        return false;
    }

    return true;
}


template <typename T>
inline expr_node_t<T> Parser<T>::parse_precedence(precedence_t precedence) {
    using namespace std::string_literals;

    advance();

    ExprPrefixParseFn prefix = get_rule(previous().type).prefix;
    if (prefix == nullptr) {
        error("Could not find parser for "s + previous().lexeme, previous().line);
    }

    bool can_assign = precedence >= ParsePrecedence::ASSIGNMENT;
    expr_node_t<T> left = std::invoke(prefix, this, can_assign);

    while (precedence <= get_rule(peek().type).precedence) {
        ExprInfixParseFn infix = get_rule(advance().type).infix;
        left = std::invoke(infix, this, can_assign, std::move(left));
    }

    if (match(TokenType::EQUAL) && can_assign) {
        error("Invalid assignment target", previous().line);
    }

    return left;
}

template <typename T>
expr_node_t<T> Parser<T>::expression() {
    return parse_precedence(ParsePrecedence::ASSIGNMENT);
}

template <typename T>
expr_node_t<T> Parser<T>::number(bool) {
    if (previous().type == TokenType::INT_VALUE) {
        int value = std::stoi(previous().lexeme);
        return expr_node_t<T>{allocate(LiteralExpr<T>, value)};
    } else {
        double value = std::stod(previous().lexeme);
        return expr_node_t<T>{allocate(LiteralExpr<T>, value)};
    }
}

template <typename T>
expr_node_t<T> Parser<T>::binary(bool, expr_node_t<T> left) {
    expr_node_t<T> right = parse_precedence(get_rule(previous().type).precedence);
    return expr_node_t<T>{allocate(BinaryExpr<T>, std::move(left), previous(), std::move(right))};
}

template <typename T>
expr_node_t<T> Parser<T>::grouping(bool) {
    expr_node_t<T> expr = expression();
    consume("Expected ')' after parenthesized expression", TokenType::RIGHT_PAREN);
    return expr_node_t<T>{allocate(GroupingExpr<T>, std::move(expr))};
}

#endif