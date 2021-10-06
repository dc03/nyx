/* Copyright (C) 2021  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "Frontend/Scanner/Scanner.hpp"

#include "Common.hpp"
#include "ErrorLogger/ErrorLogger.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>

Scanner::Scanner() {
    const char *words[]{"and", "bool", "break", "class", "const", "continue", "default", "else", "false", "float", "fn",
        "for", "if", "import", "int", "move", "null", "not", "or", "protected", "private", "public", "ref", "return",
        "string", "super", "switch", "this", "true", "type", "typeof", "var", "while"};

    TokenType types[]{TokenType::AND, TokenType::BOOL, TokenType::BREAK, TokenType::CLASS, TokenType::CONST,
        TokenType::CONTINUE, TokenType::DEFAULT, TokenType::ELSE, TokenType::FALSE, TokenType::FLOAT, TokenType::FN,
        TokenType::FOR, TokenType::IF, TokenType::IMPORT, TokenType::INT, TokenType::MOVE, TokenType::NULL_,
        TokenType::NOT, TokenType::OR, TokenType::PROTECTED, TokenType::PRIVATE, TokenType::PUBLIC, TokenType::REF,
        TokenType::RETURN, TokenType::STRING, TokenType::SUPER, TokenType::SWITCH, TokenType::THIS, TokenType::TRUE,
        TokenType::TYPE, TokenType::TYPEOF, TokenType::VAR, TokenType::WHILE};

    static_assert(std::size(words) == std::size(types), "Size of array of keywords and their types have to be same.");

    for (std::size_t i{0}; i < std::size(words); i++) {
        keywords.insert(words[i], types[i]);
    }
}

Scanner::Scanner(const std::string_view source) : Scanner() {
    this->source = source;
}

bool Scanner::is_at_end() const noexcept {
    return current >= source.length();
}

char Scanner::advance() {
    if (is_at_end()) {
        return '\0';
    }

    current++;
    return source[current - 1];
}

[[nodiscard]] char Scanner::peek() const noexcept {
    return source[current];
}

[[nodiscard]] char Scanner::peek_next() const noexcept {
    if (current + 1 >= source.size()) {
        return '\0';
    }

    return source[current + 1];
}

[[nodiscard]] const Token &Scanner::previous() const noexcept {
    assert(not tokens.empty() && "Bug in scanner.");
    return tokens[tokens.size() - 1];
}

bool Scanner::match(const char ch) {
    if (not is_at_end() && peek() == ch) {
        advance();
        return true;
    }

    return false;
}

void Scanner::add_token(const TokenType type) {
    tokens.push_back(Token{type, std::string{source.substr(start, (current - start))}, line, start, current});
}

void Scanner::number() {
    TokenType type = TokenType::INT_VALUE;
    while (not is_at_end() && std::isdigit(peek())) {
        advance();
    }

    if (peek() == '.' && std::isdigit(peek_next())) {
        type = TokenType::FLOAT_VALUE;
        advance();
        while (not is_at_end() && std::isdigit(peek())) {
            advance();
        }
    }

    if (peek() == 'e' && std::isdigit(peek_next())) {
        type = TokenType::FLOAT_VALUE;
        advance();
        while (not is_at_end() && std::isdigit(peek())) {
            advance();
        }
    }

    add_token(type);
}

void Scanner::identifier() {
    while (not is_at_end() && (std::isalnum(peek()) || peek() == '_')) {
        advance();
    }

    std::string_view lexeme = source.substr(start, (current - start));
    if (TokenType type{keywords.search(lexeme)}; type != TokenType::NONE) {
        add_token(type);
    } else {
        add_token(TokenType::IDENTIFIER);
    }
}

void Scanner::string(const char delimiter) {
    std::string lexeme{}; // I think this should be fine because of SSO leading to an allocation only after ~24 chars.
    while (not is_at_end() && peek() != delimiter) {
        if (peek() == '\n') {
            line++;
            advance();
        } else if (match('\\')) {
            if (match('b')) {
                lexeme += '\b';
            } else if (match('n')) {
                lexeme += '\n';
            } else if (match('r')) {
                lexeme += '\r';
            } else if (match('t')) {
                lexeme += '\t';
            } else if (match('\\')) {
                lexeme += '\\';
            } else if (match('\'')) {
                lexeme += '\'';
            } else if (match('\"')) {
                lexeme += '\"';
            } else {
                advance();
                warning({"Unrecognized escape sequence"}, previous());
            }
        } else {
            lexeme += advance();
        }
    }

    if (is_at_end()) {
        error(
            {"Unexpected end of file while reading string, did you forget the closing '", std::string{delimiter}, "'?"},
            Token{TokenType::STRING_VALUE, std::string{source.substr(start, (current - start))}, line, start, current});
    }

    advance(); // Consume the closing delimiter
    tokens.emplace_back(TokenType::STRING_VALUE, lexeme, line, start, current);
}

void Scanner::multiline_comment() {
    while (not is_at_end() && not(peek() == '*' && peek_next() == '/')) {
        if (match('/')) {
            if (match('*')) {
                multiline_comment();
            } else if (match('/')) {
                while (not is_at_end() && peek() != '\n') {
                    advance();
                }
            }
        } else {
            if (peek() == '\n') {
                line++;
            }
            advance();
        }
    }

    if (is_at_end()) {
        error({"Unexpected end of file while reading comment, did you forget the closing '*/'?"},
            Token{TokenType::STRING_VALUE, std::string{source.substr(start, (current - start))}, line, start, current});
    }

    advance(); // *
    advance(); // /
}

