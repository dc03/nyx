/* Copyright (C) 2020-2022  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "Frontend/Parser/Parser.hpp"

#include "Backend/VirtualMachine/Value.hpp"
#include "CLIConfigParser.hpp"
#include "Common.hpp"
#include "ErrorLogger/ErrorLogger.hpp"
#include "Frontend/FrontendManager.hpp"
#include "Frontend/Parser/FeatureFlagError.hpp"
#include "Frontend/Parser/TypeResolver.hpp"
#include "Frontend/Scanner/Scanner.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <utility>

struct ParseException : public std::invalid_argument {
    Token token{};
    explicit ParseException(Token token, const std::string_view error)
        : std::invalid_argument(std::string{error.begin(), error.end()}), token{std::move(token)} {}
};

void Parser::warning(const std::vector<std::string> &message, const Token &where) const noexcept {
    ctx->logger.warning(current_module, message, where);
}

void Parser::error(const std::vector<std::string> &message, const Token &where) const noexcept {
    ctx->logger.error(current_module, message, where);
}

void Parser::note(const std::vector<std::string> &message) const noexcept {
    ctx->logger.note(current_module, message);
}

void Parser::add_rule(TokenType type, ParseRule rule) noexcept {
    rules[static_cast<std::size_t>(type)] = rule;
}

[[nodiscard]] constexpr const typename Parser::ParseRule &Parser::get_rule(TokenType type) const noexcept {
    return rules[static_cast<std::size_t>(type)];
}

void Parser::throw_parse_error(const std::string_view message) const {
    const Token &erroneous = peek();
    error({std::string{message}}, erroneous);
    throw ParseException{erroneous, message};
}

void Parser::throw_parse_error(const std::string_view message, const Token &where) const {
    error({std::string{message}}, where);
    throw ParseException{where, message};
}

void Parser::synchronize() {
    advance();

    while (not is_at_end()) {
        if (current_token.type == TokenType::SEMICOLON || current_token.type == TokenType::END_OF_LINE ||
            current_token.type == TokenType::RIGHT_BRACE) {
            return;
        }

        switch (peek().type) {
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
            case TokenType::CONST:
            case TokenType::VAR:
            case TokenType::WHILE: return;
            default:;
        }

        advance();
    }
}

void Parser::setup_rules() noexcept {
    // clang-format off
    add_rule(TokenType::COMMA,         {nullptr, &Parser::comma, ParsePrecedence::of::COMMA});
    add_rule(TokenType::EQUAL,         {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::PLUS_EQUAL,    {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::MINUS_EQUAL,   {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::STAR_EQUAL,    {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::SLASH_EQUAL,   {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::QUESTION,      {nullptr, &Parser::ternary, ParsePrecedence::of::TERNARY});
    add_rule(TokenType::COLON,         {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::BIT_OR,        {nullptr, &Parser::binary, ParsePrecedence::of::BIT_OR});
    add_rule(TokenType::BIT_XOR,       {nullptr, &Parser::binary, ParsePrecedence::of::BIT_XOR});
    add_rule(TokenType::BIT_AND,       {nullptr, &Parser::binary, ParsePrecedence::of::BIT_AND});
    add_rule(TokenType::NOT_EQUAL,     {nullptr, &Parser::binary, ParsePrecedence::of::EQUALITY});
    add_rule(TokenType::EQUAL_EQUAL,   {nullptr, &Parser::binary, ParsePrecedence::of::EQUALITY});
    add_rule(TokenType::GREATER,       {nullptr, &Parser::binary, ParsePrecedence::of::ORDERING});
    add_rule(TokenType::GREATER_EQUAL, {nullptr, &Parser::binary, ParsePrecedence::of::ORDERING});
    add_rule(TokenType::LESS,          {nullptr, &Parser::binary, ParsePrecedence::of::ORDERING});
    add_rule(TokenType::LESS_EQUAL,    {nullptr, &Parser::binary, ParsePrecedence::of::ORDERING});
    add_rule(TokenType::RIGHT_SHIFT,   {nullptr, &Parser::binary, ParsePrecedence::of::SHIFT});
    add_rule(TokenType::LEFT_SHIFT,    {nullptr, &Parser::binary, ParsePrecedence::of::SHIFT});
    add_rule(TokenType::DOT_DOT,       {nullptr, &Parser::binary, ParsePrecedence::of::RANGE});
    add_rule(TokenType::DOT_DOT_EQUAL, {nullptr, &Parser::binary, ParsePrecedence::of::RANGE});
    add_rule(TokenType::MINUS,         {&Parser::unary, &Parser::binary, ParsePrecedence::of::SUM});
    add_rule(TokenType::PLUS,          {&Parser::unary, &Parser::binary, ParsePrecedence::of::SUM});
    add_rule(TokenType::MODULO,        {nullptr, &Parser::binary, ParsePrecedence::of::PRODUCT});
    add_rule(TokenType::SLASH,         {nullptr, &Parser::binary, ParsePrecedence::of::PRODUCT});
    add_rule(TokenType::STAR,          {nullptr, &Parser::binary, ParsePrecedence::of::PRODUCT});
    add_rule(TokenType::NOT,           {&Parser::unary, nullptr, ParsePrecedence::of::UNARY});
    add_rule(TokenType::BIT_NOT,       {&Parser::unary, nullptr, ParsePrecedence::of::UNARY});
    add_rule(TokenType::PLUS_PLUS,     {&Parser::unary, nullptr, ParsePrecedence::of::UNARY});
    add_rule(TokenType::MINUS_MINUS,   {&Parser::unary, nullptr, ParsePrecedence::of::UNARY});
    add_rule(TokenType::DOT,           {nullptr, &Parser::dot, ParsePrecedence::of::CALL});
    add_rule(TokenType::LEFT_PAREN,    {&Parser::grouping, &Parser::call, ParsePrecedence::of::CALL});
    add_rule(TokenType::RIGHT_PAREN,   {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::LEFT_INDEX,    {&Parser::list, &Parser::index, ParsePrecedence::of::CALL});
    add_rule(TokenType::RIGHT_INDEX,   {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::LEFT_BRACE,    {&Parser::tuple, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::RIGHT_BRACE,   {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::DOUBLE_COLON,  {nullptr, &Parser::scope_access, ParsePrecedence::of::PRIMARY});
    add_rule(TokenType::SEMICOLON,     {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::ARROW,         {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::IDENTIFIER,    {&Parser::variable, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::STRING_VALUE,  {&Parser::literal, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::INT_VALUE,     {&Parser::literal, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::FLOAT_VALUE,   {&Parser::literal, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::AND,           {nullptr, &Parser::and_, ParsePrecedence::of::LOGIC_AND});
    add_rule(TokenType::BREAK,         {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::CLASS,         {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::CONST,         {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::CONTINUE,      {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::DEFAULT,       {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::ELSE,          {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::FALSE,         {&Parser::literal, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::FLOAT,         {&Parser::variable, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::FN,            {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::FOR,           {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::IF,            {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::IMPORT,        {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::INT,           {&Parser::variable, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::MOVE,          {&Parser::move, nullptr, ParsePrecedence::of::PRIMARY});
    add_rule(TokenType::NULL_,         {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::OR,            {nullptr, &Parser::or_, ParsePrecedence::of::LOGIC_OR});
    add_rule(TokenType::PROTECTED,     {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::PRIVATE,       {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::PUBLIC,        {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::REF,           {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::RETURN,        {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::STRING,        {&Parser::variable, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::SUPER,         {&Parser::super, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::SWITCH,        {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::THIS,          {&Parser::this_expr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::TRUE,          {&Parser::literal, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::TYPE,          {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::TYPEOF,        {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::VAR,           {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::WHILE,         {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::NONE,          {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::END_OF_LINE,   {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::END_OF_FILE,   {nullptr, nullptr, ParsePrecedence::of::NONE});
    // clang-format on
}

Parser::Parser(FrontendContext *ctx, Scanner *scanner, Module *module, std::size_t current_depth)
    : ctx{ctx}, scanner{scanner}, current_module{module}, current_module_depth{current_depth} {
    setup_rules();
    advance();
}

[[nodiscard]] bool Parser::is_at_end() const noexcept {
    return current_token.type == TokenType::END_OF_FILE;
}

const Token &Parser::advance() {
    if (is_at_end()) {
        error({"Found unexpected EOF while parsing"}, current_token);
        throw ParseException{current_token, "Found unexpected EOF while parsing"};
    }

    current_token = scanner->scan_token();
    next_token = scanner->peek_token();
    return current_token;
}

[[nodiscard]] const Token &Parser::peek() const noexcept {
    return next_token;
}

[[nodiscard]] bool Parser::check(TokenType type) const noexcept {
    return peek().type == type;
}

template <typename... Args>
bool Parser::match(Args... args) {
    std::array arr{std::forward<Args>(args)...};

    if (std::any_of(arr.begin(), arr.end(), [this](auto &&arg) { return check(arg); })) {
        advance();
        return true;
    } else {
        return false;
    }
}

template <typename... Args>
void Parser::consume(const std::string_view message, Args... args) {
    if (not match(args...)) {
        error({std::string{message}}, peek());
        throw ParseException{peek(), message};
    }
}

template <typename... Args>
void Parser::consume(std::string_view message, const Token &where, Args... args) {
    if (not match(args...)) {
        error({std::string{message}}, where);
        throw ParseException{where, message};
    }
}

void Parser::recursively_change_module_depth(std::pair<Module, std::size_t> &module, std::size_t value) {
    module.second = value;
    for (std::size_t imported : module.first.imported) {
        recursively_change_module_depth(ctx->parsed_modules[imported], value + 1);
    }
}

bool Parser::has_optimization_flag(const std::string &flag, OptimizationFlag default_) const noexcept {
    if (default_ == OptimizationFlag::DEFAULT_OFF) {
        return ctx->config->contains(flag) && ctx->config->get<std::string>(flag) == "off";
    } else {
        return not ctx->config->contains(flag) || ctx->config->get<std::string>(flag) == "on";
    }
}

template <typename T, typename... Args>
bool all_are(T &&value, Args &&...args) {
    return ((std::forward<T>(value) == std::forward<Args>(args)) && ...);
}

IdentifierTuple Parser::ident_tuple() {
    IdentifierTuple::TupleType result{};
    while (peek().type != TokenType::RIGHT_BRACE) {
        consume("Expected either identifier or '{' in identifier tuple", TokenType::IDENTIFIER, TokenType::LEFT_BRACE);
        if (current_token.type == TokenType::IDENTIFIER) {
            result.emplace_back(
                IdentifierTuple::DeclarationDetails{current_token, NumericConversionType::NONE, false, nullptr});
        } else {
            result.emplace_back(ident_tuple());
        }
        if (peek().type != TokenType::RIGHT_BRACE && peek().type != TokenType::COMMA) {
            consume("Expected '}' after identifier tuple", TokenType::RIGHT_BRACE); // This consume will always fail
        } else {
            match(TokenType::COMMA);
        }
    }
    consume("Expected '}' after identifier tuple", TokenType::RIGHT_BRACE);
    return IdentifierTuple{std::move(result)};
}

std::vector<StmtNode> Parser::program() {
    std::vector<StmtNode> statements;

    while (peek().type != TokenType::END_OF_FILE && peek().type != TokenType::END_OF_LINE) {
        statements.emplace_back(declaration());
    }

    if (peek().type == TokenType::END_OF_LINE) {
        advance();
    }

    consume("Expected EOF at the end of file", TokenType::END_OF_FILE);
    return statements;
}

ExprNode Parser::parse_precedence(ParsePrecedence::of precedence) {
    using namespace std::string_literals;
    advance();

    ExprPrefixParseFn prefix = get_rule(current_token.type).prefix;
    if (prefix == nullptr) {
        std::string message = "Unexpected token in expression '";
        message += [this]() {
            if (current_token.type == TokenType::END_OF_LINE) {
                return "\\n' (newline)"s;
            } else {
                return current_token.lexeme + "'";
            }
        }();
        bool had_error_before = ctx->logger.had_error();
        error({message}, current_token);
        if (had_error_before) {
            note({"This may occur because of previous errors leading to the parser being confused"});
        }
        throw ParseException{current_token, message};
    }

    bool can_assign = precedence <= ParsePrecedence::of::ASSIGNMENT;
    ExprNode left = std::invoke(prefix, this, can_assign);

    while (precedence <= get_rule(peek().type).precedence) {
        ExprInfixParseFn infix = get_rule(advance().type).infix;
        if (infix == nullptr) {
            error({"'", current_token.lexeme, "' cannot occur in an infix/postfix expression"}, current_token);
            if (current_token.type == TokenType::PLUS_PLUS) {
                note({"Postfix increment is not supported"});
            } else if (current_token.type == TokenType::MINUS_MINUS) {
                note({"Postfix decrement is not supported"});
            }
            throw ParseException{current_token, "Incorrect infix/postfix expression"};
        }
        left = std::invoke(infix, this, can_assign, std::move(left));
    }

    if (can_assign && match(TokenType::EQUAL, TokenType::PLUS_EQUAL, TokenType::MINUS_EQUAL, TokenType::STAR_EQUAL,
                          TokenType::SLASH_EQUAL)) {
        error({"Invalid assignment target"}, current_token);
        throw ParseException{current_token, "Invalid assignment target"};
    }

    return left;
}

ExprNode Parser::expression() {
    return parse_precedence(ParsePrecedence::of::COMMA);
}

ExprNode Parser::assignment() {
    return parse_precedence(ParsePrecedence::of::ASSIGNMENT);
}

ExprNode Parser::and_(bool, ExprNode left) {
    Token oper = current_token;
    ExprNode right = parse_precedence(ParsePrecedence::of::LOGIC_AND);
    auto *node = allocate_node(LogicalExpr, std::move(left), std::move(right));
    node->synthesized_attrs.token = std::move(oper);
    return ExprNode{node};
}

ExprNode Parser::binary(bool, ExprNode left) {
    Token oper = current_token;
    ExprNode right = parse_precedence(
        ParsePrecedence::of{static_cast<int>(ParsePrecedence::of(get_rule(current_token.type).precedence)) + 1});

    if (has_optimization_flag(CONSTANT_FOLDING, OptimizationFlag::DEFAULT_ON) &&
        all_are(NodeType::LiteralExpr, left->type_tag(), right->type_tag())) {
        if (auto ret = compute_literal_binary_expr(
                dynamic_cast<LiteralExpr &>(*left), oper, dynamic_cast<LiteralExpr &>(*right));
            ret) {
            return ret;
        }
    }

    auto *node = allocate_node(BinaryExpr, std::move(left), std::move(right));
    node->synthesized_attrs.token = std::move(oper);
    return ExprNode{node};
}

ExprNode Parser::call(bool, ExprNode function) {
    Token paren = current_token;
    std::vector<std::tuple<ExprNode, NumericConversionType, bool>> args{};
    if (peek().type != TokenType::RIGHT_PAREN) {
        do {
            args.emplace_back(assignment(), NumericConversionType::NONE, false);
        } while (match(TokenType::COMMA));
    }
    consume("Expected ')' after function call", TokenType::RIGHT_PAREN);
    auto *node = allocate_node(CallExpr, std::move(function), std::move(args), false);
    node->synthesized_attrs.token = std::move(paren);
    return ExprNode{node};
}

ExprNode Parser::comma(bool, ExprNode left) {
    FEATURE_FLAG_DEFAULT_ERROR_SINGLE_MSG(COMMA_OPERATOR, "Usage of comma operator", current_token)

    std::vector<ExprNode> exprs{};
    exprs.emplace_back(std::move(left));
    do {
        exprs.emplace_back(assignment());
    } while (match(TokenType::COMMA));

    return ExprNode{allocate_node(CommaExpr, std::move(exprs))};
}

ExprNode Parser::dot(bool can_assign, ExprNode left) {
    // Roughly based on: https://github.com/rust-lang/rust/pull/71322/files
    //
    // If a dot access is followed by a floating point literal, that likely means the code is in the form of `x.2.0`
    // This will be scanned as `x`, `.`, `2.0`
    // Thus, split the floating literal into its components (`2`, `.`, `0`) and use those components while parsing
    std::vector<Token> components{};
    if (peek().type == TokenType::FLOAT_VALUE) {
        std::string num = peek().lexeme;
        if (num.find('.') == std::string::npos) {
            error({"Use of float literal in member access"}, peek());
            advance();
            throw_parse_error("Use of float literal in member access", current_token);
        }
        std::size_t cursor = 0;
        while (num[cursor] != '.') {
            cursor++;
        }

        components.emplace_back(
            Token{TokenType::INT_VALUE, num.substr(0, cursor), peek().line, peek().start, peek().start + cursor});
        // The dot is unnecessary so it is skipped
        components.emplace_back(
            Token{TokenType::INT_VALUE, num.substr(cursor + 1), peek().line, peek().start + cursor + 1, peek().end});

        advance();
    } else {
        consume("Expected identifier or integer literal after '.'", TokenType::IDENTIFIER, TokenType::INT_VALUE);
    }
    if (not components.empty()) {
        // If components is not empty, transform left
        // For example, `x.2.0`, has left as `x`.
        // Left is transformed into `x.2`
        // Then `name` is set to `0`, to make `x.2.0`
        left = ExprNode{allocate_node(GetExpr, std::move(left), components[0])};
    }

    Token name = current_token;

    // A floating literal was used as an access
    if (not components.empty()) {
        name = components[1];
    }

    if (can_assign && match(TokenType::EQUAL, TokenType::PLUS_EQUAL, TokenType::MINUS_EQUAL, TokenType::STAR_EQUAL,
                          TokenType::SLASH_EQUAL)) {
        Token oper = current_token;
        ExprNode value = assignment();

        auto *node = allocate_node(
            SetExpr, std::move(left), std::move(name), std::move(value), NumericConversionType::NONE, false);
        node->synthesized_attrs.token = std::move(oper);
        return ExprNode{node};
    } else {
        return ExprNode{allocate_node(GetExpr, std::move(left), std::move(name))};
    }
}

ExprNode Parser::index(bool can_assign, ExprNode object) {
    Token oper = current_token;
    ExprNode index = expression();
    consume("Expected ']' after array subscript index", TokenType::RIGHT_INDEX);
    auto ind = IndexExpr{std::move(object), std::move(index)};
    ind.synthesized_attrs.token = std::move(oper);

    if (can_assign && match(TokenType::EQUAL, TokenType::PLUS_EQUAL, TokenType::MINUS_EQUAL, TokenType::STAR_EQUAL,
                          TokenType::SLASH_EQUAL)) {
        Token equals = current_token;
        ExprNode value = assignment();
        auto *assignment =
            allocate_node(ListAssignExpr, std::move(ind), std::move(value), NumericConversionType::NONE, false);
        assignment->synthesized_attrs.token = std::move(equals);
        return ExprNode{assignment};
    }
    return ExprNode{allocate_node(IndexExpr, std::move(ind))};
}

ExprNode Parser::or_(bool, ExprNode left) {
    Token oper = current_token;
    ExprNode right = parse_precedence(ParsePrecedence::of::LOGIC_OR);
    auto *node = allocate_node(LogicalExpr, std::move(left), std::move(right));
    node->synthesized_attrs.token = std::move(oper);
    return ExprNode{node};
}

ExprNode Parser::grouping(bool) {
    ExprNode expr = expression();
    consume("Expected ')' after parenthesized expression", TokenType::RIGHT_PAREN);
    return ExprNode{allocate_node(GroupingExpr, std::move(expr), nullptr)};
}

ExprNode Parser::list(bool) {
    Token bracket = current_token;
    std::vector<ListExpr::ElementType> elements{};
    std::size_t maybeListRepeat = true;

    if (peek().type != TokenType::RIGHT_INDEX) {
        do {
            ExprNode expr = assignment();

            if (maybeListRepeat && match(TokenType::SEMICOLON)) {
                ExprNode quantity = assignment();
                consume("Expected ']' after list expression", peek(), TokenType::RIGHT_INDEX);

                return ExprNode{allocate_node(ListRepeatExpr, std::move(bracket),
                    {std::move(expr), NumericConversionType::NONE, false},
                    {std::move(quantity), NumericConversionType::NONE, false}, nullptr)};
            }

            maybeListRepeat = false;
            elements.emplace_back(std::move(expr), NumericConversionType::NONE, false);
        } while (match(TokenType::COMMA) && peek().type != TokenType::RIGHT_INDEX);
    }

    match(TokenType::COMMA); // Match trailing comma
    consume("Expected ']' after list expression", peek(), TokenType::RIGHT_INDEX);
    return ExprNode{allocate_node(ListExpr, std::move(bracket), std::move(elements), nullptr)};
}

ExprNode Parser::literal(bool) {
    auto *type = allocate_node(PrimitiveType, Type::INT, true, false);
    auto *node = allocate_node(LiteralExpr, LiteralValue{nullptr}, TypeNode{type});
    node->synthesized_attrs.token = current_token;
    switch (current_token.type) {
        case TokenType::INT_VALUE: {
            node->value = LiteralValue{std::stoi(current_token.lexeme)};
            break;
        }
        case TokenType::FLOAT_VALUE: {
            node->value = LiteralValue{std::stod(current_token.lexeme)};
            node->type->primitive = Type::FLOAT;
            break;
        }
        case TokenType::STRING_VALUE: {
            node->type->primitive = Type::STRING;
            node->value = LiteralValue{current_token.lexeme};
            while (match(TokenType::STRING_VALUE)) {
                node->value.to_string() += current_token.lexeme;
            }
            break;
        }
        case TokenType::FALSE: {
            node->type->primitive = Type::BOOL;
            node->value = LiteralValue{false};
            break;
        }
        case TokenType::TRUE: {
            node->type->primitive = Type::BOOL;
            node->value = LiteralValue{true};
            break;
        }
        case TokenType::NULL_: {
            node->type->primitive = Type::NULL_;
            node->value = LiteralValue{nullptr};
            break;
        }

        default: throw ParseException{current_token, "Unexpected TokenType passed to literal parser"};
    }

    return ExprNode{node};
}

ExprNode Parser::move(bool) {
    Token op = current_token;
    consume("Expected identifier after 'move' keyword", TokenType::IDENTIFIER);
    auto *var = allocate_node(VariableExpr, current_token, IdentifierType::LOCAL);
    var->synthesized_attrs.token = var->name;
    auto *expr = allocate_node(MoveExpr, ExprNode{var});
    expr->synthesized_attrs.token = op;
    return ExprNode{expr};
}

ExprNode Parser::scope_access(bool, ExprNode left) {
    Token colon_colon = current_token;
    consume("Expected identifier to be accessed after scope name", TokenType::IDENTIFIER);
    Token name = current_token;
    auto *node = allocate_node(ScopeAccessExpr, std::move(left), std::move(name));
    node->synthesized_attrs.token = std::move(colon_colon);
    return ExprNode{node};
}

ExprNode Parser::super(bool) {
    if (not(in_class && in_function)) {
        throw_parse_error("Cannot use super expression outside a class");
    }
    Token super = current_token;
    consume("Expected '.' after 'super' keyword", TokenType::DOT);
    consume("Expected name after '.' in super expression", TokenType::IDENTIFIER);
    Token name = current_token;
    return ExprNode{allocate_node(SuperExpr, std::move(super), std::move(name))};
}

ExprNode Parser::ternary(bool, ExprNode left) {
    FEATURE_FLAG_DEFAULT_ERROR_SINGLE_MSG(TERNARY_OPERATOR, "Usage of ternary operator", current_token)

    Token question = current_token;
    ExprNode middle = parse_precedence(ParsePrecedence::of::LOGIC_OR);
    consume("Expected colon in ternary expression", TokenType::COLON);
    ExprNode right = parse_precedence(ParsePrecedence::of::TERNARY);

    if (has_optimization_flag(CONSTANT_FOLDING, OptimizationFlag::DEFAULT_ON) &&
        all_are(NodeType::LiteralExpr, left->type_tag(), middle->type_tag(), right->type_tag())) {
        if (auto ret = compute_literal_ternary_expr(dynamic_cast<LiteralExpr &>(*left),
                dynamic_cast<LiteralExpr &>(*middle), dynamic_cast<LiteralExpr &>(*right), question);
            ret) {
            return ret;
        }
    }

    auto *node = allocate_node(TernaryExpr, std::move(left), std::move(middle), std::move(right));
    node->synthesized_attrs.token = std::move(question);
    return ExprNode{node};
}

ExprNode Parser::this_expr(bool) {
    if (not(in_class && in_function)) {
        throw_parse_error("Cannot use 'this' keyword outside a class's constructor or destructor");
    }
    Token keyword = current_token;
    return ExprNode{allocate_node(ThisExpr, std::move(keyword))};
}

ExprNode Parser::tuple(bool) {
    Token brace = current_token;
    std::vector<TupleExpr::ElementType> elements{};
    while (peek().type != TokenType::RIGHT_BRACE) {
        elements.emplace_back(std::make_tuple(ExprNode{assignment()}, NumericConversionType::NONE, false));
        match(TokenType::COMMA);
    }
    consume("Expected '}' after tuple expression", TokenType::RIGHT_BRACE);
    return ExprNode{allocate_node(TupleExpr, std::move(brace), std::move(elements), {})};
}

ExprNode Parser::unary(bool) {
    Token oper = current_token;
    ExprNode expr = parse_precedence(get_rule(current_token.type).precedence);
    auto *node = allocate_node(UnaryExpr, std::move(oper), std::move(expr));
    node->synthesized_attrs.token = node->oper;
    return ExprNode{node};
}

ExprNode Parser::variable(bool can_assign) {
    Token name = current_token;
    if (can_assign && match(TokenType::EQUAL, TokenType::PLUS_EQUAL, TokenType::MINUS_EQUAL, TokenType::STAR_EQUAL,
                          TokenType::SLASH_EQUAL)) {
        Token oper = current_token;
        ExprNode value = assignment();
        auto *node = allocate_node(
            AssignExpr, std::move(name), std::move(value), NumericConversionType::NONE, false, IdentifierType::LOCAL);
        node->synthesized_attrs.token = std::move(oper);
        return ExprNode{node};
    } else if (peek().type == TokenType::DOUBLE_COLON) {
        auto *node = allocate_node(ScopeNameExpr, name, {}, nullptr);
        node->synthesized_attrs.token = std::move(name);
        return ExprNode{node};
    } else {
        auto *node = allocate_node(VariableExpr, name, IdentifierType::LOCAL);
        node->synthesized_attrs.token = std::move(name);
        return ExprNode{node};
    }
}

////////////////////////////////////////////////////////////////////////////////

StmtNode Parser::declaration() {
    try {
        if (match(TokenType::CLASS)) {
            return class_declaration();
        } else if (match(TokenType::FN)) {
            return function_declaration();
        } else if (match(TokenType::IMPORT)) {
            return import_statement();
        } else if (match(TokenType::TYPE)) {
            return type_declaration();
        } else if (match(TokenType::VAR, TokenType::CONST, TokenType::REF)) {
            if (peek().type == TokenType::LEFT_BRACE) {
                return vartuple_declaration();
            } else {
                return variable_declaration();
            }
        } else {
            return statement();
        }
    } catch (const ParseException &) {
        synchronize();
        return {nullptr};
    }
}

StmtNode Parser::class_declaration() {
    consume("Expected class name after 'class' keyword", TokenType::IDENTIFIER);

    if (current_module->classes.find(current_token.lexeme) != current_module->classes.end()) {
        throw_parse_error("Class already defined");
    }

    Token name = current_token;
    FunctionStmt *ctor{nullptr};
    FunctionStmt *dtor{nullptr};
    std::vector<ClassStmt::MemberType> members{};
    std::vector<ClassStmt::MethodType> methods{};
    std::unordered_map<std::string_view, std::size_t> member_map{};
    std::unordered_map<std::string_view, std::size_t> method_map{};

    ScopedManager current_methods_manager{current_methods, &methods};

    consume("Expected '{' after class name", TokenType::LEFT_BRACE);
    ScopedManager class_manager{in_class, true};

    while (not is_at_end() && peek().type != TokenType::RIGHT_BRACE) {
        consume("Expected 'public', 'private' or 'protected' modifier before member declaration", TokenType::PRIVATE,
            TokenType::PUBLIC, TokenType::PROTECTED);

        VisibilityType visibility = [this]() {
            if (current_token.type == TokenType::PUBLIC) {
                return VisibilityType::PUBLIC;
            } else if (current_token.type == TokenType::PRIVATE) {
                return VisibilityType::PRIVATE;
            } else {
                return VisibilityType::PROTECTED;
            }
        }();

        if (match(TokenType::VAR, TokenType::CONST, TokenType::REF)) {
            try {
                std::unique_ptr<VarStmt> member{dynamic_cast<VarStmt *>(variable_declaration().release())};
                member_map[member->name.lexeme] = members.size();
                members.emplace_back(std::move(member), visibility);
            } catch (...) { synchronize(); }
        } else if (match(TokenType::FN)) {
            try {
                bool found_dtor = match(TokenType::BIT_NOT);
                if (found_dtor && peek().lexeme != name.lexeme) {
                    advance();
                    throw_parse_error("The name of the destructor has to be the same as the name of the class");
                }

                std::unique_ptr<FunctionStmt> method{dynamic_cast<FunctionStmt *>(function_declaration().release())};
                const Token &method_name = method->name;

                if (method_name.lexeme == name.lexeme) {
                    if (found_dtor && dtor == nullptr) {
                        using namespace std::string_literals;
                        dtor = method.get();
                        dtor->name.lexeme = "~"s + dtor->name.lexeme; // Turning Foo into ~Foo
                    } else if (ctor == nullptr) {
                        ctor = method.get();
                    } else {
                        constexpr const std::string_view message =
                            "Cannot declare constructors or destructors more than once";
                        error({std::string{message}}, method_name);
                        throw ParseException{method_name, message};
                    }
                }
                method_map[method->name.lexeme] = methods.size();
                methods.emplace_back(std::move(method), visibility);
            } catch (...) { synchronize(); }
        } else {
            throw_parse_error("Expected either member or method declaration in class");
        }
    }

    consume("Expected '}' at the end of class declaration", TokenType::RIGHT_BRACE);
    auto *class_definition = allocate_node(ClassStmt, std::move(name), ctor, dtor, std::move(members),
        std::move(methods), std::move(member_map), std::move(method_map), current_module->full_path);
    current_module->classes[class_definition->name.lexeme] = class_definition;

    return StmtNode{class_definition};
}

StmtNode Parser::function_declaration() {
    bool is_not_dtor = current_token.type != TokenType::BIT_NOT;

    consume("Expected function name after 'fn' keyword", TokenType::IDENTIFIER);

    if (not in_class && current_module->functions.find(current_token.lexeme) != current_module->functions.end()) {
        throw_parse_error("Function already defined");
    } else if (in_class) {
        bool matches_any = std::any_of(current_methods->cbegin(), current_methods->cend(),
            [previous = current_token](const ClassStmt::MethodType &arg) { return arg.first->name == previous; });
        bool matches_dtor = std::any_of(current_methods->cbegin(), current_methods->cend(),
            [previous = current_token](
                const ClassStmt::MethodType &arg) { return arg.first->name.lexeme.substr(1) == previous.lexeme; });

        if (matches_any && (is_not_dtor || matches_dtor)) {
            throw_parse_error("Method already defined", current_token);
        }
    }

    Token name = current_token;
    consume("Expected '(' after function name", TokenType::LEFT_PAREN);

    FunctionStmt *function_definition;
    {
        ScopedManager scope_depth_manager{scope_depth, scope_depth + 1};

        std::vector<FunctionStmt::ParameterType> params{};
        if (peek().type != TokenType::RIGHT_PAREN) {
            do {
                if (match(TokenType::LEFT_BRACE)) {
                    IdentifierTuple tuple = ident_tuple();
                    consume("Expected ':' after var-tuple", TokenType::COLON);
                    TypeNode parameter_type = type();
                    params.emplace_back(std::move(tuple), std::move(parameter_type));
                } else {
                    advance();
                    Token parameter_name = current_token;
                    consume("Expected ':' after function parameter name", TokenType::COLON);
                    TypeNode parameter_type = type();
                    params.emplace_back(std::move(parameter_name), std::move(parameter_type));
                }
            } while (match(TokenType::COMMA));
        }
        consume("Expected ')' after function parameters", TokenType::RIGHT_PAREN);

        // Since the scanner can possibly emit end of lines here, they have to be consumed before continuing
        //
        // The reason I haven't put the logic to stop this in the scanner is that dealing with the state would
        // not be that easy and just having to do this a few times in the parser is fine
        while (peek().type == TokenType::END_OF_LINE) {
            advance();
        }

        consume("Expected '->' after ')' to specify type", TokenType::ARROW);
        TypeNode return_type = type();
        consume("Expected '{' after function return type", TokenType::LEFT_BRACE);

        ScopedManager function_manager{in_function, true};
        StmtNode body = block_statement();

        function_definition = allocate_node(
            FunctionStmt, std::move(name), std::move(return_type), std::move(params), std::move(body), {}, 0, nullptr);
    }

    if (not in_class && scope_depth == 0) {
        current_module->functions[function_definition->name.lexeme] = function_definition;
    }

    return StmtNode{function_definition};
}

StmtNode Parser::import_statement() {
    Token keyword = current_token;
    consume("Expected path to module after 'import' keyword", TokenType::STRING_VALUE);
    Token imported = current_token;
    consume("Expected ';' or newline after imported file", current_token, TokenType::SEMICOLON, TokenType::END_OF_LINE);

    FrontendManager manager{ctx, imported.lexeme, false, current_module_depth + 1};

    auto it = std::find_if(ctx->parsed_modules.begin(), ctx->parsed_modules.end(),
        [&manager](const std::pair<Module, std::size_t> &pair) { return manager.module_name() == pair.first.name; });

    // Avoid parsing a module if its already imported
    if (it != ctx->parsed_modules.end()) {
        if (it->second < (current_module_depth + 1)) {
            recursively_change_module_depth(*it, current_module_depth + 1);
        }
        current_module->imported.push_back(it - ctx->parsed_modules.begin());

        return {nullptr};
    }

    try {
        manager.parse_module();
        manager.check_module();
    } catch (const ParseException &) {}

    ctx->module_path_map[manager.get_module().full_path.c_str()] = ctx->parsed_modules.size();
    ctx->parsed_modules.emplace_back(manager.move_module(), current_module_depth + 1);
    current_module->imported.push_back(ctx->parsed_modules.size() - 1);

    return {nullptr};
}

StmtNode Parser::type_declaration() {
    consume("Expected type name after 'type' keyword", current_token, TokenType::IDENTIFIER);
    Token name = current_token;
    consume("Expected '=' after type name", TokenType::EQUAL);
    TypeNode aliased = type();
    consume("Expected ';' or newline after type alias", TokenType::SEMICOLON, TokenType::END_OF_LINE);
    return StmtNode{allocate_node(TypeStmt, std::move(name), std::move(aliased))};
}

StmtNode Parser::variable_declaration() {
    std::string message = "Expected variable name after '";
    switch (current_token.type) {
        case TokenType::VAR: message += "var"; break;
        case TokenType::CONST: message += "const"; break;
        case TokenType::REF: message += "ref"; break;
        default: break;
    }
    message += "' keyword";
    Token keyword = current_token;
    consume(message, peek(), TokenType::IDENTIFIER);
    Token name = current_token;

    TypeNode var_type = match(TokenType::COLON) ? type() : nullptr;
    consume("Expected initializer after variable name", TokenType::EQUAL);
    ExprNode initializer = expression();
    consume("Expected ';' or newline after variable initializer", TokenType::SEMICOLON, TokenType::END_OF_LINE);

    auto *variable = allocate_node(VarStmt, std::move(keyword), std::move(name), std::move(var_type),
        std::move(initializer), NumericConversionType::NONE, false);
    return StmtNode{variable};
}

StmtNode Parser::vartuple_declaration() {
    Token keyword = current_token;
    advance();

    IdentifierTuple tuple = ident_tuple();

    TypeNode var_types = match(TokenType::COLON) ? type() : nullptr;

    consume("Expected initializer after var-tuple", TokenType::EQUAL);
    Token token = current_token;
    ExprNode initializer = expression();

    consume("Expected ';' or newline after var-tuple initializer", TokenType::SEMICOLON, TokenType::END_OF_LINE);

    return StmtNode{allocate_node(VarTupleStmt, std::move(tuple), std::move(var_types), std::move(initializer),
        std::move(token), std::move(keyword))};
}

StmtNode Parser::statement() {
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

StmtNode Parser::block_statement() {
    std::vector<StmtNode> statements{};
    ScopedManager scope_depth_manager{scope_depth, scope_depth + 1};

    while (not is_at_end() && peek().type != TokenType::RIGHT_BRACE) {
        if (match(TokenType::VAR, TokenType::CONST, TokenType::REF)) {
            if (peek().type == TokenType::LEFT_BRACE) {
                statements.emplace_back(vartuple_declaration());
            } else {
                statements.emplace_back(variable_declaration());
            }
        } else {
            statements.emplace_back(statement());
        }
    }

    consume("Expected '}' after block", TokenType::RIGHT_BRACE);
    return StmtNode{allocate_node(BlockStmt, std::move(statements))};
}

template <typename Allocated>
StmtNode Parser::single_token_statement(
    const std::string_view token, const bool condition, const std::string_view error_message) {
    if (not condition) {
        throw_parse_error(error_message);
    }
    Token keyword = current_token;
    using namespace std::string_literals;
    const std::string consume_message = "Expected ';' or newline after "s + &token[0] + " keyword";
    consume(consume_message, TokenType::SEMICOLON, TokenType::END_OF_LINE);
    return StmtNode{allocate_node(Allocated, std::move(keyword))};
}

StmtNode Parser::break_statement() {
    return single_token_statement<BreakStmt>(
        "break", (in_loop || in_switch), "Cannot use 'break' outside a loop or switch.");
}

StmtNode Parser::continue_statement() {
    return single_token_statement<ContinueStmt>("continue", in_loop, "Cannot use 'continue' outside a loop");
}

StmtNode Parser::expression_statement() {
    ExprNode expr = expression();
    consume("Expected ';' or newline after expression", TokenType::SEMICOLON, TokenType::END_OF_LINE);
    return StmtNode{allocate_node(ExpressionStmt, std::move(expr))};
}

StmtNode Parser::for_statement() {
    Token keyword = current_token;
    consume("Expected '(' after 'for' keyword", TokenType::LEFT_PAREN);
    ScopedManager scope_depth_manager{scope_depth, scope_depth + 1};

    StmtNode initializer = nullptr;
    if (match(TokenType::VAR, TokenType::CONST, TokenType::REF)) {
        initializer = variable_declaration();
    } else if (not match(TokenType::SEMICOLON)) {
        initializer = expression_statement();
    }

    ExprNode condition = nullptr;
    if (peek().type != TokenType::SEMICOLON) {
        condition = expression();
    }
    consume("Expected ';' after loop condition", TokenType::SEMICOLON);

    StmtNode increment = nullptr;
    if (peek().type != TokenType::RIGHT_PAREN) {
        increment = StmtNode{allocate_node(ExpressionStmt, expression())};
    }
    consume("Expected ')' after for loop header", TokenType::RIGHT_PAREN);

    while (peek().type == TokenType::END_OF_LINE) {
        advance();
    }

    ScopedManager loop_manager{in_loop, true};

    consume("Expected '{' after for-loop header", TokenType::LEFT_BRACE);
    StmtNode desugared_loop = StmtNode{
        allocate_node(WhileStmt, std::move(keyword), std::move(condition), block_statement(), std::move(increment))};
    // The increment is only created for for-loops, so that the `continue` statement works properly.

    auto *loop = allocate_node(BlockStmt, {});
    loop->stmts.emplace_back(std::move(initializer));
    loop->stmts.emplace_back(std::move(desugared_loop));

    return StmtNode{loop};
}

StmtNode Parser::if_statement() {
    Token keyword = current_token;
    ExprNode condition = expression();

    while (peek().type == TokenType::END_OF_LINE) {
        advance();
    }

    consume("Expected '{' after if statement condition", TokenType::LEFT_BRACE);
    StmtNode then_branch = block_statement();
    if (match(TokenType::ELSE)) {
        StmtNode else_branch = [this] {
            if (match(TokenType::IF)) {
                return if_statement();
            } else {
                consume("Expected '{' after else keyword", TokenType::LEFT_BRACE);
                return block_statement();
            }
        }();
        return StmtNode{allocate_node(
            IfStmt, std::move(keyword), std::move(condition), std::move(then_branch), std::move(else_branch))};
    } else {
        return StmtNode{
            allocate_node(IfStmt, std::move(keyword), std::move(condition), std::move(then_branch), nullptr)};
    }
}

StmtNode Parser::return_statement() {
    if (not in_function) {
        throw_parse_error("Cannot use 'return' keyword outside a function");
    }

    Token keyword = current_token;

    ExprNode return_value = [this]() {
        if (peek().type != TokenType::SEMICOLON && peek().type != TokenType::END_OF_LINE) {
            return expression();
        } else {
            return ExprNode{nullptr};
        }
    }();

    consume("Expected ';' or newline after return statement", TokenType::SEMICOLON, TokenType::END_OF_LINE);
    return StmtNode{allocate_node(ReturnStmt, std::move(keyword), std::move(return_value), 0, nullptr)};
}

StmtNode Parser::switch_statement() {
    ExprNode condition = expression();

    while (peek().type == TokenType::END_OF_LINE) {
        advance();
    }

    std::vector<std::pair<ExprNode, StmtNode>> cases{};
    StmtNode default_case = nullptr;
    consume("Expected '{' after switch statement condition", TokenType::LEFT_BRACE);

    ScopedManager switch_manager{in_switch, true};

    while (not is_at_end() && peek().type != TokenType::RIGHT_BRACE) {
        if (match(TokenType::DEFAULT)) {
            if (default_case != nullptr) {
                throw_parse_error("Cannot have more than one default case in a switch");
            }
            consume("Expected '->' after 'default'", TokenType::ARROW);
            default_case = statement();
        } else {
            ExprNode expr = expression();
            consume("Expected '->' after case expression", TokenType::ARROW);
            StmtNode stmt = statement();
            cases.emplace_back(std::move(expr), std::move(stmt));
        }
    }

    consume("Expected '}' at the end of switch statement", TokenType::RIGHT_BRACE);
    return StmtNode{allocate_node(SwitchStmt, std::move(condition), std::move(cases), std::move(default_case))};
}

StmtNode Parser::while_statement() {
    Token keyword = current_token;
    ExprNode condition = expression();

    while (peek().type == TokenType::END_OF_LINE) {
        advance();
    }

    ScopedManager loop_manager{in_loop, true};
    consume("Expected '{' after while-loop header", TokenType::LEFT_BRACE);
    StmtNode body = block_statement();

    return StmtNode{allocate_node(WhileStmt, std::move(keyword), std::move(condition), std::move(body), nullptr)};
}

////////////////////////////////////////////////////////////////////////////////

TypeNode Parser::type() {
    bool is_const = match(TokenType::CONST);
    bool is_ref = match(TokenType::REF);
    Type type = [this]() {
        if (match(TokenType::BOOL)) {
            return Type::BOOL;
        } else if (match(TokenType::INT)) {
            return Type::INT;
        } else if (match(TokenType::FLOAT)) {
            return Type::FLOAT;
        } else if (match(TokenType::STRING)) {
            return Type::STRING;
        } else if (match(TokenType::IDENTIFIER)) {
            return Type::CLASS;
        } else if (match(TokenType::LEFT_INDEX)) {
            return Type::LIST;
        } else if (match(TokenType::TYPEOF)) {
            return Type::TYPEOF;
        } else if (match(TokenType::NULL_)) {
            return Type::NULL_;
        } else if (match(TokenType::LEFT_BRACE)) {
            return Type::TUPLE;
        } else {
            error({"Unexpected token in type specifier"}, peek());
            note({"The type needs to be one of: bool, int, float, string, an identifier or an array type"});
            throw ParseException{peek(), "Unexpected token in type specifier"};
        }
    }();

    if (type == Type::CLASS) {
        Token name = current_token;
        return TypeNode{allocate_node(UserDefinedType, type, is_const, is_ref, std::move(name), nullptr)};
    } else if (type == Type::LIST) {
        return list_type(is_const, is_ref);
    } else if (type == Type::TUPLE) {
        return tuple_type(is_const, is_ref);
    } else if (type == Type::TYPEOF) {
        return TypeNode{
            allocate_node(TypeofType, type, is_const, is_ref, parse_precedence(ParsePrecedence::of::LOGIC_OR))};
    } else {
        return TypeNode{allocate_node(PrimitiveType, type, is_const, is_ref)};
    }
}

TypeNode Parser::list_type(bool is_const, bool is_ref) {
    TypeNode contained = type();
    consume("Expected ']' after array declaration", TokenType::RIGHT_INDEX);
    return TypeNode{allocate_node(ListType, Type::LIST, is_const, is_ref, std::move(contained))};
}

TypeNode Parser::tuple_type(bool is_const, bool is_ref) {
    std::vector<TypeNode> types{};
    while (peek().type != TokenType::RIGHT_BRACE) {
        types.emplace_back(type());
        match(TokenType::COMMA);
    }
    consume("Expected '}' after tuple type", TokenType::RIGHT_BRACE);
    return TypeNode{allocate_node(TupleType, Type::TUPLE, is_const, is_ref, std::move(types))};
}
