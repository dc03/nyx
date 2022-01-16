/* Copyright (C) 2020-2022  Dhruv Chawla */
/* See LICENSE at project root for license details */
#include "Frontend/Scanner/Scanner.hpp"

#include "Common.hpp"
#include "ErrorLogger/ErrorLogger.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>

Scanner::Scanner() {
    for (const auto &[lexeme, type] : keywords) {
        keyword_map.insert(lexeme, type);
    }
}

Scanner::Scanner(FrontendContext *ctx_, Module *module_, std::string_view source_) : Scanner() {
    ctx = ctx_;
    module = module_;
    source = source_;
}

void Scanner::reset() {
    line = 1;
    current_token_start = 0;
    current_token_end = 0;
    paren_depth = 0;
    current_token = Token{};
    next_token = Token{};
}

char Scanner::advance() {
    if (is_at_end()) {
        return '\0';
    }

    current_token_end++;
    return source[current_token_end - 1];
}

char Scanner::peek() const noexcept {
    if (is_at_end()) {
        return '\0';
    }

    return source[current_token_end];
}

char Scanner::peek_next() const noexcept {
    if (current_token_end + 1 >= source.length()) {
        return '\0';
    }

    return source[current_token_end + 1];
}

bool Scanner::match(char ch) {
    if (peek() == ch) {
        advance();
        return true;
    }

    return false;
}

Token Scanner::make_token(TokenType type, std::string_view lexeme) const {
    return Token{type, std::string{lexeme}, line, current_token_start, current_token_end};
}

Token Scanner::scan_number() {
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

Token Scanner::scan_identifier_or_keyword() {
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

Token Scanner::scan_string() {
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
                ctx->logger.warning(
                    module, {"Unrecognized escape sequence: '\\", std::string{invalid}, "'"}, current_token);
            }
        } else {
            lexeme += advance();
        }
    }

    if (is_at_end()) {
        ctx->logger.error(module, {"Unexpected end of file while reading string, did you forget the closing '\"'?"},
            make_token(TokenType::STRING_VALUE, current_token_lexeme()));
    }

    advance(); // Consume the closing '"'
    return make_token(TokenType::STRING_VALUE, lexeme);
}

void Scanner::skip_comment() {
    while (not is_at_end() && peek() != '\n') {
        advance();
    }
}

void Scanner::skip_multiline_comment() {
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
        ctx->logger.error(module,
            {"Unexpected end of file while skipping multiline comment, did you forget the closing '*/'"},
            make_token(TokenType::NONE, current_token_lexeme()));
    }

    advance(); // Skip the '*'
    advance(); // Skip the '//'
}

bool Scanner::is_valid_eol(const Token &token) {
    switch (token.type) {
        case TokenType::BREAK:
        case TokenType::CONTINUE:
        case TokenType::FLOAT_VALUE:
        case TokenType::INT_VALUE:
        case TokenType::STRING_VALUE:
        case TokenType::IDENTIFIER:
        case TokenType::RIGHT_PAREN:
        case TokenType::RIGHT_INDEX:
        case TokenType::TRUE:
        case TokenType::FALSE: return true;
        default: return false;
    }
}

std::string_view Scanner::current_token_lexeme() {
    return source.substr(current_token_start, (current_token_end - current_token_start));
}

bool Scanner::is_at_end() const noexcept {
    return current_token_start >= source.length();
}