void Scanner::scan_token() {
    auto add_equals = [this](TokenType equal, TokenType not_equal) {
        if (match('=')) {
            add_token(equal);
        } else {
            add_token(not_equal);
        }
    };

    switch (char ch = advance(); ch) {
        case '.':
            match('.') ? (match('=') ? add_token(TokenType::DOT_DOT_EQUAL) : add_token(TokenType::DOT_DOT))
                       : add_token(TokenType::DOT);
            break;
        case ',': add_token(TokenType::COMMA); break;
        case '?': add_token(TokenType::QUESTION); break;
        case ':': add_token(match(':') ? TokenType::DOUBLE_COLON : TokenType::COLON); break;
        case '|': add_token(match('|') ? TokenType::OR : TokenType::BIT_OR); break;
        case '&': add_token(match('&') ? TokenType::AND : TokenType::BIT_AND); break;
        case '^': add_token(TokenType::BIT_XOR); break;

        case '!': add_equals(TokenType::NOT_EQUAL, TokenType::NOT); break;
        case '=': add_equals(TokenType::EQUAL_EQUAL, TokenType::EQUAL); break;
        case '>': {
            if (match('>')) {
                add_token(TokenType::RIGHT_SHIFT);
                break;
            }

            add_equals(TokenType::GREATER_EQUAL, TokenType::GREATER);
            break;
        }
        case '<': {
            if (match('<')) {
                add_token(TokenType::LEFT_SHIFT);
                break;
            }

            add_equals(TokenType::LESS_EQUAL, TokenType::LESS);
            break;
        }

        case '*': add_equals(TokenType::STAR_EQUAL, TokenType::STAR); break;

        case '-': {
            if (match('-')) {
                add_token(TokenType::MINUS_MINUS);
                break;
            } else if (match('>')) {
                add_token(TokenType::ARROW);
                break;
            }

            add_equals(TokenType::MINUS_EQUAL, TokenType::MINUS);
            break;
        }
        case '+': {
            if (match('+')) {
                add_token(TokenType::PLUS_PLUS);
                break;
            }

            add_equals(TokenType::PLUS_EQUAL, TokenType::PLUS);
            break;
        }

        case '%': add_token(TokenType::MODULO); break;
        case '~': add_token(TokenType::BIT_NOT); break;

        case '(':
            paren_count++;
            add_token(TokenType::LEFT_PAREN);
            break;
        case ')':
            paren_count--;
            add_token(TokenType::RIGHT_PAREN);
            break;
        case '[': add_token(TokenType::LEFT_INDEX); break;
        case ']': add_token(TokenType::RIGHT_INDEX); break;
        case '{': add_token(TokenType::LEFT_BRACE); break;
        case '}': add_token(TokenType::RIGHT_BRACE); break;

        case '"': string('"'); break;
        case '\'': string('\''); break;

        case ';': add_token(TokenType::SEMICOLON); break;

        case ' ':
        case '\t':
        case '\r':
        case '\b': break;
        case '\n': {
            auto is_allowed = [this](const Token &token) {
                switch (token.type) {
                    case TokenType::BREAK:
                    case TokenType::CONTINUE:
                    case TokenType::FLOAT_VALUE:
                    case TokenType::INT_VALUE:
                    case TokenType::STRING_VALUE:
                    case TokenType::IDENTIFIER:
                    case TokenType::RIGHT_PAREN:
                    case TokenType::RIGHT_INDEX: return true;
                    default: return false;
                }
            };
            if (paren_count == 0 && not tokens.empty() && is_allowed(previous())) {
                add_token(TokenType::END_OF_LINE);
            }
            line++;
            break;
        }

        default: {
            if (std::isdigit(ch)) {
                number();
                break;
            } else if (std::isalpha(ch) || ch == '_') {
                identifier();
                break;
            } else if (ch == '/') {
                if (match('/')) {
                    while (not is_at_end() && peek() != '\n') {
                        advance();
                    }
                    break;
                } else if (match('*')) {
                    multiline_comment();
                    break;
                }
                add_equals(TokenType::SLASH_EQUAL, TokenType::SLASH);
                break;
            }

            error({"Unrecognized character ", std::string{ch}, " in input"},
                Token{TokenType::STRING_VALUE, std::string{source.substr(start, (current - start))}, line, start,
                    current});
        }
    }
}

