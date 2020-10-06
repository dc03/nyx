/* See LICENSE at project root for license details */
#include <algorithm>
#include <functional>
#include <stdexcept>
#include <utility>

#include "../ErrorLogger/ErrorLogger.hpp"
#include "Parser.hpp"

struct ParseException : public std::invalid_argument {
    Token token{};
    explicit ParseException(Token token, const std::string_view error)
            : std::invalid_argument(std::string{error.begin(), error.end()}), token{std::move(token)} {}
};

void Parser::add_rule(TokenType type, ParseRule rule) noexcept {
    rules[static_cast<std::size_t>(type)] = rule;
}

[[nodiscard]] constexpr const typename Parser::ParseRule &
Parser::get_rule(TokenType type) const noexcept {
    return rules[static_cast<std::size_t>(type)];
}

void Parser::sync_and_throw(const std::string_view message) {
    const Token &erroneous = previous();
    error(message, erroneous);
    synchronize();
    throw ParseException{erroneous, message};
}

void Parser::synchronize() {
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

Parser::Parser(const std::vector<Token> &tokens, std::vector<ClassStmt*> &classes,
                      std::vector<FunctionStmt*> &functions, std::vector<VarStmt*> &globals):
        tokens{tokens}, classes{classes}, functions{functions}, globals{globals} {
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
    add_rule(TokenType::FLOAT,         {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::FN,            {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::FOR,           {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::IF,            {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::IMPORT,        {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::INT,           {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::NULL_,         {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::OR,            {nullptr, &Parser::or_, ParsePrecedence::NONE});
    add_rule(TokenType::PROTECTED,     {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::PRIVATE,       {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::PUBLIC,        {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::REF,           {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::RETURN,        {nullptr, nullptr, ParsePrecedence::NONE});
    add_rule(TokenType::STRING,        {nullptr, nullptr, ParsePrecedence::NONE});
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

template <typename ... Args>
bool Parser::match(Args ... args) {
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

template <typename ... Args>
void Parser::consume(const std::string_view message, Args ... args) {
    if (!match(args...)) {
        error(message, peek());
        synchronize();
        throw ParseException{peek(), message};
    }
}

template <typename ... Args>
void Parser::consume(std::string_view message, const Token &where, Args ... args) {
    if (!match(args...)) {
        error(message, where);
        synchronize();
        throw ParseException{where, message};
    }
}

std::vector<stmt_node_t> Parser::program() {
    std::vector<stmt_node_t> statements;

    while (peek().type != TokenType::END_OF_FILE) {
        try {
            statements.emplace_back(declaration());
        } catch (...) {}
    }

    consume("Expected EOF at the end of file", TokenType::END_OF_FILE);
    return statements;
}

expr_node_t Parser::parse_precedence(ParsePrecedence::of precedence) {
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
    expr_node_t left = std::invoke(prefix, this, can_assign);

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

expr_node_t Parser::expression() {
    return parse_precedence(ParsePrecedence::ASSIGNMENT);
}

expr_node_t Parser::grouping(bool) {
    expr_node_t expr = expression();
    consume("Expected ')' after parenthesized expression", TokenType::RIGHT_PAREN);
    return expr_node_t{allocate(GroupingExpr, std::move(expr))};
}

expr_node_t Parser::literal(bool) {
    switch (previous().type) {
        case TokenType::INT_VALUE: {
            int value = std::stoi(previous().lexeme);
            return expr_node_t{allocate(LiteralExpr, value)};
        }
        case TokenType::FLOAT_VALUE: {
            double value = std::stod(previous().lexeme);
            return expr_node_t{allocate(LiteralExpr, value)};
        }
        case TokenType::STRING_VALUE:
            return expr_node_t{allocate(LiteralExpr, previous().lexeme)};
        case TokenType::FALSE:
            return expr_node_t{allocate(LiteralExpr, false)};
        case TokenType::TRUE:
            return expr_node_t{allocate(LiteralExpr, true)};
        case TokenType::NULL_:
            return expr_node_t{allocate(LiteralExpr, nullptr)};
        default:
            throw ParseException{previous(), "Unexpected TokenType passed to literal parser"};
    }
}


expr_node_t Parser::super(bool) {
    if (!(in_class && in_function)) {
        sync_and_throw("Cannot use super expression outside a class");
    }
    Token super = previous();
    consume("Expected '.' after 'super' keyword", TokenType::DOT);
    consume("Expected name after '.' in super expression", TokenType::IDENTIFIER);
    Token name = previous();
    return expr_node_t{allocate(SuperExpr, std::move(super), std::move(name))};
}

expr_node_t Parser::this_expr(bool) {
    if (!(in_class && in_function)) {
        sync_and_throw("Cannot use this keyword outside a class");
    }
    Token keyword = previous();
    return expr_node_t{allocate(ThisExpr, std::move(keyword))};
}

expr_node_t Parser::unary(bool) {
    expr_node_t expr = parse_precedence(get_rule(previous().type).precedence);
    return expr_node_t{allocate(UnaryExpr, previous(), std::move(expr))};
}

expr_node_t Parser::variable(bool can_assign) {
    if (can_assign && match(TokenType::EQUAL, TokenType::PLUS_EQUAL, TokenType::MINUS_EQUAL, TokenType::STAR_EQUAL,
                            TokenType::SLASH_EQUAL)) {
        expr_node_t value = expression();
        return expr_node_t{allocate(AssignExpr, previous(), std::move(value))};
    } else {
        return expr_node_t{allocate(VariableExpr, previous())};
    }
}

expr_node_t Parser::and_(bool, expr_node_t left) {
    expr_node_t right = parse_precedence(ParsePrecedence::LOGIC_AND);
    return expr_node_t{allocate(LogicalExpr, std::move(left), previous(), std::move(right))};
}

expr_node_t Parser::binary(bool, expr_node_t left) {
    expr_node_t right = parse_precedence(ParsePrecedence::of(get_rule(previous().type).precedence + 1));
    return expr_node_t{allocate(BinaryExpr, std::move(left), previous(), std::move(right))};
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
    return expr_node_t{allocate(CallExpr, std::move(function), std::move(paren), std::move(args))};
}


expr_node_t Parser::comma(bool, expr_node_t left) {
    std::vector<expr_node_t> exprs{};
    exprs.emplace_back(std::move(left));
    do {
        exprs.emplace_back(parse_precedence(ParsePrecedence::ASSIGNMENT));
    } while (match(TokenType::COMMA));

    return expr_node_t{allocate(CommaExpr, std::move(exprs))};
}

expr_node_t Parser::dot(bool can_assign, expr_node_t left) {
    consume("Expected identifier after '.'", TokenType::IDENTIFIER);
    Token name = previous();
    if (can_assign && match(TokenType::EQUAL, TokenType::PLUS_EQUAL, TokenType::MINUS_EQUAL, TokenType::STAR_EQUAL,
                            TokenType::SLASH_EQUAL)) {
        expr_node_t value = expression();
        return expr_node_t{allocate(SetExpr, std::move(left), std::move(name), std::move(value))};
    }  else {
        return expr_node_t{allocate(GetExpr, std::move(left), std::move(name))};
    }
}

expr_node_t Parser::index(bool, expr_node_t object) {
    expr_node_t index = expression();
    consume("Expected ']' after array subscript index", TokenType::RIGHT_INDEX);
    return expr_node_t{allocate(IndexExpr, std::move(object), previous(), std::move(index))};
}

expr_node_t Parser::or_(bool, expr_node_t left) {
    expr_node_t right = parse_precedence(ParsePrecedence::LOGIC_OR);
    return expr_node_t{allocate(LogicalExpr, std::move(left), previous(), std::move(right))};
}

expr_node_t Parser::ternary(bool, expr_node_t left) {
    Token question = previous();
    expr_node_t middle = parse_precedence(ParsePrecedence::LOGIC_OR);
    consume("Expected colon in ternary expression", TokenType::COLON);
    expr_node_t right = parse_precedence(ParsePrecedence::LOGIC_OR);
    return expr_node_t{allocate(TernaryExpr, std::move(left), question, std::move(middle), std::move(right))};
}

////////////////////////////////////////////////////////////////////////////////

type_node_t Parser::type() {
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
        return type_node_t{allocate(UserDefinedType, data, std::move(name))};
    } else if (type == TypeType::LIST) {
        return list_type(is_const, is_ref);
    } else if (type == TypeType::TYPEOF) {
        return type_node_t{allocate(TypeofType, expression())};
    } else {
        return type_node_t{allocate(PrimitiveType, data)};
    }
}

type_node_t Parser::list_type(bool is_const, bool is_ref) {
    type_node_t contained = type();
    consume("Expected ',' after contained type of array", TokenType::COMMA);
    expr_node_t size = expression();
    consume("Expected ']' after array size", TokenType::RIGHT_INDEX);
    SharedData data{TypeType::LIST, is_const, is_ref};
    return type_node_t{allocate(ListType, data, std::move(contained),
                                std::move(size))};
}

////////////////////////////////////////////////////////////////////////////////

stmt_node_t Parser::declaration() {
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

stmt_node_t Parser::class_declaration() {
    consume("Expected class name after 'class' keyword", TokenType::IDENTIFIER);
    Token name = previous();
    bool has_ctor{false};
    bool has_dtor{false};
    std::vector<std::pair<stmt_node_t,VisibilityType>> members{};
    std::vector<std::pair<stmt_node_t,VisibilityType>> methods{};

    scope_depth++;

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
            stmt_node_t member = variable_declaration();
            members.emplace_back(std::move(member), visibility);
        } else if (match(TokenType::FN)) {
            bool found_dtor = match(TokenType::BIT_NOT);
            if (found_dtor && peek().lexeme != name.lexeme) {
                advance();
                sync_and_throw("The name of the destructor has to be the same as the name of the class");
            }

            stmt_node_t method = function_declaration();
            const Token &method_name = dynamic_cast<FunctionStmt*>(method.get())->name;

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
                    constexpr const std::string_view message =
                            "Cannot declare constructors or destructors more than once";
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

    scope_depth--;

    consume("Expected '}' at the end of class declaration", TokenType::RIGHT_BRACE);
    auto *class_definition = allocate(ClassStmt, std::move(name), has_ctor, has_dtor, std::move(members),
                                      std::move(methods));
    classes.push_back(class_definition);

    return stmt_node_t{class_definition};
}

stmt_node_t Parser::function_declaration() {
    consume("Expected function name after 'fn' keyword", TokenType::IDENTIFIER);
    Token name = previous();

    consume("Expected '(' after function name", TokenType::LEFT_PAREN);
    std::vector<std::pair<Token,type_node_t>> params{};
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
    // not be that easy and just having to do this once in the parser is fine
    while (peek().type == TokenType::END_OF_LINE) {
        advance();
    }

    consume("Expected '->' after ')' to specify type", TokenType::ARROW);
    type_node_t return_type = type();
    consume("Expected '{' after function return type", TokenType::LEFT_BRACE);

    scope_depth++;

    bool in_function_outer = in_function;
    in_function = true;
    stmt_node_t body = block_statement();
    in_function = in_function_outer;

    auto *function_definition = allocate(FunctionStmt, std::move(name), std::move(return_type), std::move(params),
                                         std::move(body));

    scope_depth--;

    if (!in_class && scope_depth == 0) {
        functions.push_back(function_definition);
    }

    return stmt_node_t{function_definition};
}

stmt_node_t Parser::import_statement() {
    consume("Expected a name after 'import' keyword", TokenType::IDENTIFIER);
    Token name = previous();
    consume("Expected ';' or newline after imported file", previous(), TokenType::SEMICOLON, TokenType::END_OF_LINE);
    return stmt_node_t{allocate(ImportStmt, std::move(name))};
}

stmt_node_t Parser::type_declaration() {
    consume("Expected type name after 'type' keyword", previous(), TokenType::IDENTIFIER);
    Token name = previous();
    consume("Expected '=' after type name", TokenType::EQUAL);
    type_node_t aliased = type();
    consume("Expected ';' or newline after type alias", TokenType::SEMICOLON, TokenType::END_OF_LINE);
    return stmt_node_t{allocate(TypeStmt, std::move(name), std::move(aliased))};
}

stmt_node_t Parser::variable_declaration() {
    std::string message = "Expected variable name after 'var' keyword";
    if (previous().type == TokenType::VAL) {
        message[32] = 'l';
    }
    consume(message, previous(), TokenType::IDENTIFIER);
    Token name = previous();
    consume("Expected ':' after variable name", previous(), TokenType::COLON);
    type_node_t var_type = type();
    expr_node_t initializer = [this]() {
        if (match(TokenType::EQUAL)) {
            return expression();
        } else {
            return expr_node_t{nullptr};
        }
    }();
    consume("Expected ';' or newline after variable initializer", TokenType::SEMICOLON, TokenType::END_OF_LINE);

    auto *variable = allocate(VarStmt, std::move(name), std::move(var_type), std::move(initializer));
    if (!(in_class || in_function) && scope_depth == 0) {
        globals.push_back(variable);
    }

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

    scope_depth++;

    while (!is_at_end() && peek().type != TokenType::RIGHT_BRACE) {
        if (match(TokenType::VAR, TokenType::VAL)) {
            statements.emplace_back(variable_declaration());
        } else {
            statements.emplace_back(statement());
        }
    }

    scope_depth--;
    consume("Expected '}' after block", TokenType::RIGHT_BRACE);
    return stmt_node_t{allocate(BlockStmt, std::move(statements))};
}

template <typename Allocated>
stmt_node_t Parser::single_token_statement(const std::string_view token, const bool condition,
                                                  const std::string_view error_message) {
    if (!condition) {
        sync_and_throw(error_message);
    }
    Token keyword = previous();
    using namespace std::string_literals;
    const std::string consume_message = "Expected ';' or newline after "s + &token[0] + " keyword";
    consume(consume_message, TokenType::SEMICOLON, TokenType::END_OF_LINE);
    return stmt_node_t{allocate(Allocated, std::move(keyword))};
}

stmt_node_t Parser::break_statement() {
    return single_token_statement<BreakStmt>("break", (in_loop || in_switch),
                                             "Cannot use 'break' outside a loop or switch.");
}

stmt_node_t Parser::continue_statement() {
    return single_token_statement<ContinueStmt>("continue", in_loop, "Cannot use 'continue' outside a loop or switch");
}

stmt_node_t Parser::expression_statement() {
    expr_node_t expr = expression();
    consume("Expected ';' or newline after expression", TokenType::SEMICOLON, TokenType::END_OF_LINE);
    return stmt_node_t{allocate(ExpressionStmt, std::move(expr))};
}

stmt_node_t Parser::for_statement() {
    consume("Expected '(' after 'for' keyword", TokenType::LEFT_PAREN);
    scope_depth++;

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
        increment = stmt_node_t{allocate(ExpressionStmt, expression())};
    }
    consume("Expected ')' after for loop header", TokenType::RIGHT_PAREN);

    bool in_loop_outer = in_loop;
    in_loop = true;

    auto *modified_body = allocate(BlockStmt, {});

    scope_depth++;
    modified_body->stmts.emplace_back(statement()); // The actual body
    modified_body->stmts.emplace_back(std::move(increment));
    scope_depth--;

    stmt_node_t desugared_loop = stmt_node_t{allocate(WhileStmt, std::move(condition),
                                                      stmt_node_t{modified_body})};

    auto *loop = allocate(BlockStmt, {});
    loop->stmts.emplace_back(std::move(initializer));
    loop->stmts.emplace_back(std::move(desugared_loop));

    in_loop = in_loop_outer;
    scope_depth--;

    return stmt_node_t{loop};
}

stmt_node_t Parser::if_statement() {
    expr_node_t condition = expression();
    stmt_node_t then_branch = statement();
    if (match(TokenType::ELSE)) {
        stmt_node_t else_branch = statement();
        return stmt_node_t{allocate(IfStmt, std::move(condition), std::move(then_branch),
                                    std::move(else_branch))};
    } else {
        return stmt_node_t{allocate(IfStmt, std::move(condition), std::move(then_branch), nullptr)};
    }
}

stmt_node_t Parser::return_statement() {
    if (!in_function) {
        sync_and_throw("Cannot use 'return' keyword outside a function");
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
    return stmt_node_t{allocate(ReturnStmt, std::move(keyword), std::move(return_value))};
}

stmt_node_t Parser::switch_statement() {
    expr_node_t condition = expression();
    std::vector<std::pair<expr_node_t,stmt_node_t>> cases{};
    std::optional<stmt_node_t> default_case = std::nullopt;
    consume("Expected '{' after switch statement condition", TokenType::LEFT_BRACE);
    bool in_switch_outer = in_switch;
    in_switch = true;
    while (!is_at_end() && peek().type != TokenType::RIGHT_BRACE) {
        if (match(TokenType::CASE)) {
            expr_node_t expr = expression();
            consume("Expected ':' after case expression", TokenType::COLON);

            stmt_node_t stmt = statement();
            cases.emplace_back(std::move(expr), std::move(stmt));
        } else if (match(TokenType::DEFAULT) && default_case == std::nullopt) {
            consume("Expected ':' after case expression", TokenType::COLON);
            stmt_node_t stmt = statement();
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
    return stmt_node_t{allocate(SwitchStmt, std::move(condition), std::move(cases), std::move(default_case))};
}

stmt_node_t Parser::while_statement() {
    expr_node_t condition = expression();
    bool in_loop_outer = in_loop;
    in_loop = true;
    stmt_node_t body = statement();
    in_loop = in_loop_outer;
    return stmt_node_t{allocate(WhileStmt, std::move(condition), std::move(body))};
}