// =============================================================================
// lexer.hpp — Lexical Analyzer Interface
// =============================================================================
// Declares the Lexer class that transforms raw source text into a stream of
// tokens. Handles whitespace, single-line (//) and block (/* */) comments,
// tracks line and column numbers for precise error diagnostics.
//
// Part of the C-Subset Mini-Compiler project.
// =============================================================================

#pragma once

#include "token.hpp"
#include <string>
#include <vector>
#include <stdexcept>

namespace mc {

// ─────────────────────────────────────────────────────────────────────────────
// LexerError — Thrown when the lexer encounters an unrecognized character
//              or an unterminated block comment.
// ─────────────────────────────────────────────────────────────────────────────
class LexerError : public std::runtime_error {
public:
    int line;       // Source line where the error was detected
    int column;     // Source column where the error was detected

    LexerError(const std::string& msg, int line, int column)
        : std::runtime_error(msg), line(line), column(column) {}
};

// ─────────────────────────────────────────────────────────────────────────────
// Lexer — Scans source text character-by-character and produces a vector
//         of Tokens. The final token is always END_OF_FILE.
// ─────────────────────────────────────────────────────────────────────────────
class Lexer {
public:
    /// Construct a Lexer over the given source string.
    explicit Lexer(std::string source);

    /// Tokenize the entire source and return the token stream.
    /// The returned vector always ends with an END_OF_FILE token.
    std::vector<Token> tokenize();

private:
    std::string source_;    // Full source text
    size_t      pos_;       // Current byte offset into source_
    int         line_;      // Current 1-based line number
    int         column_;    // Current 1-based column number

    // ── Character access ────────────────────────────────────────────────
    char current() const;           // Character at pos_ (or '\0' at end)
    char peek() const;              // Character at pos_+1 (or '\0')
    char advance();                 // Consume and return current char
    bool isAtEnd() const;           // True when pos_ >= source_.size()

    // ── Skipping helpers ────────────────────────────────────────────────
    void skipWhitespace();          // Advance past spaces, tabs, newlines
    void skipLineComment();         // Advance past // ... \n
    void skipBlockComment();        // Advance past /* ... */

    // ── Scanning helpers ────────────────────────────────────────────────
    Token scanNumber();             // Scan a decimal integer literal
    Token scanIdentifierOrKeyword();// Scan an identifier or keyword
    Token scanToken();              // Scan an operator or delimiter
};

} // namespace mc