const std::vector<Token> &Scanner::scan() {
    while (not is_at_end()) {
        start = current;
        scan_token();
    }

    if (not tokens.empty() && tokens.back().type != TokenType::END_OF_LINE &&
        tokens.back().type != TokenType::SEMICOLON) {
        tokens.emplace_back(TokenType::END_OF_LINE, "\n", line, 0, 0);
    }
    tokens.emplace_back(TokenType::END_OF_FILE, "", line, 0, 0);
    return tokens;
}

ScannerV2::ScannerV2() {
    for (const auto &[lexeme, type] : keywords) {
        keyword_map.insert(lexeme, type);
    }
}

ScannerV2::ScannerV2(std::string_view source) : ScannerV2() {
    this->source = source;
}

char ScannerV2::advance() {
    if (is_at_end()) {
        return '\0';
    }

    current_token_end++;
    return source[current_token_end - 1];
}

char ScannerV2::peek() const noexcept {
    if (is_at_end()) {
        return '\0';
    }

    return source[current_token_end];
}

char ScannerV2::peek_next() const noexcept {
    if (current_token_end + 1 >= source.length()) {
        return '\0';
    }

    return source[current_token_end + 1];
}

bool ScannerV2::match(char ch) {
    if (peek() == ch) {
        advance();
        return true;
    }

    return false;
}

Token ScannerV2::make_token(TokenType type, std::string_view lexeme) const {
    return Token{type, std::string{lexeme}, line, current_token_start, current_token_end};
}

Token ScannerV2::scan_number() {
    TokenType type = TokenType::INT_VALUE;

    auto scan_digits = [this] {
        while (not is_at_end() && std::isdigit(peek())) {
            advance();
        }
    };

    scan_digits();

    if (peek() == '.' && std::isdigit(peek_next())) {
        type = TokenType::FLOAT_VALUE;
        advance();
        scan_digits();
    }

    if (peek() == 'e' && std::isdigit(peek_next())) {
        type = TokenType::FLOAT_VALUE;
        advance();
        scan_digits();
    }

    return make_token(type, current_token_lexeme());
}

Token ScannerV2::scan_identifier_or_keyword() {
    while (not is_at_end() && (std::isalnum(peek()) || peek() == '_')) {
        advance();
    }

    std::string_view lexeme = current_token_lexeme();
    if (TokenType type = keyword_map.search(lexeme); type != TokenType::NONE) {
        return make_token(type, lexeme);
    } else {
        return make_token(TokenType::IDENTIFIER, lexeme);
    }
}

