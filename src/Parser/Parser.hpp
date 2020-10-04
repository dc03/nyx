#pragma once

/* See LICENSE at project root for license details */

#ifndef PARSER_HPP
#  define PARSER_HPP

#include <algorithm>
#include <array>
#include <functional>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include "../AST.hpp"
#include "../ErrorLogger/ErrorLogger.hpp"
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
        : token{std::move(token)}, std::invalid_argument(std::string{error.begin(), error.end()}) {}
};

template <typename T>
class Parser {
    using ExprPrefixParseFn = expr_node_t<T>(Parser::*)(bool can_assign);
    using ExprInfixParseFn = expr_node_t<T>(Parser::*)(bool can_assign, expr_node_t<T> left);

    struct ParseRule {
        ExprPrefixParseFn prefix{};
        ExprInfixParseFn infix{};
        ParsePrecedence::of precedence{};
    };

    std::size_t current{};
    const std::vector<Token> &tokens{};
    ParseRule rules[static_cast<std::size_t>(TokenType::END_OF_FILE) + 1];

    void add_rule(TokenType type, ParseRule rule) noexcept;
    [[nodiscard]] constexpr const ParseRule &get_rule(TokenType type) const noexcept;

  public:
    explicit Parser(const std::vector<Token> &tokens);

    [[nodiscard]] constexpr bool is_at_end() const noexcept;
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

    // Expression parsing
    expr_node_t<T> parse_precedence(ParsePrecedence::of precedence);
    expr_node_t<T> expression();

    expr_node_t<T> literal(bool);
    expr_node_t<T> binary(bool, expr_node_t<T> left);
    expr_node_t<T> grouping(bool);
    expr_node_t<T> unary(bool);
    expr_node_t<T> ternary(bool, expr_node_t<T> left);
    expr_node_t<T> comma(bool, expr_node_t<T> left);
    expr_node_t<T> and_(bool, expr_node_t<T> left);
    expr_node_t<T> or_(bool, expr_node_t<T> left);
    expr_node_t<T> index(bool, expr_node_t<T> object);
    expr_node_t<T> variable(bool can_assign);
    expr_node_t<T> dot(bool can_assign, expr_node_t<T> left);
    expr_node_t<T> call(bool, expr_node_t<T> function);
    expr_node_t<T> super(bool);
    expr_node_t<T> this_expr(bool);

    type_node_t<T> type();
    type_node_t<T> list_type(bool is_const, bool is_ref);

