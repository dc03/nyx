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
        : std::invalid_argument(std::string{error.begin(), error.end()}), token{std::move(token)} {}
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

    bool in_class{false};
    bool in_loop{false};
    bool in_function{false};
    bool in_switch{false};

    void add_rule(TokenType type, ParseRule rule) noexcept;
    [[nodiscard]] constexpr const ParseRule &get_rule(TokenType type) const noexcept;
    void synchronize();

    template <template <typename> typename Allocated>
    stmt_node_t<T> single_token_statement(std::string_view token, bool condition, std::string_view error_message);
    void sync_and_throw(std::string_view message);

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

    expr_node_t<T> grouping(bool);
    expr_node_t<T> literal(bool);
    expr_node_t<T> super(bool);
    expr_node_t<T> this_expr(bool);
    expr_node_t<T> unary(bool);
    expr_node_t<T> variable(bool can_assign);

    expr_node_t<T> and_(bool, expr_node_t<T> left);
    expr_node_t<T> binary(bool, expr_node_t<T> left);
    expr_node_t<T> call(bool, expr_node_t<T> function);
    expr_node_t<T> comma(bool, expr_node_t<T> left);
    expr_node_t<T> dot(bool can_assign, expr_node_t<T> left);
    expr_node_t<T> index(bool, expr_node_t<T> object);
    expr_node_t<T> or_(bool, expr_node_t<T> left);
    expr_node_t<T> ternary(bool, expr_node_t<T> left);

    // Type parsing
    type_node_t<T> type();
    type_node_t<T> list_type(bool is_const, bool is_ref);

    // Statement parsing
    stmt_node_t<T> declaration();
    stmt_node_t<T> class_declaration();
    stmt_node_t<T> function_declaration();
    stmt_node_t<T> import_statement();
    stmt_node_t<T> type_declaration();
    stmt_node_t<T> variable_declaration();

    stmt_node_t<T> statement();
    stmt_node_t<T> block_statement();
    stmt_node_t<T> break_statement();
    stmt_node_t<T> continue_statement();
    stmt_node_t<T> expression_statement();
    stmt_node_t<T> for_statement();
    stmt_node_t<T> if_statement();
    stmt_node_t<T> return_statement();
    stmt_node_t<T> switch_statement();
    stmt_node_t<T> while_statement();
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
inline void Parser<T>::sync_and_throw(const std::string_view message) {
    const Token &erroneous = previous();
    error(message, erroneous);
    synchronize();
    throw ParseException{erroneous, message};
}