Token Scanner::scan_next() {
    char next = advance();
    switch (next) {
        case '.': {
            if (match('.')) {
                if (match('=')) {
                    return make_token(TokenType::DOT_DOT_EQUAL, current_token_lexeme());
                } else {
                    return make_token(TokenType::DOT_DOT, current_token_lexeme());
                }
            } else {
                return make_token(TokenType::DOT, current_token_lexeme());
            }
        }
        case ',': {
            return make_token(TokenType::COMMA, current_token_lexeme());
        }
        case '?': {
            return make_token(TokenType::QUESTION, current_token_lexeme());
        }
        case ':': {
            if (match(':')) {
                return make_token(TokenType::DOUBLE_COLON, current_token_lexeme());
            } else {
                return make_token(TokenType::COLON, current_token_lexeme());
            }
        }
        case '|': {
            if (match('|')) {
                return make_token(TokenType::OR, current_token_lexeme());
            } else {
                return make_token(TokenType::BIT_OR, current_token_lexeme());
            }
        }
        case '&': {
            if (match('&')) {
                return make_token(TokenType::AND, current_token_lexeme());
            } else {
                return make_token(TokenType::BIT_AND, current_token_lexeme());
            }
        }
        case '^': {
            return make_token(TokenType::BIT_XOR, current_token_lexeme());
        }
        case '!': {
            if (match('=')) {
                return make_token(TokenType::NOT_EQUAL, current_token_lexeme());
            } else {
                return make_token(TokenType::NOT, current_token_lexeme());
            }
        }
        case '=': {
            if (match('=')) {
                return make_token(TokenType::EQUAL_EQUAL, current_token_lexeme());
            } else {
                return make_token(TokenType::EQUAL, current_token_lexeme());
            }
        }
        case '>': {
            if (match('>')) {
                return make_token(TokenType::RIGHT_SHIFT, current_token_lexeme());
            } else if (match('=')) {
                return make_token(TokenType::GREATER_EQUAL, current_token_lexeme());
            } else {
                return make_token(TokenType::GREATER, current_token_lexeme());
            }
        }
        case '<': {
            if (match('<')) {
                return make_token(TokenType::LEFT_SHIFT, current_token_lexeme());
            } else if (match('=')) {
                return make_token(TokenType::LESS_EQUAL, current_token_lexeme());
            } else {
                return make_token(TokenType::LESS, current_token_lexeme());
            }
        }
        case '*': {
            if (match('=')) {
                return make_token(TokenType::STAR_EQUAL, current_token_lexeme());
            } else {
                return make_token(TokenType::STAR, current_token_lexeme());
            }
        }
        case '-': {
            if (match('-')) {
                return make_token(TokenType::MINUS_MINUS, current_token_lexeme());
            } else if (match('>')) {
                return make_token(TokenType::ARROW, current_token_lexeme());
            } else if (match('=')) {
                return make_token(TokenType::MINUS_EQUAL, current_token_lexeme());
            } else {
                return make_token(TokenType::MINUS, current_token_lexeme());
            }
        }
        case '+': {
            if (match('+')) {
                return make_token(TokenType::PLUS_PLUS, current_token_lexeme());
            } else if (match('=')) {
                return make_token(TokenType::PLUS_EQUAL, current_token_lexeme());
            } else {
                return make_token(TokenType::PLUS, current_token_lexeme());
            }
        }
        case '%': {
            return make_token(TokenType::MODULO, current_token_lexeme());
        }
        case '~': {
            return make_token(TokenType::BIT_NOT, current_token_lexeme());
        }

        case '(': {
            paren_depth++;
            return make_token(TokenType::LEFT_PAREN, current_token_lexeme());
        }
        case ')': {
            paren_depth--;
            return make_token(TokenType::RIGHT_PAREN, current_token_lexeme());
        }
        case '[': {
            return make_token(TokenType::LEFT_INDEX, current_token_lexeme());
        }
        case ']': {
            return make_token(TokenType::RIGHT_INDEX, current_token_lexeme());
        }
        case '{': {
            return make_token(TokenType::LEFT_BRACE, current_token_lexeme());
        }
        case '}': {
            return make_token(TokenType::RIGHT_BRACE, current_token_lexeme());
        }

        case '"': {
            return scan_string();
        }

        case ';': {
            return make_token(TokenType::SEMICOLON, current_token_lexeme());
        }

        case ' ':
        case '\t':
        case '\r':
        case '\b': {
            current_token_start = current_token_end;
            return scan_next();
        }

        case '\n': {
            line++;
            if (paren_depth == 0 && is_valid_eol(current_token)) {
                return make_token(TokenType::END_OF_LINE, "<EOL>");
            } else {
                current_token_start = current_token_end;
                return scan_next();
            }
        }

        default: {
            if (std::isdigit(next)) {
                return scan_number();
            } else if (std::isalpha(next) || next == '_') {
                return scan_identifier_or_keyword();
            } else if (next == '/') {
                if (match('/')) {
                    skip_comment();
                    current_token_start = current_token_end;
                    return scan_next();
                } else if (match('*')) {
                    skip_multiline_comment();
                    current_token_start = current_token_end;
                    return scan_next();
                } else if (match('=')) {
                    return make_token(TokenType::SLASH_EQUAL, current_token_lexeme());
                } else {
                    return make_token(TokenType::SLASH, current_token_lexeme());
                }
            } else if (is_at_end()) {
                if (is_valid_eol(current_token)) {
                    return make_token(TokenType::END_OF_LINE, "<EOL>");
                } else {
                    return make_token(TokenType::END_OF_FILE, "<EOF>");
                }
            }

            ctx->logger.error(module, {"Unrecognized character '", std::string{next}, "' in input"},
                make_token(TokenType::INVALID, current_token_lexeme()));
            return make_token(TokenType::INVALID, current_token_lexeme());
        }
    }
}

Token Scanner::scan_token() {
    current_token = next_token;
    current_token_start = current_token_end;
    next_token = scan_next();
    return current_token;
}

const Token &Scanner::peek_token() {
    return next_token;
}

std::vector<Token> Scanner::scan_all() {
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