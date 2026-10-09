// =============================================================================
// lexer.cpp — Lexical Analyzer Implementation
// =============================================================================
// Scans source text character-by-character to produce a token stream.
// Handles:
//   • Single-line comments  (//)
//   • Block comments         (/* */)
//   • Whitespace skipping with line/column tracking
//   • Multi-character operators (==, !=, <=, >=, &&, ||)
//   • Keyword recognition via a hash-map lookup
//   • Descriptive error messages with precise source locations
//
// Part of the C-Subset Mini-Compiler project.
// =============================================================================

#include "lexer.hpp"
#include <cctype>
#include <unordered_map>

namespace mc {

// ─────────────────────────────────────────────────────────────────────────────
// Keyword table — maps reserved words to their TokenType.
// ─────────────────────────────────────────────────────────────────────────────
static const std::unordered_map<std::string, TokenType> kKeywords = {
    {"int",   TokenType::KW_INT},
    {"if",    TokenType::KW_IF},
    {"else",  TokenType::KW_ELSE},
    {"while", TokenType::KW_WHILE},
    {"print", TokenType::KW_PRINT},
};

// ═════════════════════════════════════════════════════════════════════════════
//  Construction
// ═════════════════════════════════════════════════════════════════════════════

Lexer::Lexer(std::string source)
    : source_(std::move(source))
    , pos_(0)
    , line_(1)
    , column_(1)
{}

// ═════════════════════════════════════════════════════════════════════════════
//  Character-level accessors
// ═════════════════════════════════════════════════════════════════════════════

char Lexer::current() const {
    return isAtEnd() ? '\0' : source_[pos_];
}

char Lexer::peek() const {
    return (pos_ + 1 >= source_.size()) ? '\0' : source_[pos_ + 1];
}

char Lexer::advance() {
    char c = source_[pos_++];
    if (c == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    return c;
}

bool Lexer::isAtEnd() const {
    return pos_ >= source_.size();
}

// ═════════════════════════════════════════════════════════════════════════════
//  Whitespace & comment skipping
// ═════════════════════════════════════════════════════════════════════════════

void Lexer::skipWhitespace() {
    while (!isAtEnd() && std::isspace(static_cast<unsigned char>(current()))) {
        advance();
    }
}

void Lexer::skipLineComment() {
    // Consume the "//" prefix
    advance();
    advance();
    // Consume everything until end-of-line or end-of-file
    while (!isAtEnd() && current() != '\n') {
        advance();
    }
}

void Lexer::skipBlockComment() {
    int startLine = line_;
    int startCol  = column_;
    // Consume the "/*" prefix
    advance();
    advance();
    while (!isAtEnd()) {
        if (current() == '*' && peek() == '/') {
            advance();  // consume '*'
            advance();  // consume '/'
            return;
        }
        advance();
    }
    // If we reach here, the comment was never closed.
    throw LexerError("Unterminated block comment", startLine, startCol);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Token scanners
// ═════════════════════════════════════════════════════════════════════════════

Token Lexer::scanNumber() {
    int startCol = column_;
    std::string digits;
    while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(current()))) {
        digits += advance();
    }
    int value = std::stoi(digits);
    return Token(TokenType::INT_LITERAL, digits, line_, startCol, value);
}

Token Lexer::scanIdentifierOrKeyword() {
    int startCol = column_;
    std::string text;
    while (!isAtEnd() &&
           (std::isalnum(static_cast<unsigned char>(current())) || current() == '_'))
    {
        text += advance();
    }
    // Check if it's a reserved keyword
    auto it = kKeywords.find(text);
    if (it != kKeywords.end()) {
        return Token(it->second, text, line_, startCol);
    }
    return Token(TokenType::IDENTIFIER, text, line_, startCol);
}

Token Lexer::scanToken() {
    int  startLine = line_;
    int  startCol  = column_;
    char c = advance();

    switch (c) {
        // ── Single-character tokens ─────────────────────────────────────
        case '+': return Token(TokenType::PLUS,      "+", startLine, startCol);
        case '-': return Token(TokenType::MINUS,     "-", startLine, startCol);
        case '*': return Token(TokenType::STAR,      "*", startLine, startCol);
        case '/': return Token(TokenType::SLASH,     "/", startLine, startCol);
        case '%': return Token(TokenType::PERCENT,   "%", startLine, startCol);
        case '(': return Token(TokenType::LPAREN,    "(", startLine, startCol);
        case ')': return Token(TokenType::RPAREN,    ")", startLine, startCol);
        case '{': return Token(TokenType::LBRACE,    "{", startLine, startCol);
        case '}': return Token(TokenType::RBRACE,    "}", startLine, startCol);
        case ';': return Token(TokenType::SEMICOLON, ";", startLine, startCol);

        // ── Possibly two-character tokens ───────────────────────────────
        case '=':
            if (!isAtEnd() && current() == '=') {
                advance();
                return Token(TokenType::EQ, "==", startLine, startCol);
            }
            return Token(TokenType::ASSIGN, "=", startLine, startCol);

        case '!':
            if (!isAtEnd() && current() == '=') {
                advance();
                return Token(TokenType::NEQ, "!=", startLine, startCol);
            }
            return Token(TokenType::BANG, "!", startLine, startCol);

        case '<':
            if (!isAtEnd() && current() == '=') {
                advance();
                return Token(TokenType::LTEQ, "<=", startLine, startCol);
            }
            return Token(TokenType::LT, "<", startLine, startCol);

        case '>':
            if (!isAtEnd() && current() == '=') {
                advance();
                return Token(TokenType::GTEQ, ">=", startLine, startCol);
            }
            return Token(TokenType::GT, ">", startLine, startCol);

        case '&':
            if (!isAtEnd() && current() == '&') {
                advance();
                return Token(TokenType::AND, "&&", startLine, startCol);
            }
            throw LexerError(
                "Unexpected character '&' — did you mean '&&'?",
                startLine, startCol);

        case '|':
            if (!isAtEnd() && current() == '|') {
                advance();
                return Token(TokenType::OR, "||", startLine, startCol);
            }
            throw LexerError(
                "Unexpected character '|' — did you mean '||'?",
                startLine, startCol);

        default:
            throw LexerError(
                std::string("Unexpected character '") + c + "'",
                startLine, startCol);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Main tokenize loop
// ═════════════════════════════════════════════════════════════════════════════

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (!isAtEnd()) {
        // 1) Skip whitespace
        skipWhitespace();
        if (isAtEnd()) break;

        // 2) Skip comments
        if (current() == '/' && peek() == '/') {
            skipLineComment();
            continue;
        }
        if (current() == '/' && peek() == '*') {
            skipBlockComment();
            continue;
        }

        // 3) Numeric literals
        if (std::isdigit(static_cast<unsigned char>(current()))) {
            tokens.push_back(scanNumber());
            continue;
        }

        // 4) Identifiers / keywords
        if (std::isalpha(static_cast<unsigned char>(current())) || current() == '_') {
            tokens.push_back(scanIdentifierOrKeyword());
            continue;
        }

        // 5) Operators and delimiters
        tokens.push_back(scanToken());
    }

    // Sentinel token
    tokens.emplace_back(TokenType::END_OF_FILE, "", line_, column_);
    return tokens;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Token utility implementations  (declared in token.hpp)
// ═════════════════════════════════════════════════════════════════════════════

const char* tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::INT_LITERAL: return "INT_LITERAL";
        case TokenType::IDENTIFIER:  return "IDENTIFIER";
        case TokenType::KW_INT:      return "KW_INT";
        case TokenType::KW_IF:       return "KW_IF";
        case TokenType::KW_ELSE:     return "KW_ELSE";
        case TokenType::KW_WHILE:    return "KW_WHILE";
        case TokenType::KW_PRINT:    return "KW_PRINT";
        case TokenType::PLUS:        return "PLUS";
        case TokenType::MINUS:       return "MINUS";
        case TokenType::STAR:        return "STAR";
        case TokenType::SLASH:       return "SLASH";
        case TokenType::PERCENT:     return "PERCENT";
        case TokenType::ASSIGN:      return "ASSIGN";
        case TokenType::EQ:          return "EQ";
        case TokenType::NEQ:         return "NEQ";
        case TokenType::LT:          return "LT";
        case TokenType::LTEQ:        return "LTEQ";
        case TokenType::GT:          return "GT";
        case TokenType::GTEQ:        return "GTEQ";
        case TokenType::AND:         return "AND";
        case TokenType::OR:          return "OR";
        case TokenType::BANG:        return "BANG";
        case TokenType::LPAREN:      return "LPAREN";
        case TokenType::RPAREN:      return "RPAREN";
        case TokenType::LBRACE:      return "LBRACE";
        case TokenType::RBRACE:      return "RBRACE";
        case TokenType::SEMICOLON:   return "SEMICOLON";
        case TokenType::END_OF_FILE: return "EOF";
        case TokenType::INVALID:     return "INVALID";
        default:                     return "UNKNOWN";
    }
}

std::ostream& operator<<(std::ostream& os, const Token& token) {
    os << "[" << token.line << ":" << token.column << "] "
       << tokenTypeToString(token.type);
    if (!token.lexeme.empty()) {
        os << " '" << token.lexeme << "'";
    }
    if (token.type == TokenType::INT_LITERAL) {
        os << " (value=" << token.int_value << ")";
    }
    return os;
}

} // namespace mc
