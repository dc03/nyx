/* See LICENSE at project root for license details */
#include <cctype>

#include "../ErrorLogger/ErrorLogger.hpp"
#include "Scanner.hpp"

Scanner::Scanner() {
    const char *words[]{
        "and", "break", "class", "const", "continue", "else", "false", "float",
        "fn", "for", "if", "import", "int", "null", "or", "protected", "private",
        "public", "ref", "return", "string", "true", "type", "typeof", "val",
        "var", "while"
    };

    TokenType types[]{
        TokenType::AND, TokenType::BREAK, TokenType::CLASS, TokenType::CONST,
        TokenType::CONTINUE, TokenType::ELSE, TokenType::FALSE, TokenType::FLOAT,
        TokenType::FN, TokenType::FOR, TokenType::IF, TokenType::IMPORT, TokenType::INT,
        TokenType::NULL_, TokenType::OR, TokenType::PROTECTED, TokenType::PRIVATE,
        TokenType::PUBLIC, TokenType::REF, TokenType::RETURN, TokenType::STRING,
        TokenType::TRUE, TokenType::TYPE, TokenType::TYPEOF, TokenType::VAL, TokenType::VAR,
        TokenType::WHILE
    };

    static_assert(std::size(words) == std::size(types), "Size of array of keywords and their types have to be same.");

    for (std::size_t i{0}; i < std::size(words); i++) {
        keywords.insert(words[i], types[i]);
    }
}

Scanner::Scanner(const std::string_view source): Scanner() {
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

char Scanner::peek() const noexcept {
    return source[current];
}

char Scanner::peek_next() const noexcept {
    if (current + 1 >= source.size()) {
        return '\0';
    }

    return source[current + 1];
}

bool Scanner::match(const char ch) {
    if (!is_at_end() && peek() == ch) {
        advance();
        return true;
    }

    return false;
}

void Scanner::add_token(const TokenType type) {
    tokens.push_back(Token{type, std::string{source.substr(start, (current - start))},
                           line, start, current});
}

void Scanner::number() {
    TokenType type = TokenType::INT_VALUE;
    while (!is_at_end() && std::isdigit(peek())) {
        advance();
    }

    if (peek() == '.' && std::isdigit(peek_next())) {
        type = TokenType::FLOAT_VALUE;
        advance();
        while (!is_at_end() && std::isdigit(peek())) {
            advance();
        }
    }

    if (peek() == 'e' && std::isdigit(peek_next())) {
        type = TokenType::FLOAT_VALUE;
        advance();
        while (!is_at_end() && std::isdigit(peek())) {
            advance();
        }
    }

    add_token(type);
}

void Scanner::identifier() {
    while (!is_at_end() && (std::isalnum(peek()) || peek() == '_')) {
        advance();
    }

    std::string_view lexeme = source.substr(start, (current - start));
    if (TokenType type{keywords.search(lexeme)}; type != TokenType::NONE) {
        add_token(type);
    } else {
        add_token(TokenType::IDENTIFIER);
    }
}

void Scanner::string(const char delim) {
    std::string lexeme{}; // I think this should be fine because of SSO leading to an allocation only after ~24 chars.
    while (!is_at_end() && peek() != delim) {
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
            }
        } else {
            lexeme += advance();
        }
    }

    using namespace std::string_literals;
    if (is_at_end()) {
        std::string message{"Unexpected end of file while reading string, did you"
                            " forget the closing '"s + std::string{delim} + "'?"};
        error(message, line, source.substr(start, (current - start)));
    }

    advance(); // Consume the closing delimiter
    tokens.push_back(Token{TokenType::STRING, lexeme, line, start, current});
}

void Scanner::multiline_comment() {
    while(!is_at_end() && !(peek() == '*' && peek_next() == '/')) {
        if (match('/') && match('*')) {
            multiline_comment();
        } else {
            if (peek() == '\n') {
                line++;
            }
            advance();
        }
    }

    if (is_at_end()) {
        error("Unexpected end of file while reading comment, did you forget the closing '*/'?",
              line, source.substr(start, (current - start)));
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

    switch(char ch = advance(); ch) {
        case '.': add_token(TokenType::DOT); break;
        case ',': add_token(TokenType::COMMA); break;
        case '?': add_token(TokenType::QUESTION); break;
        case ':': add_token(TokenType::COLON); break;
        case '|': add_token(match('|') ? TokenType::LOGIC_OR : TokenType::BIT_OR); break;
        case '&': add_token(match('&') ? TokenType::LOGIC_AND : TokenType::BIT_AND); break;
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

        case '(': add_token(TokenType::LEFT_PAREN); break;
        case ')': add_token(TokenType::RIGHT_PAREN); break;
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
        case '\n':
            add_token(TokenType::END_OF_LINE);
            line++;
            break;

        default: {
            if (std::isdigit(ch)) {
                number();
                break;
            } else if (std::isalpha(ch) || ch == '_') {
                identifier();
                break;
            } else if (ch == '/') {
                if (match('/')) {
                    while (!is_at_end() && peek() != '\n') {
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

            using namespace std::string_literals;
            error(("Unrecognized character "s + std::string{ch} + " in input"), line);
        }
    }
}

const std::vector<Token> &Scanner::scan() {
    while (!is_at_end()) {
        start = current;
        scan_token();
    }

    tokens.emplace_back(TokenType::END_OF_FILE, "", line, 0, 0);
    return tokens;
}
