// =============================================================================
// token.hpp — Token Types & Token Struct
// =============================================================================
// Defines the complete set of token types recognized by the lexer, and the
// Token struct that carries lexeme text, source location (line/column), and
// an integer value for numeric literals.
//
// Part of the C-Subset Mini-Compiler project.
// =============================================================================

#pragma once

#include <string>
#include <ostream>

namespace mc {

// ─────────────────────────────────────────────────────────────────────────────
// TokenType — Every distinct lexical element in the source language.
// ─────────────────────────────────────────────────────────────────────────────
enum class TokenType {
    // ── Literals & Identifiers ──────────────────────────────────────────────
    INT_LITERAL,        // e.g. 42, 0, 1000
    IDENTIFIER,         // e.g. x, myVar, counter

    // ── Keywords ────────────────────────────────────────────────────────────
    KW_INT,             // "int"
    KW_IF,              // "if"
    KW_ELSE,            // "else"
    KW_WHILE,           // "while"
    KW_PRINT,           // "print"

    // ── Arithmetic Operators ────────────────────────────────────────────────
    PLUS,               // +
    MINUS,              // -
    STAR,               // *
    SLASH,              // /
    PERCENT,            // %

    // ── Assignment ──────────────────────────────────────────────────────────
    ASSIGN,             // =

    // ── Comparison Operators ────────────────────────────────────────────────
    EQ,                 // ==
    NEQ,                // !=
    LT,                 // <
    LTEQ,               // <=
    GT,                 // >
    GTEQ,               // >=

    // ── Logical Operators ───────────────────────────────────────────────────
    AND,                // &&
    OR,                 // ||
    BANG,               // !  (logical NOT / prefix)

    // ── Delimiters ──────────────────────────────────────────────────────────
    LPAREN,             // (
    RPAREN,             // )
    LBRACE,             // {
    RBRACE,             // }
    SEMICOLON,          // ;

    // ── Special ─────────────────────────────────────────────────────────────
    END_OF_FILE,        // Sentinel marking end of input
    INVALID             // Placeholder for error recovery
};

// ─────────────────────────────────────────────────────────────────────────────
// Token — A single lexical unit carrying its type, text, and source position.
// ─────────────────────────────────────────────────────────────────────────────
struct Token {
    TokenType   type;
    std::string lexeme;     // The raw text of the token
    int         line;       // 1-based source line number
    int         column;     // 1-based source column number
    int         int_value;  // Meaningful only when type == INT_LITERAL

    Token(TokenType type, std::string lexeme, int line, int column, int int_value = 0)
        : type(type)
        , lexeme(std::move(lexeme))
        , line(line)
        , column(column)
        , int_value(int_value)
    {}
};

// ─────────────────────────────────────────────────────────────────────────────
// Utility: convert TokenType to a human-readable string.
// ─────────────────────────────────────────────────────────────────────────────
const char* tokenTypeToString(TokenType type);

// ─────────────────────────────────────────────────────────────────────────────
// Stream insertion for pretty-printing tokens during --lex output.
// ─────────────────────────────────────────────────────────────────────────────
std::ostream& operator<<(std::ostream& os, const Token& token);

} // namespace mc
