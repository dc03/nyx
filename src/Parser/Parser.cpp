/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "Parser.hpp"

#include "../Common.hpp"
#include "../ErrorLogger/ErrorLogger.hpp"
#include "../Module.hpp"
#include "../Scanner/Scanner.hpp"
#include "TypeResolver.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <utility>

std::vector<std::pair<Module, std::size_t>> Parser::parsed_modules{};

struct ParseException : public std::invalid_argument {
    Token token{};
    explicit ParseException(Token token, const std::string_view error)
        : std::invalid_argument(std::string{error.begin(), error.end()}), token{std::move(token)} {}
};

struct ScopedBooleanManager {
    bool &scoped_bool;
    bool previous_value{};
    explicit ScopedBooleanManager(bool &controlled) : scoped_bool{controlled} {
        previous_value = controlled;
        controlled = true;
    }

    ~ScopedBooleanManager() { scoped_bool = previous_value; }
};

struct ScopedIntegerManager {
    std::size_t &scoped_int;
    explicit ScopedIntegerManager(std::size_t &controlled) : scoped_int{controlled} { controlled++; }
    ~ScopedIntegerManager() { scoped_int--; }
};

void Parser::add_rule(TokenType type, ParseRule rule) noexcept {
    rules[static_cast<std::size_t>(type)] = rule;
}

[[nodiscard]] constexpr const typename Parser::ParseRule &Parser::get_rule(TokenType type) const noexcept {
    return rules[static_cast<std::size_t>(type)];
}

void Parser::throw_parse_error(const std::string_view message) const {
    const Token &erroneous = peek();
    error(message, erroneous);
    throw ParseException{erroneous, message};
}

void Parser::synchronize() {
    advance();

    while (!is_at_end()) {
        if (previous().type == TokenType::SEMICOLON || previous().type == TokenType::END_OF_LINE) {
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
            case TokenType::VAL:
            case TokenType::VAR:
            case TokenType::WHILE: return;
            default:;
        }

        advance();
    }
}