Token ScannerV2::scan_string() {
    std::size_t size = 1;
    while ((current_token_start + size) <= source.length() && source[current_token_start + size] != '"') {
        size++;
        if (source[current_token_start + size] == '\\') {
            size++;
            size++;
        }
    }

    std::string lexeme{};
    lexeme.reserve(size);

    while (not is_at_end() && peek() != '"') {
        if (peek() == '\n') {
            line++;
            advance();
            lexeme += '\n';
        } else if (match('\\')) {
            if (match('b')) {
                lexeme += '\b';
            } else if (match('n')) {
                lexeme += '\n';
            } else if (match('r')) {
                lexeme += '\r';
            } else if (match('t')) {
                lexeme += '\t';
            } else if (match('\\')) {
                lexeme += '\\';
            } else if (match('\'')) {
                lexeme += '\'';
            } else if (match('\"')) {
                lexeme += '\"';
            } else {
                char invalid = advance();
                warning({"Unrecognized escape sequence: '\\", std::string{invalid}, "'"}, previous);
            }
        } else {
            lexeme += advance();
        }
    }

    if (is_at_end()) {
        error({"Unexpected end of file while reading string, did you forget the closing '\"'?"},
            make_token(TokenType::STRING_VALUE, current_token_lexeme()));
    }

    advance(); // Consume the closing '"'
    return make_token(TokenType::STRING_VALUE, lexeme);
}

void ScannerV2::skip_comment() {
    while (not is_at_end() && peek() != '\n') {
        advance();
    }
}

void ScannerV2::skip_multiline_comment() {
    while (not is_at_end() && (peek() != '*' || peek_next() != '/')) {
        if (match('/')) {
            if (match('*')) {
                skip_multiline_comment(); // Skip the nested comment
            } else if (match('/')) {
                skip_comment();
            }
        } else {
            if (peek() == '\n') {
                line++;
            }
            advance();
        }
    }

    if (is_at_end()) {
        error({"Unexpected end of file while skipping multiline comment, did you forget the closing '*/'"},
            make_token(TokenType::NONE, current_token_lexeme()));
    }

    advance(); // Skip the '*'
    advance(); // Skip the '//'
}

bool ScannerV2::is_valid_eol(const Token &token) {
    switch (token.type) {
        case TokenType::BREAK:
        case TokenType::CONTINUE:
        case TokenType::FLOAT_VALUE:
        case TokenType::INT_VALUE:
        case TokenType::STRING_VALUE:
        case TokenType::IDENTIFIER:
        case TokenType::RIGHT_PAREN:
        case TokenType::RIGHT_INDEX: return true;
        default: return false;
    }
}

std::string_view ScannerV2::current_token_lexeme() {
    return source.substr(current_token_start, (current_token_end - current_token_start));
}

bool ScannerV2::is_at_end() const noexcept {
    return current_token_end >= source.length();
}