    // Statement parsing
    stmt_node_t<T> statement();
    stmt_node_t<T> expression_statement();
    stmt_node_t<T> import_statement();
    stmt_node_t<T> variable_declaration();
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
    add_rule(TokenType::COMMA,         {nullptr, &Parser<T>::comma, ParsePrecedence::COMMA});
    add_rule(TokenType::EQUAL,         {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::PLUS_EQUAL,    {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::MINUS_EQUAL,   {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::STAR_EQUAL,    {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::SLASH_EQUAL,   {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::QUESTION,      {nullptr, &Parser<T>::ternary, ParsePrecedence::ASSIGNMENT});
    add_rule(TokenType::COLON,         {nullptr, nullptr, ParsePrecedence::NONE});
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
    add_rule(TokenType::MINUS,         {&Parser<T>::unary, &Parser<T>::binary, ParsePrecedence::SUM});
    add_rule(TokenType::PLUS,          {&Parser<T>::unary, &Parser<T>::binary, ParsePrecedence::SUM});
    add_rule(TokenType::MODULO,        {nullptr, &Parser<T>::binary, ParsePrecedence::PRODUCT});
    add_rule(TokenType::SLASH,         {nullptr, &Parser<T>::binary, ParsePrecedence::PRODUCT});
    add_rule(TokenType::STAR,          {nullptr, &Parser<T>::binary, ParsePrecedence::PRODUCT});
    add_rule(TokenType::NOT,           {&Parser<T>::unary, nullptr, ParsePrecedence::UNARY});
    add_rule(TokenType::BIT_NOT,       {&Parser<T>::unary, nullptr, ParsePrecedence::UNARY});
    add_rule(TokenType::PLUS_PLUS,     {&Parser<T>::unary, nullptr, ParsePrecedence::UNARY});
    add_rule(TokenType::MINUS_MINUS,   {&Parser<T>::unary, nullptr, ParsePrecedence::UNARY});
    add_rule(TokenType::DOT,           {nullptr, &Parser<T>::dot, ParsePrecedence::CALL});
    add_rule(TokenType::LEFT_PAREN,    {&Parser<T>::grouping, &Parser<T>::call, ParsePrecedence::CALL});
    add_rule(TokenType::RIGHT_PAREN,   {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::LEFT_INDEX,    {nullptr, &Parser<T>::index, ParsePrecedence::CALL});
    add_rule(TokenType::RIGHT_INDEX,   {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::LEFT_BRACE,    {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::RIGHT_BRACE,   {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::SEMICOLON,     {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::ARROW,         {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::IDENTIFIER,    {&Parser<T>::variable, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::STRING_VALUE,  {&Parser<T>::literal, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::INT_VALUE,     {&Parser<T>::literal, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::FLOAT_VALUE,   {&Parser<T>::literal, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::AND,           {nullptr, &Parser<T>::and_, ParsePrecedence::NONE});
    add_rule(TokenType::BREAK,         {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::CLASS,         {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::CONST,         {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::CONTINUE,      {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::ELSE,          {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::FALSE,         {&Parser<T>::literal, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::FLOAT,         {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::FN,            {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::FOR,           {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::IF,            {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::IMPORT,        {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::INT,           {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::NULL_,         {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::OR,            {nullptr, &Parser<T>::or_, ParsePrecedence::NONE});
    add_rule(TokenType::PROTECTED,     {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::PRIVATE,       {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::PUBLIC,        {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::REF,           {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::RETURN,        {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::STRING,        {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::SUPER,         {&Parser<T>::super, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::THIS,          {&Parser<T>::this_expr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::TRUE,          {&Parser<T>::literal, nullptr, ParsePrecedence::NONE});
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
[[nodiscard]] inline constexpr bool Parser<T>::is_at_end() const noexcept {
    return current >= tokens.size();
}

template <typename T>
[[nodiscard]] inline const Token &Parser<T>::previous() const noexcept {
    return tokens[current - 1];
}

template <typename T>
inline const Token &Parser<T>::advance() {
    if (is_at_end()) {
        error("Found unexpected EOF while parsing", previous());
        throw ParseException{previous(), "Found unexpected EOF while parsing"};
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

    if (std::any_of(arr.begin(), arr.end(), [this](auto &&arg) { return check(arg); })) {
        advance();

//        while (peek().type == TokenType::END_OF_LINE) {
//            advance();
//        }

        return true;
    } else {
        return false;
    }
}

template <typename T>
template <typename ... Args>
inline void Parser<T>::consume(const std::string_view message, Args ... args) {
    if (!match(args...)) {
        error(message, peek());
        throw ParseException{peek(), message};
    }
}

template <typename T>
template <typename ... Args>
inline void Parser<T>::consume(std::string_view message, const Token &where, Args ... args) {
    if (!match(args...)) {
        error(message, where);
        throw ParseException{where, message};
    }
}

template <typename T>
inline expr_node_t<T> Parser<T>::parse_precedence(ParsePrecedence::of precedence) {
    using namespace std::string_literals;

//    while (peek().type == TokenType::END_OF_LINE) {
//        advance();
//    }
    advance();

    ExprPrefixParseFn prefix = get_rule(previous().type).prefix;
    if (prefix == nullptr) {
        std::string message = "Could not find parser for '";
        message += [this]() {
            if (previous().type == TokenType::END_OF_LINE) {
                return "\\n' (newline)"s;
            } else {
                return previous().lexeme + "'";
            }
        }();
        error(message, previous());
        note("This may occur because of previous errors leading to the parser being confused");
        throw ParseException{previous(), message};
    }

    bool can_assign = precedence <= ParsePrecedence::ASSIGNMENT;
    expr_node_t<T> left = std::invoke(prefix, this, can_assign);

    while (precedence <= get_rule(peek().type).precedence) {
        ExprInfixParseFn infix = get_rule(advance().type).infix;
        left = std::invoke(infix, this, can_assign, std::move(left));
    }

    if (match(TokenType::EQUAL, TokenType::PLUS_EQUAL, TokenType::MINUS_EQUAL, TokenType::STAR_EQUAL,
              TokenType::SLASH_EQUAL) && can_assign) {
        error("Invalid assignment target", previous());
        throw ParseException{previous(), "Invalid assignment target"};
    }

    return left;
}

template <typename T>
inline expr_node_t<T> Parser<T>::expression() {
    return parse_precedence(ParsePrecedence::ASSIGNMENT);
}

template <typename T>
inline expr_node_t<T> Parser<T>::literal(bool) {
    switch (previous().type) {
        case TokenType::INT_VALUE: {
            int value = std::stoi(previous().lexeme);
            return expr_node_t<T>{allocate(LiteralExpr<T>, value)};
        }
        case TokenType::FLOAT_VALUE: {
            double value = std::stod(previous().lexeme);
            return expr_node_t<T>{allocate(LiteralExpr<T>, value)};
        }
        case TokenType::STRING_VALUE:
            return expr_node_t<T>{allocate(LiteralExpr<T>, previous().lexeme)};
        case TokenType::FALSE:
            return expr_node_t<T>{allocate(LiteralExpr<T>, false)};
        case TokenType::TRUE:
            return expr_node_t<T>{allocate(LiteralExpr<T>, true)};
        case TokenType::NULL_:
            return expr_node_t<T>{allocate(LiteralExpr<T>, nullptr)};
        default:
            throw ParseException{previous(), "Unexpected TokenType passed to literal parser"};
    }
}

template <typename T>
inline expr_node_t<T> Parser<T>::binary(bool, expr_node_t<T> left) {
    expr_node_t<T> right = parse_precedence(ParsePrecedence::of(get_rule(previous().type).precedence + 1));
    return expr_node_t<T>{allocate(BinaryExpr<T>, std::move(left), previous(), std::move(right))};
}

template <typename T>
inline expr_node_t<T> Parser<T>::grouping(bool) {
    expr_node_t<T> expr = expression();
    consume("Expected ')' after parenthesized expression", TokenType::RIGHT_PAREN);
    return expr_node_t<T>{allocate(GroupingExpr<T>, std::move(expr))};
}

template <typename T>
inline expr_node_t<T> Parser<T>::unary(bool) {
    expr_node_t<T> expr = parse_precedence(get_rule(previous().type).precedence);
    return expr_node_t<T>{allocate(UnaryExpr<T>, previous(), std::move(expr))};
}

template <typename T>
inline expr_node_t<T> Parser<T>::ternary(bool, expr_node_t<T> left) {
    Token question = previous();
    expr_node_t<T> middle = parse_precedence(ParsePrecedence::LOGIC_OR);
    consume("Expected colon in ternary expression", TokenType::COLON);
    expr_node_t<T> right = parse_precedence(ParsePrecedence::LOGIC_OR);
    return expr_node_t<T>{allocate(TernaryExpr<T>, std::move(left), question, std::move(middle), std::move(right))};
}

template <typename T>
inline expr_node_t<T> Parser<T>::comma(bool, expr_node_t<T> left) {
    std::vector<expr_node_t<T>> exprs{};
    exprs.emplace_back(std::move(left));
    do {
        exprs.emplace_back(parse_precedence(ParsePrecedence::ASSIGNMENT));
    } while (match(TokenType::COMMA));

    return expr_node_t<T>{allocate(CommaExpr<T>, std::move(exprs))};
}

template <typename T>
inline expr_node_t<T> Parser<T>::and_(bool, expr_node_t<T> left) {
    expr_node_t<T> right = parse_precedence(ParsePrecedence::LOGIC_AND);
    return expr_node_t<T>{allocate(LogicalExpr<T>, std::move(left), previous(), std::move(right))};
}

template <typename T>
inline expr_node_t<T> Parser<T>::or_(bool, expr_node_t<T> left) {
    expr_node_t<T> right = parse_precedence(ParsePrecedence::LOGIC_OR);
    return expr_node_t<T>{allocate(LogicalExpr<T>, std::move(left), previous(), std::move(right))};
}

template <typename T>
inline expr_node_t<T> Parser<T>::index(bool, expr_node_t<T> object) {
    expr_node_t<T> index = expression();
    consume("Expected ']' after array subscript index", TokenType::RIGHT_INDEX);
    return expr_node_t<T>{allocate(IndexExpr<T>, std::move(object), previous(), std::move(index))};
}

template <typename T>
inline expr_node_t<T> Parser<T>::variable(bool can_assign) {
    if (can_assign && match(TokenType::EQUAL, TokenType::PLUS_EQUAL, TokenType::MINUS_EQUAL, TokenType::STAR_EQUAL,
                            TokenType::SLASH_EQUAL)) {
        expr_node_t<T> value = expression();
        return expr_node_t<T>{allocate(AssignExpr<T>, previous(), std::move(value))};
    } else {
        return expr_node_t<T>{allocate(VariableExpr<T>, previous())};
    }
}

template <typename T>
inline expr_node_t<T> Parser<T>::dot(bool can_assign, expr_node_t<T> left) {
    consume("Expected identifier after '.'", TokenType::IDENTIFIER);
    Token name = previous();
    if (can_assign && match(TokenType::EQUAL, TokenType::PLUS_EQUAL, TokenType::MINUS_EQUAL, TokenType::STAR_EQUAL,
              TokenType::SLASH_EQUAL)) {
        expr_node_t<T> value = expression();
        return expr_node_t<T>{allocate(SetExpr<T>, std::move(left), std::move(name), std::move(value))};
    }  else {
        return expr_node_t<T>{allocate(GetExpr<T>, std::move(left), std::move(name))};
    }
}

template <typename T>
inline expr_node_t<T> Parser<T>::call(bool, expr_node_t<T> function) {
    Token paren = previous();
    std::vector<expr_node_t<T>> args{};
    if (!match(TokenType::RIGHT_PAREN)) {
        do {
            args.emplace_back(parse_precedence(ParsePrecedence::ASSIGNMENT));
        } while (match(TokenType::COMMA));

    }
    consume("Expected ')' after function call", TokenType::RIGHT_PAREN);
    return expr_node_t<T>{allocate(CallExpr<T>, std::move(function), std::move(paren), std::move(args))};
}

template <typename T>
inline expr_node_t<T> Parser<T>::super(bool) {
    Token super = previous();
    consume("Expected '.' after 'super' keyword", TokenType::DOT);
    consume("Expected name after '.' in super expression", TokenType::IDENTIFIER);
    Token name = previous();
    return expr_node_t<T>{allocate(SuperExpr<T>, std::move(super), std::move(name))};
}

template <typename T>
inline expr_node_t<T> Parser<T>::this_expr(bool) {
    Token keyword = previous();
    return expr_node_t<T>{allocate(ThisExpr<T>, std::move(keyword))};
}

////////////////////////////////////////////////////////////////////////////////

template <typename T>
inline type_node_t<T> Parser<T>::type() {
    bool is_const = match(TokenType::CONST);
    bool is_ref = match(TokenType::REF);
    TypeType type = [this]() {
        if (match(TokenType::BOOL)) {
            return TypeType::BOOL;
        } else if (match(TokenType::INT)) {
            return TypeType::INT;
        } else if (match(TokenType::FLOAT)) {
            return TypeType::FLOAT;
        } else if (match(TokenType::STRING)) {
            return TypeType::STRING;
        } else if (match(TokenType::CLASS)) {
            return TypeType::CLASS;
        } else if (match(TokenType::LEFT_INDEX)) {
            return TypeType::LIST;
        }
    }();

    if (type == TypeType::CLASS) {
        Token name = previous();
        return type_node_t<T>{allocate(UserDefinedType<T>, type, is_const, is_ref, std::move(name))};
    } else if (type == TypeType::LIST) {
        return list_type(is_const, is_ref);
    } else {
        return type_node_t<T>{allocate(PrimitiveType<T>, type, is_const, is_ref)};
    }
}

template <typename T>
inline type_node_t<T> Parser<T>::list_type(bool is_const, bool is_ref) {
    type_node_t<T> contained = type();
    consume("Expected ',' after contained type of array", TokenType::COMMA);
    expr_node_t<T> size = expression();
    consume("Expected ']' after array size", TokenType::RIGHT_INDEX);
    return type_node_t<T>{allocate(ListType<T>, TypeType::LIST, is_const, is_ref, std::move(contained),
                                   std::move(size))};
}

////////////////////////////////////////////////////////////////////////////////

template <typename T>
inline stmt_node_t<T> Parser<T>::statement() {
//    while (peek().type == TokenType::END_OF_LINE) {
//        advance();
//    }
    if (match(TokenType::IMPORT)) {
        return import_statement();
    } else if (match(TokenType::VAR, TokenType::VAL)) {
        return variable_declaration();
    }
    return expression_statement();
}

template <typename T>
inline stmt_node_t<T> Parser<T>::expression_statement() {
    expr_node_t<T> expr = expression();
    consume("Expected ';' or newline after expression", TokenType::SEMICOLON, TokenType::END_OF_LINE);
    return stmt_node_t<T>{allocate(ExpressionStmt<T>, std::move(expr))};
}

template <typename T>
inline stmt_node_t<T> Parser<T>::import_statement() {
    consume("Expected a name after 'import' keyword", TokenType::IDENTIFIER);
    Token name = previous();
    consume("Expected ';' or newline after imported file", previous(), TokenType::SEMICOLON, TokenType::END_OF_LINE);
    return stmt_node_t<T>{allocate(ImportStmt<T>, std::move(name))};
}

template <typename T>
inline stmt_node_t<T> Parser<T>::variable_declaration() {
    consume("Expected variable name after 'var'/'val' keyword", previous(), TokenType::IDENTIFIER);
    consume("Expected ':' after variable name", previous(), TokenType::COLON);
}

#endif