template <typename T>
void Parser<T>::synchronize() {
    advance();

    while (!is_at_end()) {
        if (previous().type == TokenType::SEMICOLON || previous().type == TokenType::END_OF_LINE) {
            return;
        }

        switch(peek().type) {
            case TokenType::BREAK:
            case TokenType::CONTINUE:
            case TokenType::CLASS:
            case TokenType::FN:
            case TokenType::FOR:
            case TokenType::IF:
            case TokenType::IMPORT:
            case TokenType::PRIVATE:
            case TokenType::PROTECTED:
            case TokenType::PUBLIC:
            case TokenType::RETURN:
            case TokenType::TYPE:
            case TokenType::VAL:
            case TokenType::VAR:
            case TokenType::WHILE:
                return;
            default:
                ;
        }

        advance();
    }
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
    add_rule(TokenType::CASE,          {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::CLASS,         {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::CONST,         {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::CONTINUE,      {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::DEFAULT,       {nullptr, nullptr, ParsePrecedence::NONE});
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
    add_rule(TokenType::SWITCH,        {nullptr, nullptr, ParsePrecedence::NONE});
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
        synchronize();
        throw ParseException{peek(), message};
    }
}

template <typename T>
template <typename ... Args>
inline void Parser<T>::consume(std::string_view message, const Token &where, Args ... args) {
    if (!match(args...)) {
        error(message, where);
        synchronize();
        throw ParseException{where, message};
    }
}

template <typename T>
inline expr_node_t<T> Parser<T>::parse_precedence(ParsePrecedence::of precedence) {
    using namespace std::string_literals;
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
inline expr_node_t<T> Parser<T>::grouping(bool) {
    expr_node_t<T> expr = expression();
    consume("Expected ')' after parenthesized expression", TokenType::RIGHT_PAREN);
    return expr_node_t<T>{allocate(GroupingExpr<T>, std::move(expr))};
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
inline expr_node_t<T> Parser<T>::super(bool) {
    if (!(in_class && in_function)) {
        sync_and_throw("Cannot use super expression outside a class");
    }
    Token super = previous();
    consume("Expected '.' after 'super' keyword", TokenType::DOT);
    consume("Expected name after '.' in super expression", TokenType::IDENTIFIER);
    Token name = previous();
    return expr_node_t<T>{allocate(SuperExpr<T>, std::move(super), std::move(name))};
}

template <typename T>
inline expr_node_t<T> Parser<T>::this_expr(bool) {
    if (!(in_class && in_function)) {
        sync_and_throw("Cannot use this keyword outside a class");
    }
    Token keyword = previous();
    return expr_node_t<T>{allocate(ThisExpr<T>, std::move(keyword))};
}

template <typename T>
inline expr_node_t<T> Parser<T>::unary(bool) {
    expr_node_t<T> expr = parse_precedence(get_rule(previous().type).precedence);
    return expr_node_t<T>{allocate(UnaryExpr<T>, previous(), std::move(expr))};
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
inline expr_node_t<T> Parser<T>::and_(bool, expr_node_t<T> left) {
    expr_node_t<T> right = parse_precedence(ParsePrecedence::LOGIC_AND);
    return expr_node_t<T>{allocate(LogicalExpr<T>, std::move(left), previous(), std::move(right))};
}

template <typename T>
inline expr_node_t<T> Parser<T>::binary(bool, expr_node_t<T> left) {
    expr_node_t<T> right = parse_precedence(ParsePrecedence::of(get_rule(previous().type).precedence + 1));
    return expr_node_t<T>{allocate(BinaryExpr<T>, std::move(left), previous(), std::move(right))};
}

template <typename T>
inline expr_node_t<T> Parser<T>::call(bool, expr_node_t<T> function) {
    Token paren = previous();
    std::vector<expr_node_t<T>> args{};
    if (peek().type != TokenType::RIGHT_PAREN) {
        do {
            args.emplace_back(parse_precedence(ParsePrecedence::ASSIGNMENT));
        } while (match(TokenType::COMMA));

    }
    consume("Expected ')' after function call", TokenType::RIGHT_PAREN);
    return expr_node_t<T>{allocate(CallExpr<T>, std::move(function), std::move(paren), std::move(args))};
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
inline expr_node_t<T> Parser<T>::index(bool, expr_node_t<T> object) {
    expr_node_t<T> index = expression();
    consume("Expected ']' after array subscript index", TokenType::RIGHT_INDEX);
    return expr_node_t<T>{allocate(IndexExpr<T>, std::move(object), previous(), std::move(index))};
}

template <typename T>
inline expr_node_t<T> Parser<T>::or_(bool, expr_node_t<T> left) {
    expr_node_t<T> right = parse_precedence(ParsePrecedence::LOGIC_OR);
    return expr_node_t<T>{allocate(LogicalExpr<T>, std::move(left), previous(), std::move(right))};
}

template <typename T>
inline expr_node_t<T> Parser<T>::ternary(bool, expr_node_t<T> left) {
    Token question = previous();
    expr_node_t<T> middle = parse_precedence(ParsePrecedence::LOGIC_OR);
    consume("Expected colon in ternary expression", TokenType::COLON);
    expr_node_t<T> right = parse_precedence(ParsePrecedence::LOGIC_OR);
    return expr_node_t<T>{allocate(TernaryExpr<T>, std::move(left), question, std::move(middle), std::move(right))};
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
        } else if (match(TokenType::IDENTIFIER)) {
            return TypeType::CLASS;
        } else if (match(TokenType::LEFT_INDEX)) {
            return TypeType::LIST;
        } else if (match(TokenType::TYPEOF)) {
            return TypeType::TYPEOF;
        } else if (match(TokenType::NULL_)) {
            return TypeType::NULL_;
        } else {
            error("Unexpected token in type specifier", peek());
            note("The type needs to be one of: bool, int, float, string, an identifier or an array type");
            advance();
            const Token &erroneous = previous();
            synchronize();
            throw ParseException{erroneous, "Unexpected token in type specifier"};
        }
    }();

    SharedData data{type, is_const, is_ref};
    if (type == TypeType::CLASS) {
        Token name = previous();
        return type_node_t<T>{allocate(UserDefinedType<T>, data, std::move(name))};
    } else if (type == TypeType::LIST) {
        return list_type(is_const, is_ref);
    } else if (type == TypeType::TYPEOF) {
        return type_node_t<T>{allocate(TypeofType<T>, expression())};
    } else {
            return type_node_t<T>{allocate(PrimitiveType<T>, data)};
    }
}

template <typename T>
inline type_node_t<T> Parser<T>::list_type(bool is_const, bool is_ref) {
    type_node_t<T> contained = type();
    consume("Expected ',' after contained type of array", TokenType::COMMA);
    expr_node_t<T> size = expression();
    consume("Expected ']' after array size", TokenType::RIGHT_INDEX);
    SharedData data{TypeType::LIST, is_const, is_ref};
    return type_node_t<T>{allocate(ListType<T>, data, std::move(contained),
                                   std::move(size))};
}

////////////////////////////////////////////////////////////////////////////////

template <typename T>
inline stmt_node_t<T> Parser<T>::declaration() {
    if (match(TokenType::CLASS)) {
        return class_declaration();
    } else if (match(TokenType::FN)) {
        return function_declaration();
    } else if (match(TokenType::IMPORT)) {
        return import_statement();
    } else if (match(TokenType::TYPE)) {
        return type_declaration();
    } else if (match(TokenType::VAR, TokenType::VAL)) {
        return variable_declaration();
    } else {
        return statement();
    }
}

template <typename T>
inline stmt_node_t<T> Parser<T>::class_declaration() {
    consume("Expected class name after 'class' keyword", TokenType::IDENTIFIER);
    Token name = previous();
    bool has_ctor{false};
    bool has_dtor{false};
    std::vector<std::pair<stmt_node_t<T>,VisibilityType>> members{};
    std::vector<std::pair<stmt_node_t<T>,VisibilityType>> methods{};

    consume("Expected '{' after class name", TokenType::LEFT_BRACE);
    bool in_class_outer = in_class;
    in_class = true;
    while (!is_at_end() && peek().type != TokenType::RIGHT_BRACE) {
        consume("Expected 'public', 'private' or 'protected' modifier before member declaration", TokenType::PRIVATE,
                TokenType::PUBLIC, TokenType::PROTECTED);

        VisibilityType visibility = [this]() {
            if (previous().type == TokenType::PUBLIC) {
                return VisibilityType::PUBLIC;
            } else if (previous().type == TokenType::PRIVATE) {
                return VisibilityType::PRIVATE;
            } else {
                return VisibilityType::PROTECTED;
            }
        }();

        if (match(TokenType::VAR, TokenType::VAR)) {
            stmt_node_t<T> member = variable_declaration();
            members.emplace_back(std::move(member), visibility);
        } else if (match(TokenType::FN)) {
            bool found_dtor = match(TokenType::BIT_NOT);
            if (found_dtor && peek().lexeme != name.lexeme) {
                advance();
                sync_and_throw("The name of the destructor has to be the same as the name of the class");
            }

            stmt_node_t<T> method = function_declaration();
            const Token &method_name = dynamic_cast<FunctionStmt<T>*>(method.get())->name;

            if (method_name.lexeme == name.lexeme) {
                if (found_dtor && !has_dtor) {
                    has_dtor = true;
                    switch (visibility) {
                        case VisibilityType::PUBLIC:    visibility = VisibilityType::PUBLIC_DTOR; break;
                        case VisibilityType::PRIVATE:   visibility = VisibilityType::PRIVATE_DTOR; break;
                        case VisibilityType::PROTECTED: visibility = VisibilityType::PROTECTED_DTOR; break;

                        default:
                            break;
                    }
                } else if (!has_ctor) {
                    has_ctor = true;
                } else {
                    constexpr std::string_view message = "Cannot declare constructors or destructors more than once";
                    error(message, method_name);
                    throw ParseException{method_name, message};
                }
            }
            methods.emplace_back(std::move(method), visibility);
        } else {
            in_class = in_class_outer;
            sync_and_throw("Expected either member or method declaration in class");
        }
    }
    in_class = in_class_outer;

    consume("Expected '}' at the end of class declaration", TokenType::RIGHT_BRACE);
    return stmt_node_t<T>{allocate(ClassStmt<T>, std::move(name), has_ctor, has_dtor, std::move(members),
                                   std::move(methods))};
}

template <typename T>
inline stmt_node_t<T> Parser<T>::function_declaration() {
    consume("Expected function name after 'fn' keyword", TokenType::IDENTIFIER);
    Token name = previous();

    consume("Expected '(' after function name", TokenType::LEFT_PAREN);
    std::vector<std::pair<Token,type_node_t<T>>> params{};
    if (peek().type != TokenType::RIGHT_PAREN) {
        do {
            advance();
            Token parameter_name = previous();
            consume("Expected ':' after function parameter name", TokenType::COLON);
            type_node_t<T> parameter_type = type();
            params.emplace_back(std::move(parameter_name), std::move(parameter_type));
        } while (match(TokenType::COMMA));
    }
    consume("Expected ')' after function parameters", TokenType::RIGHT_PAREN);

    // Since the scanner can possibly emit end of lines here, they have to be consumed before continuing
    //
    // The reason I haven't put the logic to stop this in the scanner is that dealing with the state would
    // not be that easy and just having to do this once in the parser is fine
    while (peek().type == TokenType::END_OF_LINE) {
        advance();
    }

    consume("Expected '->' after ')' to specify type", TokenType::ARROW);
    type_node_t<T> return_type = type();
    consume("Expected '{' after function return type", TokenType::LEFT_BRACE);

    bool in_function_outer = in_function;
    in_function = true;
    stmt_node_t<T> body = block_statement();
    in_function = in_function_outer;

    return stmt_node_t<T>{allocate(FunctionStmt<T>, std::move(name), std::move(return_type), std::move(params),
                                   std::move(body))};
}

template <typename T>
inline stmt_node_t<T> Parser<T>::import_statement() {
    consume("Expected a name after 'import' keyword", TokenType::IDENTIFIER);
    Token name = previous();
    consume("Expected ';' or newline after imported file", previous(), TokenType::SEMICOLON, TokenType::END_OF_LINE);
    return stmt_node_t<T>{allocate(ImportStmt<T>, std::move(name))};
}

template <typename T>
inline stmt_node_t<T> Parser<T>::type_declaration() {
    consume("Expected type name after 'type' keyword", previous(), TokenType::IDENTIFIER);
    Token name = previous();
    consume("Expected '=' after type name", TokenType::EQUAL);
    type_node_t<T> aliased = type();
    consume("Expected ';' or newline after type alias", TokenType::SEMICOLON, TokenType::END_OF_LINE);
    return stmt_node_t<T>{allocate(TypeStmt<T>, std::move(name), std::move(aliased))};
}

template <typename T>
inline stmt_node_t<T> Parser<T>::variable_declaration() {
    std::string message = "Expected variable name after 'var' keyword";
    if (previous().type == TokenType::VAL) {
        message[32] = 'l';
    }
    consume(message, previous(), TokenType::IDENTIFIER);
    Token name = previous();
    consume("Expected ':' after variable name", previous(), TokenType::COLON);
    type_node_t<T> var_type = type();
    expr_node_t<T> initializer = [this]() {
        if (match(TokenType::EQUAL)) {
            return expression();
        } else {
            return expr_node_t<T>{nullptr};
        }
    }();
    consume("Expected ';' or newline after variable initializer", TokenType::SEMICOLON, TokenType::END_OF_LINE);
    return stmt_node_t<T>{allocate(VarStmt<T>, std::move(name), std::move(var_type), std::move(initializer))};
}

template <typename T>
inline stmt_node_t<T> Parser<T>::statement() {
    if (match(TokenType::LEFT_BRACE)) {
        return block_statement();
    } else if (match(TokenType::BREAK)) {
        return break_statement();
    } else if (match(TokenType::CONTINUE)) {
        return continue_statement();
    } else if (match(TokenType::FOR)) {
        return for_statement();
    } else if (match(TokenType::IF)) {
        return if_statement();
    } else if (match(TokenType::RETURN)) {
        return return_statement();
    } else if (match(TokenType::SWITCH)) {
        return switch_statement();
    } else if (match(TokenType::WHILE)) {
        return while_statement();
    } else {
        return expression_statement();
    }
}

template <typename T>
inline stmt_node_t<T> Parser<T>::block_statement() {
    std::vector<stmt_node_t<T>> statements{};
    while (!is_at_end() && peek().type != TokenType::RIGHT_BRACE) {
        if (match(TokenType::VAR, TokenType::VAL)) {
            statements.emplace_back(variable_declaration());
        } else {
            statements.emplace_back(statement());
        }
    }
    consume("Expected '}' after block", TokenType::RIGHT_BRACE);
    return stmt_node_t<T>{allocate(BlockStmt<T>, std::move(statements))};
}

template <typename T>
template <template <typename> typename Allocated>
inline stmt_node_t<T> Parser<T>::single_token_statement(const std::string_view token, const bool condition,
                                                        const std::string_view error_message) {
    if (!condition) {
        sync_and_throw(error_message);
    }
    Token keyword = previous();
    using namespace std::string_literals;
    const std::string consume_message = "Expected ';' or newline after "s + &token[0] + " keyword";
    consume(consume_message, TokenType::SEMICOLON, TokenType::END_OF_LINE);
    return stmt_node_t<T>{allocate(Allocated<T>, std::move(keyword))};
}

template <typename T>
inline stmt_node_t<T> Parser<T>::break_statement() {
    return single_token_statement<BreakStmt>("break", (in_loop || in_switch),
                                             "Cannot use 'break' outside a loop or switch.");
}

template <typename T>
inline stmt_node_t<T> Parser<T>::continue_statement() {
    return single_token_statement<ContinueStmt>("continue", in_loop, "Cannot use 'continue' outside a loop or switch");
}

template <typename T>
inline stmt_node_t<T> Parser<T>::expression_statement() {
    expr_node_t<T> expr = expression();
    consume("Expected ';' or newline after expression", TokenType::SEMICOLON, TokenType::END_OF_LINE);
    return stmt_node_t<T>{allocate(ExpressionStmt<T>, std::move(expr))};
}

template <typename T>
inline stmt_node_t<T> Parser<T>::for_statement() {
    consume("Expected '(' after 'for' keyword", TokenType::LEFT_PAREN);
    stmt_node_t<T> initializer = nullptr;
    if (match(TokenType::VAR, TokenType::VAL)) {
        initializer = variable_declaration();
    } else if (!match(TokenType::SEMICOLON)) {
        initializer = expression_statement();
    }

    expr_node_t<T> condition = nullptr;
    if (peek().type != TokenType::SEMICOLON) {
        condition = expression();
    }
    consume("Expected ';' after loop condition", TokenType::SEMICOLON);

    stmt_node_t<T> increment = nullptr;
    if (peek().type != TokenType::RIGHT_PAREN) {
        increment = stmt_node_t<T>{allocate(ExpressionStmt<T>, expression())};
    }
    consume("Expected ')' after for loop header", TokenType::RIGHT_PAREN);

    bool in_loop_outer = in_loop;
    in_loop = true;

    stmt_node_t<T> body = statement();

    auto *modified_body = allocate(BlockStmt<T>, {});
    modified_body->stmts.emplace_back(std::move(body));
    modified_body->stmts.emplace_back(std::move(increment));
    stmt_node_t<T> desugared_loop = stmt_node_t<T>{allocate(WhileStmt<T>, std::move(condition),
                                                            std::move(stmt_node_t<T>{modified_body}))};

    auto *loop = allocate(BlockStmt<T>, {});
    loop->stmts.emplace_back(std::move(initializer));
    loop->stmts.emplace_back(std::move(desugared_loop));

    in_loop = in_loop_outer;

    return stmt_node_t<T>{loop};
}

template <typename T>
inline stmt_node_t<T> Parser<T>::if_statement() {
    expr_node_t<T> condition = expression();
    stmt_node_t<T> then_branch = statement();
    if (match(TokenType::ELSE)) {
        stmt_node_t<T> else_branch = statement();
        return stmt_node_t<T>{allocate(IfStmt<T>, std::move(condition), std::move(then_branch),
                                       std::move(else_branch))};
    } else {
        return stmt_node_t<T>{allocate(IfStmt<T>, std::move(condition), std::move(then_branch), nullptr)};
    }
}

template <typename T>
inline stmt_node_t<T> Parser<T>::return_statement() {
    if (!in_function) {
        sync_and_throw("Cannot use 'return' keyword outside a function");
    }

    Token keyword = previous();

    expr_node_t<T> return_value = [this]() {
        if (peek().type != TokenType::SEMICOLON && peek().type != TokenType::END_OF_LINE) {
            return expression();
        } else {
            return expr_node_t<T>{nullptr};
        }
    }();

    consume("Expected ';' or newline after return statement", TokenType::SEMICOLON, TokenType::END_OF_LINE);
    return stmt_node_t<T>{allocate(ReturnStmt<T>, std::move(keyword), std::move(return_value))};
}

template <typename T>
inline stmt_node_t<T> Parser<T>::switch_statement() {
    expr_node_t<T> condition = expression();
    std::vector<std::pair<expr_node_t<T>,stmt_node_t<T>>> cases{};
    std::optional<stmt_node_t<T>> default_case = std::nullopt;
    consume("Expected '{' after switch statement condition", TokenType::LEFT_BRACE);
    bool in_switch_outer = in_switch;
    in_switch = true;
    while (!is_at_end() && peek().type != TokenType::RIGHT_BRACE) {
        if (match(TokenType::CASE)) {
            expr_node_t<T> expr = expression();
            consume("Expected ':' after case expression", TokenType::COLON);

            stmt_node_t<T> stmt = statement();
            cases.emplace_back(std::move(expr), std::move(stmt));
        } else if (match(TokenType::DEFAULT) && default_case == std::nullopt) {
            consume("Expected ':' after case expression", TokenType::COLON);
            stmt_node_t<T> stmt = statement();
            default_case = std::move(stmt);
        } else if (default_case != std::nullopt) {
            in_switch = in_switch_outer;
            sync_and_throw("Cannot have more than one default cases in a switch");
        } else {
            in_switch = in_switch_outer;
            sync_and_throw("Expected either 'case' or 'default' as start of switch statement cases");
        }
    }

    in_switch = in_switch_outer;
    consume("Expected '}' at the end of switch statement", TokenType::RIGHT_BRACE);
    return stmt_node_t<T>{allocate(SwitchStmt<T>, std::move(condition), std::move(cases), std::move(default_case))};
}

template <typename T>
inline stmt_node_t<T> Parser<T>::while_statement() {
    expr_node_t<T> condition = expression();
    bool in_loop_outer = in_loop;
    in_loop = true;
    stmt_node_t<T> body = statement();
    in_loop = in_loop_outer;
    return stmt_node_t<T>{allocate(WhileStmt<T>, std::move(condition), std::move(body))};
}

#endif