Parser::Parser(const std::vector<Token> &tokens, Module &module, std::size_t current_depth)
    : tokens{tokens}, current_module{module}, current_module_depth{current_depth} {
    // clang-format off
    add_rule(TokenType::COMMA,         {nullptr, &Parser::comma, ParsePrecedence::of::COMMA});
    add_rule(TokenType::EQUAL,         {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::PLUS_EQUAL,    {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::MINUS_EQUAL,   {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::STAR_EQUAL,    {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::SLASH_EQUAL,   {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::QUESTION,      {nullptr, &Parser::ternary, ParsePrecedence::of::ASSIGNMENT});
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
    add_rule(TokenType::LEFT_INDEX,    {nullptr, &Parser::index, ParsePrecedence::of::CALL});
    add_rule(TokenType::RIGHT_INDEX,   {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::LEFT_BRACE,    {nullptr, nullptr, ParsePrecedence::of::NONE});
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
    add_rule(TokenType::CASE,          {nullptr, nullptr, ParsePrecedence::of::NONE});
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
    add_rule(TokenType::VAL,           {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::VAR,           {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::WHILE,         {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::NONE,          {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::END_OF_LINE,   {nullptr, nullptr, ParsePrecedence::of::NONE});
    add_rule(TokenType::END_OF_FILE,   {nullptr, nullptr, ParsePrecedence::of::NONE});
    // clang-format on
}

[[nodiscard]] bool Parser::is_at_end() const noexcept {
    return current >= tokens.size();
}

[[nodiscard]] const Token &Parser::previous() const noexcept {
    return tokens[current - 1];
}

const Token &Parser::advance() {
    if (is_at_end()) {
        error("Found unexpected EOF while parsing", previous());
        throw ParseException{previous(), "Found unexpected EOF while parsing"};
    }

    current++;
    return tokens[current - 1];
}

[[nodiscard]] const Token &Parser::peek() const noexcept {
    return tokens[current];
}

[[nodiscard]] bool Parser::check(TokenType type) const noexcept {
    return peek().type == type;
}

template <typename... Args>
bool Parser::match(Args... args) {
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

template <typename... Args>
void Parser::consume(const std::string_view message, Args... args) {
    if (!match(args...)) {
        error(message, peek());
        throw ParseException{peek(), message};
    }
}

template <typename... Args>
void Parser::consume(std::string_view message, const Token &where, Args... args) {
    if (!match(args...)) {
        error(message, where);
        throw ParseException{where, message};
    }
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

    ExprPrefixParseFn prefix = get_rule(previous().type).prefix;
    if (prefix == nullptr) {
        std::string message = "Unexpected token in expression '";
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

    bool can_assign = precedence <= ParsePrecedence::of::ASSIGNMENT;
    ExprNode left = std::invoke(prefix, this, can_assign);

    while (precedence <= get_rule(peek().type).precedence) {
        ExprInfixParseFn infix = get_rule(advance().type).infix;
        left = std::invoke(infix, this, can_assign, std::move(left));
    }

    if (can_assign && match(TokenType::EQUAL, TokenType::PLUS_EQUAL, TokenType::MINUS_EQUAL, TokenType::STAR_EQUAL,
                          TokenType::SLASH_EQUAL)) {
        error("Invalid assignment target", previous());
        throw ParseException{previous(), "Invalid assignment target"};
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
    Token oper = previous();
    ExprNode right = parse_precedence(ParsePrecedence::of::LOGIC_AND);
    auto *node = allocate_node(LogicalExpr, std::move(left), std::move(right));
    node->resolved.token = std::move(oper);
    return ExprNode{node};
}

ExprNode Parser::binary(bool, ExprNode left) {
    Token oper = previous();
    ExprNode right = parse_precedence(
        ParsePrecedence::of{static_cast<int>(ParsePrecedence::of(get_rule(previous().type).precedence)) + 1});
    auto *node = allocate_node(BinaryExpr, std::move(left), std::move(right));
    node->resolved.token = std::move(oper);
    return ExprNode{node};
}

ExprNode Parser::call(bool, ExprNode function) {
    Token paren = previous();
    std::vector<std::tuple<ExprNode, NumericConversionType, bool>> args{};
    if (peek().type != TokenType::RIGHT_PAREN) {
        do {
            args.emplace_back(assignment(), NumericConversionType::NONE, false);
        } while (match(TokenType::COMMA));
    }
    consume("Expected ')' after function call", TokenType::RIGHT_PAREN);
    auto *node = allocate_node(CallExpr, std::move(function), std::move(args), false);
    node->resolved.token = std::move(paren);
    return ExprNode{node};
}

ExprNode Parser::comma(bool, ExprNode left) {
    std::vector<ExprNode> exprs{};
    exprs.emplace_back(std::move(left));
    do {
        exprs.emplace_back(assignment());
    } while (match(TokenType::COMMA));

    return ExprNode{allocate_node(CommaExpr, std::move(exprs))};
}

ExprNode Parser::dot(bool can_assign, ExprNode left) {
    consume("Expected identifier after '.'", TokenType::IDENTIFIER);
    Token name = previous();
    if (can_assign && match(TokenType::EQUAL, TokenType::PLUS_EQUAL, TokenType::MINUS_EQUAL, TokenType::STAR_EQUAL,
                          TokenType::SLASH_EQUAL)) {
        Token oper = previous();
        ExprNode value = assignment();

        auto *node = allocate_node(
            SetExpr, std::move(left), std::move(name), std::move(value), NumericConversionType::NONE, false);
        node->resolved.token = std::move(oper);
        return ExprNode{node};
    } else {
        return ExprNode{allocate_node(GetExpr, std::move(left), std::move(name))};
    }
}

ExprNode Parser::index(bool can_assign, ExprNode object) {
    Token oper = previous();
    ExprNode index = expression();
    consume("Expected ']' after array subscript index", TokenType::RIGHT_INDEX);
    auto ind = IndexExpr{std::move(object), std::move(index)};
    ind.resolved.token = std::move(oper);

    if (can_assign && match(TokenType::EQUAL, TokenType::PLUS_EQUAL, TokenType::MINUS_EQUAL, TokenType::STAR_EQUAL,
                          TokenType::SLASH_EQUAL)) {
        Token equals = previous();
        ExprNode value = assignment();
        auto *assignment =
            allocate_node(ListAssignExpr, std::move(ind), std::move(value), NumericConversionType::NONE, false);
        assignment->resolved.token = std::move(equals);
        return ExprNode{assignment};
    }
    return ExprNode{allocate_node(IndexExpr, std::move(ind))};
}

ExprNode Parser::or_(bool, ExprNode left) {
    Token oper = previous();
    ExprNode right = parse_precedence(ParsePrecedence::of::LOGIC_OR);
    auto *node = allocate_node(LogicalExpr, std::move(left), std::move(right));
    node->resolved.token = std::move(oper);
    return ExprNode{node};
}

ExprNode Parser::grouping(bool) {
    ExprNode expr = expression();
    consume("Expected ')' after parenthesized expression", TokenType::RIGHT_PAREN);
    return ExprNode{allocate_node(GroupingExpr, std::move(expr))};
}

ExprNode Parser::literal(bool) {
    auto *type = allocate_node(PrimitiveType, SharedData{Type::INT, true, false});
    auto *node = allocate_node(LiteralExpr, LiteralValue{nullptr}, TypeNode{type});
    node->resolved.token = previous();
    switch (previous().type) {
        case TokenType::INT_VALUE: {
            node->value = LiteralValue{std::stoi(previous().lexeme)};
            break;
        }
        case TokenType::FLOAT_VALUE: {
            node->value = LiteralValue{std::stod(previous().lexeme)};
            node->type->data.type = Type::FLOAT;
            break;
        }
        case TokenType::STRING_VALUE: {
            node->type->data.type = Type::STRING;
            node->value = LiteralValue{previous().lexeme};
            break;
        }
        case TokenType::FALSE: {
            node->type->data.type = Type::BOOL;
            node->value = LiteralValue{false};
            break;
        }
        case TokenType::TRUE: {
            node->type->data.type = Type::BOOL;
            node->value = LiteralValue{true};
            break;
        }
        case TokenType::NULL_: {
            node->type->data.type = Type::NULL_;
            node->value = LiteralValue{nullptr};
            break;
        }

        default: throw ParseException{previous(), "Unexpected TokenType passed to literal parser"};
    }

    return ExprNode{node};
}

ExprNode Parser::scope_access(bool, ExprNode left) {
    Token colon_colon = previous();
    consume("Expected identifier to be accessed after scope name", TokenType::IDENTIFIER);
    Token name = previous();
    auto *node = allocate_node(ScopeAccessExpr, std::move(left), std::move(name));
    node->resolved.token = std::move(colon_colon);
    return ExprNode{node};
}

ExprNode Parser::super(bool) {
    if (!(in_class && in_function)) {
        throw_parse_error("Cannot use super expression outside a class");
    }
    Token super = previous();
    consume("Expected '.' after 'super' keyword", TokenType::DOT);
    consume("Expected name after '.' in super expression", TokenType::IDENTIFIER);
    Token name = previous();
    return ExprNode{allocate_node(SuperExpr, std::move(super), std::move(name))};
}

ExprNode Parser::ternary(bool, ExprNode left) {
    Token question = previous();
    ExprNode middle = parse_precedence(ParsePrecedence::of::LOGIC_OR);
    consume("Expected colon in ternary expression", TokenType::COLON);
    ExprNode right = parse_precedence(ParsePrecedence::of::LOGIC_OR);
    auto *node = allocate_node(TernaryExpr, std::move(left), std::move(middle), std::move(right));
    node->resolved.token = std::move(question);
    return ExprNode{node};
}

ExprNode Parser::this_expr(bool) {
    if (!(in_class && in_function)) {
        throw_parse_error("Cannot use 'this' keyword outside a class's constructor or destructor");
    }
    Token keyword = previous();
    return ExprNode{allocate_node(ThisExpr, std::move(keyword))};
}

ExprNode Parser::unary(bool) {
    Token oper = previous();
    ExprNode expr = parse_precedence(get_rule(previous().type).precedence);
    auto *node = allocate_node(UnaryExpr, std::move(oper), std::move(expr));
    node->resolved.token = node->oper;
    return ExprNode{node};
}

ExprNode Parser::variable(bool can_assign) {
    Token name = previous();
    if (can_assign && match(TokenType::EQUAL, TokenType::PLUS_EQUAL, TokenType::MINUS_EQUAL, TokenType::STAR_EQUAL,
                          TokenType::SLASH_EQUAL)) {
        Token oper = previous();
        ExprNode value = assignment();
        auto *node = allocate_node(AssignExpr, std::move(name), std::move(value), NumericConversionType::NONE, false);
        node->resolved.token = std::move(oper);
        return ExprNode{node};
    } else if (peek().type == TokenType::DOUBLE_COLON) {
        auto *node = allocate_node(ScopeNameExpr, name);
        node->resolved.token = std::move(name);
        return ExprNode{node};
    } else {
        auto *node = allocate_node(VariableExpr, name, IdentifierType::VARIABLE);
        node->resolved.token = std::move(name);
        return ExprNode{node};
    }
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
        } else {
            error("Unexpected token in type specifier", peek());
            note("The type needs to be one of: bool, int, float, string, an identifier or an array type");
            throw ParseException{peek(), "Unexpected token in type specifier"};
        }
    }();

    SharedData data{type, is_const, is_ref};
    if (type == Type::CLASS) {
        Token name = previous();
        return TypeNode{allocate_node(UserDefinedType, data, std::move(name))};
    } else if (type == Type::LIST) {
        return list_type(is_const, is_ref);
    } else if (type == Type::TYPEOF) {
        return TypeNode{allocate_node(TypeofType, data, parse_precedence(ParsePrecedence::of::LOGIC_OR))};
    } else {
        return TypeNode{allocate_node(PrimitiveType, data)};
    }
}

TypeNode Parser::list_type(bool is_const, bool is_ref) {
    TypeNode contained = type();
    ExprNode size = match(TokenType::COMMA) ? expression() : nullptr;
    consume("Expected ']' after array declaration", TokenType::RIGHT_INDEX);
    SharedData data{Type::LIST, is_const, is_ref};
    return TypeNode{allocate_node(ListType, data, std::move(contained), std::move(size))};
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
        } else if (match(TokenType::VAR, TokenType::VAL)) {
            return variable_declaration();
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

    if (current_module.classes.find(previous().lexeme) != current_module.classes.end()) {
        throw_parse_error("Class already defined");
    }

    Token name = previous();
    FunctionStmt *ctor{nullptr};
    FunctionStmt *dtor{nullptr};
    std::vector<ClassStmt::MemberType> members{};
    std::vector<ClassStmt::MethodType> methods{};

    // ScopedIntegerManager depth_manager{scope_depth};

    consume("Expected '{' after class name", TokenType::LEFT_BRACE);
    ScopedBooleanManager class_manager{in_class};
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

        if (match(TokenType::VAR, TokenType::VAL)) {
            try {
                std::unique_ptr<VarStmt> member{dynamic_cast<VarStmt *>(variable_declaration().release())};
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
                        error(message, method_name);
                        throw ParseException{method_name, message};
                    }
                }
                methods.emplace_back(std::move(method), visibility);
            } catch (...) { synchronize(); }
        } else {
            throw_parse_error("Expected either member or method declaration in class");
        }
    }

    consume("Expected '}' at the end of class declaration", TokenType::RIGHT_BRACE);
    auto *class_definition =
        allocate_node(ClassStmt, std::move(name), ctor, dtor, std::move(members), std::move(methods));
    current_module.classes[class_definition->name.lexeme] = class_definition;

    return StmtNode{class_definition};
}

StmtNode Parser::function_declaration() {
    consume("Expected function name after 'fn' keyword", TokenType::IDENTIFIER);

    if (!in_class && current_module.functions.find(previous().lexeme) != current_module.functions.end()) {
        throw_parse_error("Function already defined");
    }

    Token name = previous();
    consume("Expected '(' after function name", TokenType::LEFT_PAREN);

    // ScopedIntegerManager manager{scope_depth};

    std::vector<std::pair<Token, TypeNode>> params{};
    if (peek().type != TokenType::RIGHT_PAREN) {
        do {
            advance();
            Token parameter_name = previous();
            consume("Expected ':' after function parameter name", TokenType::COLON);
            TypeNode parameter_type = type();
            params.emplace_back(std::move(parameter_name), std::move(parameter_type));
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

    ScopedBooleanManager function_manager{in_function};
    StmtNode body = block_statement();

    auto *function_definition =
        allocate_node(FunctionStmt, std::move(name), std::move(return_type), std::move(params), std::move(body), {}, 0);

    if (!in_class && scope_depth == 0) {
        current_module.functions[function_definition->name.lexeme] = function_definition;
    }

    return StmtNode{function_definition};
}

void recursively_change_module_depth(std::pair<Module, std::size_t> &module, std::size_t value) {
    module.second = value;
    for (std::size_t imported : module.first.imported) {
        recursively_change_module_depth(Parser::parsed_modules[imported], value + 1);
    }
}

StmtNode Parser::import_statement() {
    Token keyword = previous();
    consume("Expected path to module after 'import' keyword", TokenType::STRING_VALUE);
    Token imported = previous();
    consume("Expected ';' or newline after imported file", previous(), TokenType::SEMICOLON, TokenType::END_OF_LINE);

    std::string imported_dir = imported.lexeme[0] == '/' ? "" : current_module.module_directory;

    std::ifstream module{imported_dir + imported.lexeme, std::ios::in};
    std::size_t name_index = imported.lexeme.find_last_of('/');
    std::string module_name = imported.lexeme.substr(name_index != std::string::npos ? name_index + 1 : 0);
    if (!module.is_open()) {
        std::string message = "Unable to open module '";
        message += module_name;
        message += "'";
        error(message, imported);
        return {nullptr};
    }

    if (module_name == current_module.name) {
        error("Cannot import module with the same name as the current one", imported);
    }

    std::string module_source{std::istreambuf_iterator<char>{module}, std::istreambuf_iterator<char>{}};
    Module imported_module{module_name, imported_dir};
    std::string_view logger_source{logger.source};
    std::string_view logger_module_name{logger.module_name};

    auto it = std::find_if(parsed_modules.begin(), parsed_modules.end(),
        [&module_name](const std::pair<Module, std::size_t> &pair) { return module_name == pair.first.name; });

    // Avoid parsing a module if its already imported
    if (it != parsed_modules.end()) {
        if (it->second < (current_module_depth + 1)) {
            recursively_change_module_depth(*it, current_module_depth + 1);
        }
        current_module.imported.push_back(it - parsed_modules.begin());
        return {nullptr};
    }

    try {
        logger.set_source(module_source);
        logger.set_module_name(module_name);
        Scanner scanner{module_source};
        Parser parser{scanner.scan(), imported_module, current_module_depth + 1};
        imported_module.statements = parser.program();
        TypeResolver resolver{imported_module};
        resolver.check(imported_module.statements);
    } catch (const ParseException &) {}

    logger.set_source(logger_source);
    logger.set_module_name(logger_module_name);
    parsed_modules.emplace_back(std::move(imported_module), current_module_depth + 1);
    current_module.imported.push_back(parsed_modules.size() - 1);
    return {nullptr};
}

StmtNode Parser::type_declaration() {
    consume("Expected type name after 'type' keyword", previous(), TokenType::IDENTIFIER);
    Token name = previous();
    consume("Expected '=' after type name", TokenType::EQUAL);
    TypeNode aliased = type();
    consume("Expected ';' or newline after type alias", TokenType::SEMICOLON, TokenType::END_OF_LINE);
    return StmtNode{allocate_node(TypeStmt, std::move(name), std::move(aliased))};
}

StmtNode Parser::variable_declaration() {
    std::string message = "Expected variable name after 'var' keyword";
    if (previous().type == TokenType::VAL) {
        message[32] = 'l'; // The index of 'r' in the word 'var' in the above string
    }
    TokenType keyword = previous().type;
    consume(message, previous(), TokenType::IDENTIFIER);
    Token name = previous();

    TypeNode var_type = match(TokenType::COLON) ? type() : nullptr;
    ExprNode initializer = match(TokenType::EQUAL) ? expression() : nullptr;
    consume("Expected ';' or newline after variable initializer", TokenType::SEMICOLON, TokenType::END_OF_LINE);

    auto *variable = allocate_node(VarStmt, (keyword == TokenType::VAL), std::move(name), std::move(var_type),
        std::move(initializer), NumericConversionType::NONE, false, false);
    return StmtNode{variable};
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
    ScopedIntegerManager manager{scope_depth};

    while (!is_at_end() && peek().type != TokenType::RIGHT_BRACE) {
        if (match(TokenType::VAR, TokenType::VAL)) {
            statements.emplace_back(variable_declaration());
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
    if (!condition) {
        throw_parse_error(error_message);
    }
    Token keyword = previous();
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
    Token keyword = previous();
    consume("Expected '(' after 'for' keyword", TokenType::LEFT_PAREN);
    ScopedIntegerManager manager{scope_depth};

    StmtNode initializer = nullptr;
    if (match(TokenType::VAR, TokenType::VAL)) {
        initializer = variable_declaration();
    } else if (!match(TokenType::SEMICOLON)) {
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

    ScopedBooleanManager loop_manager{in_loop};
    //    auto *modified_body = allocate_node(BlockStmt, {});

    //    ScopedIntegerManager manager2{scope_depth};
    //    modified_body->stmts.emplace_back(statement()); // The actual body
    //    modified_body->stmts.emplace_back(std::move(increment));

    StmtNode desugared_loop =
        StmtNode{allocate_node(WhileStmt, std::move(keyword), std::move(condition), statement(), std::move(increment))};
    // The increment is only created for for-loops, so that the `continue` statement works properly.

    auto *loop = allocate_node(BlockStmt, {});
    loop->stmts.emplace_back(std::move(initializer));
    loop->stmts.emplace_back(std::move(desugared_loop));

    return StmtNode{loop};
}

StmtNode Parser::if_statement() {
    Token keyword = previous();
    ExprNode condition = expression();
    StmtNode then_branch = statement();
    if (match(TokenType::ELSE)) {
        StmtNode else_branch = statement();
        return StmtNode{allocate_node(
            IfStmt, std::move(keyword), std::move(condition), std::move(then_branch), std::move(else_branch))};
    } else {
        return StmtNode{
            allocate_node(IfStmt, std::move(keyword), std::move(condition), std::move(then_branch), nullptr)};
    }
}

StmtNode Parser::return_statement() {
    if (!in_function) {
        throw_parse_error("Cannot use 'return' keyword outside a function");
    }

    Token keyword = previous();

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
    std::vector<std::pair<ExprNode, StmtNode>> cases{};
    StmtNode default_case = nullptr;
    consume("Expected '{' after switch statement condition", TokenType::LEFT_BRACE);

    ScopedBooleanManager switch_manager{in_switch};

    while (!is_at_end() && peek().type != TokenType::RIGHT_BRACE) {
        if (match(TokenType::CASE)) {
            ExprNode expr = expression();
            consume("Expected ':' after case expression", TokenType::COLON);

            StmtNode stmt = statement();
            cases.emplace_back(std::move(expr), std::move(stmt));
        } else if (match(TokenType::DEFAULT) && default_case == nullptr) {
            consume("Expected ':' after case expression", TokenType::COLON);
            StmtNode stmt = statement();
            default_case = std::move(stmt);
        } else if (default_case != nullptr) {
            throw_parse_error("Cannot have more than one default cases in a switch");
        } else {
            throw_parse_error("Expected either 'case' or 'default' as start of switch statement cases");
        }
    }

    consume("Expected '}' at the end of switch statement", TokenType::RIGHT_BRACE);
    return StmtNode{allocate_node(SwitchStmt, std::move(condition), std::move(cases), std::move(default_case))};
}

StmtNode Parser::while_statement() {
    Token keyword = previous();
    // ScopedIntegerManager manager{scope_depth};

    ExprNode condition = expression();

    while (peek().type == TokenType::END_OF_LINE) {
        advance();
    }

    ScopedBooleanManager loop_manager{in_loop};
    StmtNode body = statement();

    return StmtNode{allocate_node(WhileStmt, std::move(keyword), std::move(condition), std::move(body), nullptr)};
}