Token ScannerV2::scan_token() {
    previous = current;

    char next = advance();
    switch (next) {
        case '.': {
            if (match('.')) {
                if (match('=')) {
                    current = make_token(TokenType::DOT_DOT_EQUAL, current_token_lexeme());
                } else {
                    current = make_token(TokenType::DOT_DOT, current_token_lexeme());
                }
            } else {
                current = make_token(TokenType::DOT, current_token_lexeme());
            }
        }
        case ',': {
            current = make_token(TokenType::COMMA, current_token_lexeme());
        }
        case '?': {
            current = make_token(TokenType::QUESTION, current_token_lexeme());
        }
        case ':': {
            if (match(':')) {
                current = make_token(TokenType::DOUBLE_COLON, current_token_lexeme());
            } else {
                current = make_token(TokenType::COLON, current_token_lexeme());
            }
        }
        case '|': {
            if (match('|')) {
                current = make_token(TokenType::OR, current_token_lexeme());
            } else {
                current = make_token(TokenType::BIT_OR, current_token_lexeme());
            }
        }
        case '&': {
            if (match('&')) {
                current = make_token(TokenType::AND, current_token_lexeme());
            } else {
                current = make_token(TokenType::BIT_AND, current_token_lexeme());
            }
        }
        case '^': {
            current = make_token(TokenType::BIT_XOR, current_token_lexeme());
        }
        case '!': {
            if (match('=')) {
                current = make_token(TokenType::NOT_EQUAL, current_token_lexeme());
            } else {
                current = make_token(TokenType::NOT, current_token_lexeme());
            }
        }
        case '=': {
            if (match('=')) {
                current = make_token(TokenType::EQUAL_EQUAL, current_token_lexeme());
            } else {
                current = make_token(TokenType::EQUAL, current_token_lexeme());
            }
        }
        case '>': {
            if (match('>')) {
                current = make_token(TokenType::RIGHT_SHIFT, current_token_lexeme());
            } else if (match('=')) {
                current = make_token(TokenType::GREATER_EQUAL, current_token_lexeme());
            } else {
                current = make_token(TokenType::GREATER, current_token_lexeme());
            }
        }
        case '<': {
            if (match('<')) {
                current = make_token(TokenType::LEFT_SHIFT, current_token_lexeme());
            } else if (match('=')) {
                current = make_token(TokenType::LESS_EQUAL, current_token_lexeme());
            } else {
                current = make_token(TokenType::LESS, current_token_lexeme());
            }
        }
        case '*': {
            if (match('=')) {
                current = make_token(TokenType::STAR_EQUAL, current_token_lexeme());
            } else {
                current = make_token(TokenType::STAR, current_token_lexeme());
            }
        }
        case '-': {
            if (match('-')) {
                current = make_token(TokenType::MINUS_MINUS, current_token_lexeme());
            } else if (match('>')) {
                current = make_token(TokenType::ARROW, current_token_lexeme());
            } else if (match('=')) {
                current = make_token(TokenType::MINUS_EQUAL, current_token_lexeme());
            } else {
                current = make_token(TokenType::MINUS, current_token_lexeme());
            }
        }
        case '+': {
            if (match('+')) {
                current = make_token(TokenType::PLUS_PLUS, current_token_lexeme());
            } else if (match('=')) {
                current = make_token(TokenType::PLUS_EQUAL, current_token_lexeme());
            } else {
                current = make_token(TokenType::PLUS, current_token_lexeme());
            }
        }
        case '%': {
            current = make_token(TokenType::MODULO, current_token_lexeme());
        }
        case '~': {
            current = make_token(TokenType::BIT_NOT, current_token_lexeme());
        }

        case '(': {
            paren_depth++;
            current = make_token(TokenType::LEFT_PAREN, current_token_lexeme());
        }
        case ')': {
            paren_depth--;
            current = make_token(TokenType::RIGHT_PAREN, current_token_lexeme());
        }
        case '[': {
            current = make_token(TokenType::LEFT_INDEX, current_token_lexeme());
        }
        case ']': {
            current = make_token(TokenType::RIGHT_INDEX, current_token_lexeme());
        }
        case '{': {
            current = make_token(TokenType::LEFT_BRACE, current_token_lexeme());
        }
        case '}': {
            current = make_token(TokenType::RIGHT_BRACE, current_token_lexeme());
        }

        case '"': {
            current = scan_string();
        }

        case ';': {
            current = make_token(TokenType::SEMICOLON, current_token_lexeme());
        }

        case ' ':
        case '\t':
        case '\r':
        case '\b': {
            current = scan_token();
        }

        case '\n': {
            line++;
            if (paren_depth == 0 && is_valid_eol(previous)) {
                current = make_token(TokenType::END_OF_LINE, current_token_lexeme());
            } else {
                current = scan_token();
            }
        }

        default: {
            if (std::isdigit(next)) {
                current = scan_number();
            } else if (std::isalpha(next) || next == '_') {
                current = scan_identifier_or_keyword();
            } else if (next == '/') {
                if (match('/')) {
                    skip_comment();
                    current = scan_token();
                } else if (match('*')) {
                    skip_multiline_comment();
                    current = scan_token();
                } else if (match('=')) {
                    current = make_token(TokenType::SLASH_EQUAL, current_token_lexeme());
                } else {
                    current = make_token(TokenType::SLASH, current_token_lexeme());
                }
            } else if (is_at_end()) {
                current = make_token(TokenType::END_OF_FILE, "<EOF>");
            }

            error({"Unrecognized character '", std::string{next}, "' in input"},
                make_token(TokenType::INVALID, current_token_lexeme()));
            current = make_token(TokenType::INVALID, current_token_lexeme());
        }
    }

    return previous;
}

std::vector<Token> ScannerV2::scan_all() {
    std::vector<Token> tokens{};
    while (not is_at_end()) {
        tokens.emplace_back(scan_token());
    }

    if (not tokens.empty() && tokens.back().type != TokenType::END_OF_LINE &&
        tokens.back().type != TokenType::SEMICOLON) {
        tokens.emplace_back(make_token(TokenType::END_OF_LINE, "\\n"));
    }

    tokens.emplace_back(make_token(TokenType::END_OF_FILE, "<EOF>"));
    return tokens;
}