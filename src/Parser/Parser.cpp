/* See LICENSE at project root for license details */
#include "Parser.hpp"

#include "../ErrorLogger/ErrorLogger.hpp"
#include "../Module.hpp"
#include "../Scanner/Scanner.hpp"
#include "TypeResolver.hpp"

#include <fstream>
#include <functional>
#include <stdexcept>
#include <utility>

struct ParseException : public std::invalid_argument {
    Token token{};
    explicit ParseException(Token token, const std::string_view error)
        : std::invalid_argument(std::string{error.begin(), error.end()}), token{std::move(token)} {}
};

struct scoped_boolean_manager {
    bool &scoped_bool;
    bool previous_value{};
    explicit scoped_boolean_manager(bool &controlled) : scoped_bool{controlled} {
        previous_value = controlled;
        controlled = true;
    }

    ~scoped_boolean_manager() {
        scoped_bool = previous_value;
    }
};

struct scoped_integer_manager {
    std::size_t &scoped_int;
    explicit scoped_integer_manager(std::size_t &controlled) : scoped_int{controlled} {
        controlled++;
    }
    ~scoped_integer_manager() {
        scoped_int--;
    }
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

Parser::Parser(const std::vector<Token> &tokens, Module &module)
    : tokens{tokens}, current_module{module}, classes{module.classes}, functions{module.functions} {
    // clang-format off
    add_rule(TokenType::COMMA,         {nullptr, &Parser::comma, ParsePrecedence::COMMA});
    add_rule(TokenType::EQUAL,         {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::PLUS_EQUAL,    {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::MINUS_EQUAL,   {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::STAR_EQUAL,    {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::SLASH_EQUAL,   {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::QUESTION,      {nullptr, &Parser::ternary, ParsePrecedence::ASSIGNMENT});
    add_rule(TokenType::COLON,         {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::BIT_OR,        {nullptr, &Parser::binary, ParsePrecedence::BIT_OR});
    add_rule(TokenType::BIT_XOR,       {nullptr, &Parser::binary, ParsePrecedence::BIT_XOR});
    add_rule(TokenType::BIT_AND,       {nullptr, &Parser::binary, ParsePrecedence::BIT_AND});
    add_rule(TokenType::NOT_EQUAL,     {nullptr, &Parser::binary, ParsePrecedence::EQUALITY});
    add_rule(TokenType::EQUAL_EQUAL,   {nullptr, &Parser::binary, ParsePrecedence::EQUALITY});
    add_rule(TokenType::GREATER,       {nullptr, &Parser::binary, ParsePrecedence::ORDERING});
    add_rule(TokenType::GREATER_EQUAL, {nullptr, &Parser::binary, ParsePrecedence::ORDERING});
    add_rule(TokenType::LESS,          {nullptr, &Parser::binary, ParsePrecedence::ORDERING});
    add_rule(TokenType::LESS_EQUAL,    {nullptr, &Parser::binary, ParsePrecedence::ORDERING});
    add_rule(TokenType::RIGHT_SHIFT,   {nullptr, &Parser::binary, ParsePrecedence::SHIFT});
    add_rule(TokenType::LEFT_SHIFT,    {nullptr, &Parser::binary, ParsePrecedence::SHIFT});
    add_rule(TokenType::MINUS,         {&Parser::unary, &Parser::binary, ParsePrecedence::SUM});
    add_rule(TokenType::PLUS,          {&Parser::unary, &Parser::binary, ParsePrecedence::SUM});
    add_rule(TokenType::MODULO,        {nullptr, &Parser::binary, ParsePrecedence::PRODUCT});
    add_rule(TokenType::SLASH,         {nullptr, &Parser::binary, ParsePrecedence::PRODUCT});
    add_rule(TokenType::STAR,          {nullptr, &Parser::binary, ParsePrecedence::PRODUCT});
    add_rule(TokenType::NOT,           {&Parser::unary, nullptr, ParsePrecedence::UNARY});
    add_rule(TokenType::BIT_NOT,       {&Parser::unary, nullptr, ParsePrecedence::UNARY});
    add_rule(TokenType::PLUS_PLUS,     {&Parser::unary, nullptr, ParsePrecedence::UNARY});
    add_rule(TokenType::MINUS_MINUS,   {&Parser::unary, nullptr, ParsePrecedence::UNARY});
    add_rule(TokenType::DOT,           {nullptr, &Parser::dot, ParsePrecedence::CALL});
    add_rule(TokenType::LEFT_PAREN,    {&Parser::grouping, &Parser::call, ParsePrecedence::CALL});
    add_rule(TokenType::RIGHT_PAREN,   {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::LEFT_INDEX,    {nullptr, &Parser::index, ParsePrecedence::CALL});
    add_rule(TokenType::RIGHT_INDEX,   {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::LEFT_BRACE,    {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::RIGHT_BRACE,   {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::SEMICOLON,     {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::ARROW,         {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::IDENTIFIER,    {&Parser::variable, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::STRING_VALUE,  {&Parser::literal, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::INT_VALUE,     {&Parser::literal, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::FLOAT_VALUE,   {&Parser::literal, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::AND,           {nullptr, &Parser::and_, ParsePrecedence::NONE});
    add_rule(TokenType::BREAK,         {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::CASE,          {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::CLASS,         {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::CONST,         {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::CONTINUE,      {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::DEFAULT,       {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::ELSE,          {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::FALSE,         {&Parser::literal, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::FLOAT,         {&Parser::variable, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::FN,            {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::FOR,           {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::IF,            {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::IMPORT,        {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::INT,           {&Parser::variable, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::NULL_,         {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::OR,            {nullptr, &Parser::or_, ParsePrecedence::NONE});
    add_rule(TokenType::PROTECTED,     {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::PRIVATE,       {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::PUBLIC,        {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::REF,           {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::RETURN,        {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::STRING,        {&Parser::variable, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::SUPER,         {&Parser::super, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::SWITCH,        {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::THIS,          {&Parser::this_expr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::TRUE,          {&Parser::literal, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::TYPE,          {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::TYPEOF,        {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::VAL,           {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::VAR,           {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::WHILE,         {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::NONE,          {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::END_OF_LINE,   {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::END_OF_FILE,   {nullptr, nullptr, ParsePrecedence::NONE});
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

std::vector<stmt_node_t> Parser::program() {
    std::vector<stmt_node_t> statements;

    while (peek().type != TokenType::END_OF_FILE && peek().type != TokenType::END_OF_LINE) {
        statements.emplace_back(declaration());
    }

    if (peek().type == TokenType::END_OF_LINE) {
        advance();
    }

    consume("Expected EOF at the end of file", TokenType::END_OF_FILE);
    return statements;
}

expr_node_t Parser::parse_precedence(ParsePrecedence::of precedence) {
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

    bool can_assign = precedence <= ParsePrecedence::ASSIGNMENT;
    expr_node_t left = std::invoke(prefix, this, can_assign);

    while (precedence <= get_rule(peek().type).precedence) {
        ExprInfixParseFn infix = get_rule(advance().type).infix;
        left = std::invoke(infix, this, can_assign, std::move(left));
    }

    if (match(TokenType::EQUAL, TokenType::PLUS_EQUAL, TokenType::MINUS_EQUAL, TokenType::STAR_EQUAL,
            TokenType::SLASH_EQUAL) &&
        can_assign) {
        error("Invalid assignment target", previous());
        throw ParseException{previous(), "Invalid assignment target"};
    }

    return left;
}

expr_node_t Parser::and_(bool, expr_node_t left) {
    Token oper = previous();
    expr_node_t right = parse_precedence(ParsePrecedence::LOGIC_AND);
    return expr_node_t{allocate_node(LogicalExpr, std::move(left), std::move(oper), std::move(right))};
}

expr_node_t Parser::binary(bool, expr_node_t left) {
    Token oper = previous();
    expr_node_t right = parse_precedence(ParsePrecedence::of(get_rule(previous().type).precedence + 1));
    return expr_node_t{allocate_node(BinaryExpr, std::move(left), std::move(oper), std::move(right))};
}

expr_node_t Parser::call(bool, expr_node_t function) {
    Token paren = previous();
    std::vector<expr_node_t> args{};
    if (peek().type != TokenType::RIGHT_PAREN) {
        do {
            args.emplace_back(parse_precedence(ParsePrecedence::ASSIGNMENT));
        } while (match(TokenType::COMMA));
    }
    consume("Expected ')' after function call", TokenType::RIGHT_PAREN);
    return expr_node_t{allocate_node(CallExpr, std::move(function), std::move(paren), std::move(args))};
}

expr_node_t Parser::comma(bool, expr_node_t left) {
    std::vector<expr_node_t> exprs{};
    exprs.emplace_back(std::move(left));
    do {
        exprs.emplace_back(parse_precedence(ParsePrecedence::ASSIGNMENT));
    } while (match(TokenType::COMMA));

    return expr_node_t{allocate_node(CommaExpr, std::move(exprs))};
}

expr_node_t Parser::dot(bool can_assign, expr_node_t left) {
    consume("Expected identifier after '.'", TokenType::IDENTIFIER);
    Token name = previous();
    if (can_assign && match(TokenType::EQUAL, TokenType::PLUS_EQUAL, TokenType::MINUS_EQUAL, TokenType::STAR_EQUAL,
                          TokenType::SLASH_EQUAL)) {
        expr_node_t value = expression();
        return expr_node_t{allocate_node(SetExpr, std::move(left), std::move(name), std::move(value))};
    } else {
        return expr_node_t{allocate_node(GetExpr, std::move(left), std::move(name))};
    }
}

expr_node_t Parser::index(bool, expr_node_t object) {
    Token oper = previous();
    expr_node_t index = expression();
    consume("Expected ']' after array subscript index", TokenType::RIGHT_INDEX);
    return expr_node_t{allocate_node(IndexExpr, std::move(object), std::move(oper), std::move(index))};
}

expr_node_t Parser::or_(bool, expr_node_t left) {
    Token oper = previous();
    expr_node_t right = parse_precedence(ParsePrecedence::LOGIC_OR);
    return expr_node_t{allocate_node(LogicalExpr, std::move(left), std::move(oper), std::move(right))};
}

expr_node_t Parser::expression() {
    return parse_precedence(ParsePrecedence::ASSIGNMENT);
}

expr_node_t Parser::grouping(bool) {
    expr_node_t expr = expression();
    consume("Expected ')' after parenthesized expression", TokenType::RIGHT_PAREN);
    return expr_node_t{allocate_node(GroupingExpr, std::move(expr))};
}

expr_node_t Parser::literal(bool) {
    type_node_t type = type_node_t{allocate_node(PrimitiveType, SharedData{Type::INT, true, false})};
    switch (previous().type) {
        case TokenType::INT_VALUE: {
            int value = std::stoi(previous().lexeme);
            return expr_node_t{allocate_node(LiteralExpr, value, previous(), std::move(type))};
        }
        case TokenType::FLOAT_VALUE: {
            double value = std::stod(previous().lexeme);
            type->data.type = Type::FLOAT;
            return expr_node_t{allocate_node(LiteralExpr, value, previous(), std::move(type))};
        }
        case TokenType::STRING_VALUE: {
            type->data.type = Type::STRING;
            return expr_node_t{allocate_node(LiteralExpr, previous().lexeme, previous(), std::move(type))};
        }
        case TokenType::FALSE: {
            type->data.type = Type::BOOL;
            return expr_node_t{allocate_node(LiteralExpr, false, previous(), std::move(type))};
        }
        case TokenType::TRUE: {
            type->data.type = Type::BOOL;
            return expr_node_t{allocate_node(LiteralExpr, true, previous(), std::move(type))};
        }
        case TokenType::NULL_: {
            type->data.type = Type::NULL_;
            return expr_node_t{allocate_node(LiteralExpr, nullptr, previous(), std::move(type))};
        }

        default: throw ParseException{previous(), "Unexpected TokenType passed to literal parser"};
    }
}

expr_node_t Parser::super(bool) {
    if (!(in_class && in_function)) {
        throw_parse_error("Cannot use super expression outside a class");
    }
    Token super = previous();
    consume("Expected '.' after 'super' keyword", TokenType::DOT);
    consume("Expected name after '.' in super expression", TokenType::IDENTIFIER);
    Token name = previous();
    return expr_node_t{allocate_node(SuperExpr, std::move(super), std::move(name))};
}

expr_node_t Parser::this_expr(bool) {
    if (!(in_class && in_function)) {
        throw_parse_error("Cannot use this keyword outside a class");
    }
    Token keyword = previous();
    return expr_node_t{allocate_node(ThisExpr, std::move(keyword))};
}

expr_node_t Parser::unary(bool) {
    Token oper = previous();
    expr_node_t expr = parse_precedence(get_rule(previous().type).precedence);
    return expr_node_t{allocate_node(UnaryExpr, std::move(oper), std::move(expr))};
}

expr_node_t Parser::variable(bool can_assign) {
    Token name = previous();
    if (can_assign && match(TokenType::EQUAL, TokenType::PLUS_EQUAL, TokenType::MINUS_EQUAL, TokenType::STAR_EQUAL,
                          TokenType::SLASH_EQUAL)) {
        expr_node_t value = expression();
        return expr_node_t{allocate_node(AssignExpr, std::move(name), std::move(value))};
    } else if (match(TokenType::DOUBLE_COLON)) {
        consume("Expected name to access after module name", TokenType::IDENTIFIER);
        Token accessed = previous();
        return expr_node_t{allocate_node(AccessExpr, std::move(name), std::move(accessed))};
    } else {
        return expr_node_t{allocate_node(VariableExpr, std::move(name), 0)};
    }
}

expr_node_t Parser::ternary(bool, expr_node_t left) {
    Token question = previous();
    expr_node_t middle = parse_precedence(ParsePrecedence::LOGIC_OR);
    consume("Expected colon in ternary expression", TokenType::COLON);
    expr_node_t right = parse_precedence(ParsePrecedence::LOGIC_OR);
    return expr_node_t{allocate_node(TernaryExpr, std::move(left), question, std::move(middle), std::move(right))};
}

////////////////////////////////////////////////////////////////////////////////

type_node_t Parser::type() {
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
        return type_node_t{allocate_node(UserDefinedType, data, std::move(name))};
    } else if (type == Type::LIST) {
        return list_type(is_const, is_ref);
    } else if (type == Type::TYPEOF) {
        return type_node_t{allocate_node(TypeofType, data, expression())};
    } else {
        return type_node_t{allocate_node(PrimitiveType, data)};
    }
}

type_node_t Parser::list_type(bool is_const, bool is_ref) {
    type_node_t contained = type();
    expr_node_t size = match(TokenType::COMMA) ? expression() : nullptr;
    consume("Expected ']' after array declaration", TokenType::RIGHT_INDEX);
    SharedData data{Type::LIST, is_const, is_ref};
    return type_node_t{allocate_node(ListType, data, std::move(contained), std::move(size))};
}

////////////////////////////////////////////////////////////////////////////////

stmt_node_t Parser::declaration() {
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

stmt_node_t Parser::class_declaration() {
    consume("Expected class name after 'class' keyword", TokenType::IDENTIFIER);

    for (auto *class_ : classes) {
        if (class_->name.lexeme == previous().lexeme) {
            throw_parse_error("Class already defined");
        }
    }

    Token name = previous();
    bool has_ctor{false};
    bool has_dtor{false};
    std::vector<std::pair<stmt_node_t, VisibilityType>> members{};
    std::vector<std::pair<stmt_node_t, VisibilityType>> methods{};

    scoped_integer_manager depth_manager{scope_depth};

    consume("Expected '{' after class name", TokenType::LEFT_BRACE);
    scoped_boolean_manager class_manager{in_class};
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
            stmt_node_t member = variable_declaration();
            members.emplace_back(std::move(member), visibility);
        } else if (match(TokenType::FN)) {
            bool found_dtor = match(TokenType::BIT_NOT);
            if (found_dtor && peek().lexeme != name.lexeme) {
                advance();
                throw_parse_error("The name of the destructor has to be the same as the name of the class");
            }

            stmt_node_t method = function_declaration();
            const Token &method_name = dynamic_cast<FunctionStmt *>(method.get())->name;

            if (method_name.lexeme == name.lexeme) {
                if (found_dtor && !has_dtor) {
                    has_dtor = true;
                    switch (visibility) {
                        case VisibilityType::PUBLIC: visibility = VisibilityType::PUBLIC_DTOR; break;
                        case VisibilityType::PRIVATE: visibility = VisibilityType::PRIVATE_DTOR; break;
                        case VisibilityType::PROTECTED: visibility = VisibilityType::PROTECTED_DTOR; break;

                        default: break;
                    }
                } else if (!has_ctor) {
                    has_ctor = true;
                } else {
                    constexpr const std::string_view message =
                        "Cannot declare constructors or destructors more than once";
                    error(message, method_name);
                    throw ParseException{method_name, message};
                }
            }
            methods.emplace_back(std::move(method), visibility);
        } else {
            throw_parse_error("Expected either member or method declaration in class");
        }
    }

    consume("Expected '}' at the end of class declaration", TokenType::RIGHT_BRACE);
    auto *class_definition =
        allocate_node(ClassStmt, std::move(name), has_ctor, has_dtor, std::move(members), std::move(methods));
    classes.push_back(class_definition);

    return stmt_node_t{class_definition};
}

stmt_node_t Parser::function_declaration() {
    consume("Expected function name after 'fn' keyword", TokenType::IDENTIFIER);

    for (auto *func : functions) {
        if (func->name.lexeme == previous().lexeme) {
            throw_parse_error("Function already defined");
        }
    }

    Token name = previous();
    consume("Expected '(' after function name", TokenType::LEFT_PAREN);

    scoped_integer_manager manager{scope_depth};

    std::vector<std::pair<Token, type_node_t>> params{};
    if (peek().type != TokenType::RIGHT_PAREN) {
        do {
            advance();
            Token parameter_name = previous();
            consume("Expected ':' after function parameter name", TokenType::COLON);
            type_node_t parameter_type = type();
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
    type_node_t return_type = type();
    consume("Expected '{' after function return type", TokenType::LEFT_BRACE);

    scoped_boolean_manager function_manager{in_function};
    stmt_node_t body = block_statement();

    auto *function_definition =
        allocate_node(FunctionStmt, std::move(name), std::move(return_type), std::move(params), std::move(body));

    if (!in_class && scope_depth == 1) {
        functions.push_back(function_definition);
    }

    return stmt_node_t{function_definition};
}

stmt_node_t Parser::import_statement() {
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

    try {
        logger.set_source(module_source);
        logger.set_module_name(module_name);
        Scanner scanner{module_source};
        Parser parser{scanner.scan(), imported_module};
        imported_module.statements = parser.program();
        TypeResolver resolver{imported_module};
        resolver.check(imported_module.statements);
    } catch (const ParseException &) {}

    logger.set_source(logger_source);
    logger.set_module_name(logger_module_name);
    current_module.imported.emplace_back(std::move(imported_module));
    return {nullptr};
}

stmt_node_t Parser::type_declaration() {
    consume("Expected type name after 'type' keyword", previous(), TokenType::IDENTIFIER);
    Token name = previous();
    consume("Expected '=' after type name", TokenType::EQUAL);
    type_node_t aliased = type();
    consume("Expected ';' or newline after type alias", TokenType::SEMICOLON, TokenType::END_OF_LINE);
    return stmt_node_t{allocate_node(TypeStmt, std::move(name), std::move(aliased))};
}

stmt_node_t Parser::variable_declaration() {
    std::string message = "Expected variable name after 'var' keyword";
    if (previous().type == TokenType::VAL) {
        message[32] = 'l';
    }
    TokenType keyword = previous().type;
    consume(message, previous(), TokenType::IDENTIFIER);
    Token name = previous();

    type_node_t var_type = match(TokenType::COLON) ? type() : nullptr;
    expr_node_t initializer = match(TokenType::EQUAL) ? expression() : nullptr;
    consume("Expected ';' or newline after variable initializer", TokenType::SEMICOLON, TokenType::END_OF_LINE);

    if (var_type != nullptr && keyword == TokenType::VAL) {
        var_type.get()->data.is_const = true;
    }

    auto *variable = allocate_node(VarStmt, std::move(name), std::move(var_type), std::move(initializer));
    return stmt_node_t{variable};
}

stmt_node_t Parser::statement() {
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

stmt_node_t Parser::block_statement() {
    std::vector<stmt_node_t> statements{};
    scoped_integer_manager manager{scope_depth};

    while (!is_at_end() && peek().type != TokenType::RIGHT_BRACE) {
        if (match(TokenType::VAR, TokenType::VAL)) {
            statements.emplace_back(variable_declaration());
        } else {
            statements.emplace_back(statement());
        }
    }

    consume("Expected '}' after block", TokenType::RIGHT_BRACE);
    return stmt_node_t{allocate_node(BlockStmt, std::move(statements))};
}

template <typename Allocated>
stmt_node_t Parser::single_token_statement(
    const std::string_view token, const bool condition, const std::string_view error_message) {
    if (!condition) {
        throw_parse_error(error_message);
    }
    Token keyword = previous();
    using namespace std::string_literals;
    const std::string consume_message = "Expected ';' or newline after "s + &token[0] + " keyword";
    consume(consume_message, TokenType::SEMICOLON, TokenType::END_OF_LINE);
    return stmt_node_t{allocate_node(Allocated, std::move(keyword))};
}

stmt_node_t Parser::break_statement() {
    return single_token_statement<BreakStmt>(
        "break", (in_loop || in_switch), "Cannot use 'break' outside a loop or switch.");
}

stmt_node_t Parser::continue_statement() {
    return single_token_statement<ContinueStmt>("continue", in_loop, "Cannot use 'continue' outside a loop");
}

stmt_node_t Parser::expression_statement() {
    expr_node_t expr = expression();
    consume("Expected ';' or newline after expression", TokenType::SEMICOLON, TokenType::END_OF_LINE);
    return stmt_node_t{allocate_node(ExpressionStmt, std::move(expr))};
}

stmt_node_t Parser::for_statement() {
    Token keyword = previous();
    consume("Expected '(' after 'for' keyword", TokenType::LEFT_PAREN);
    scoped_integer_manager manager{scope_depth};

    stmt_node_t initializer = nullptr;
    if (match(TokenType::VAR, TokenType::VAL)) {
        initializer = variable_declaration();
    } else if (!match(TokenType::SEMICOLON)) {
        initializer = expression_statement();
    }

    expr_node_t condition = nullptr;
    if (peek().type != TokenType::SEMICOLON) {
        condition = expression();
    }
    consume("Expected ';' after loop condition", TokenType::SEMICOLON);

    stmt_node_t increment = nullptr;
    if (peek().type != TokenType::RIGHT_PAREN) {
        increment = stmt_node_t{allocate_node(ExpressionStmt, expression())};
    }
    consume("Expected ')' after for loop header", TokenType::RIGHT_PAREN);

    while (peek().type == TokenType::END_OF_LINE) {
        advance();
    }

    scoped_boolean_manager loop_manager{in_loop};
    auto *modified_body = allocate_node(BlockStmt, {});

    scoped_integer_manager manager2{scope_depth};
    modified_body->stmts.emplace_back(statement()); // The actual body
    modified_body->stmts.emplace_back(std::move(increment));

    stmt_node_t desugared_loop =
        stmt_node_t{allocate_node(WhileStmt, std::move(keyword), std::move(condition), stmt_node_t{modified_body})};

    auto *loop = allocate_node(BlockStmt, {});
    loop->stmts.emplace_back(std::move(initializer));
    loop->stmts.emplace_back(std::move(desugared_loop));

    return stmt_node_t{loop};
}

stmt_node_t Parser::if_statement() {
    expr_node_t condition = expression();
    stmt_node_t then_branch = statement();
    if (match(TokenType::ELSE)) {
        stmt_node_t else_branch = statement();
        return stmt_node_t{allocate_node(IfStmt, std::move(condition), std::move(then_branch), std::move(else_branch))};
    } else {
        return stmt_node_t{allocate_node(IfStmt, std::move(condition), std::move(then_branch), nullptr)};
    }
}

stmt_node_t Parser::return_statement() {
    if (!in_function) {
        throw_parse_error("Cannot use 'return' keyword outside a function");
    }

    Token keyword = previous();

    expr_node_t return_value = [this]() {
        if (peek().type != TokenType::SEMICOLON && peek().type != TokenType::END_OF_LINE) {
            return expression();
        } else {
            return expr_node_t{nullptr};
        }
    }();

    consume("Expected ';' or newline after return statement", TokenType::SEMICOLON, TokenType::END_OF_LINE);
    return stmt_node_t{allocate_node(ReturnStmt, std::move(keyword), std::move(return_value))};
}

stmt_node_t Parser::switch_statement() {
    expr_node_t condition = expression();
    std::vector<std::pair<expr_node_t, stmt_node_t>> cases{};
    stmt_node_t default_case = nullptr;
    consume("Expected '{' after switch statement condition", TokenType::LEFT_BRACE);

    scoped_boolean_manager switch_manager{in_switch};

    while (!is_at_end() && peek().type != TokenType::RIGHT_BRACE) {
        if (match(TokenType::CASE)) {
            expr_node_t expr = expression();
            consume("Expected ':' after case expression", TokenType::COLON);

            stmt_node_t stmt = statement();
            cases.emplace_back(std::move(expr), std::move(stmt));
        } else if (match(TokenType::DEFAULT) && default_case == nullptr) {
            consume("Expected ':' after case expression", TokenType::COLON);
            stmt_node_t stmt = statement();
            default_case = std::move(stmt);
        } else if (default_case != nullptr) {
            throw_parse_error("Cannot have more than one default cases in a switch");
        } else {
            throw_parse_error("Expected either 'case' or 'default' as start of switch statement cases");
        }
    }

    consume("Expected '}' at the end of switch statement", TokenType::RIGHT_BRACE);
    return stmt_node_t{allocate_node(SwitchStmt, std::move(condition), std::move(cases), std::move(default_case))};
}

stmt_node_t Parser::while_statement() {
    Token keyword = previous();
    scoped_integer_manager manager{scope_depth};

    expr_node_t condition = expression();

    while (peek().type == TokenType::END_OF_LINE) {
        advance();
    }

    scoped_boolean_manager loop_manager{in_loop};
    stmt_node_t body = statement();

    return stmt_node_t{allocate_node(WhileStmt, std::move(keyword), std::move(condition), std::move(body))};
}
