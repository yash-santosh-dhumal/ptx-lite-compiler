// =============================================================================
// parser.hpp — Recursive Descent Parser Interface
// =============================================================================
// Declares the Parser class that consumes a token stream and builds an
// Abstract Syntax Tree. Implements operator precedence via a layered
// recursive-descent approach (precedence climbing).
//
// Grammar (informal):
//   program       → statement* EOF
//   statement     → varDecl | ifStmt | whileStmt | printStmt | block | assign
//   varDecl       → "int" IDENT "=" expression ";"
//   ifStmt        → "if" "(" expression ")" block ("else" block)?
//   whileStmt     → "while" "(" expression ")" block
//   printStmt     → "print" "(" expression ")" ";"
//   block         → "{" statement* "}"
//   assign        → IDENT "=" expression ";"
//
//   expression    → logicalOr
//   logicalOr     → logicalAnd ("||" logicalAnd)*
//   logicalAnd    → equality   ("&&" equality)*
//   equality      → comparison (("==" | "!=") comparison)*
//   comparison    → additive   (("<" | "<=" | ">" | ">=") additive)*
//   additive      → multiplicative (("+" | "-") multiplicative)*
//   multiplicative→ unary      (("*" | "/" | "%") unary)*
//   unary         → ("-" | "!") unary | primary
//   primary       → INT_LITERAL | IDENT | "(" expression ")"
//
// Part of the C-Subset Mini-Compiler project.
// =============================================================================

#pragma once

#include "token.hpp"
#include "ast.hpp"
#include <vector>
#include <memory>
#include <stdexcept>

namespace mc {

// ─────────────────────────────────────────────────────────────────────────────
// ParseError — Thrown when the parser encounters a syntactic violation.
// ─────────────────────────────────────────────────────────────────────────────
class ParseError : public std::runtime_error {
public:
    int line;       // Line number where the error was detected
    int column;     // Column number where the error was detected

    ParseError(const std::string& msg, int line, int column)
        : std::runtime_error(msg), line(line), column(column) {}
};

// ─────────────────────────────────────────────────────────────────────────────
// Parser — Builds an AST from a token stream using recursive descent.
// ─────────────────────────────────────────────────────────────────────────────
class Parser {
public:
    /// Construct a parser over the given token stream.
    explicit Parser(std::vector<Token> tokens);

    /// Parse the entire token stream and return the root Program node.
    /// Throws ParseError on syntactic violations.
    std::unique_ptr<Program> parse();

private:
    std::vector<Token> tokens_;
    size_t pos_;    // Current position in the token stream

    // ── Token navigation ────────────────────────────────────────────────
    const Token& current() const;                       // Token at pos_
    const Token& peek() const;                          // Token at pos_+1
    const Token& advance();                             // Consume & return
    bool         check(TokenType type) const;           // Test without consuming
    bool         match(TokenType type);                 // Consume if matching
    const Token& expect(TokenType type,                 // Consume or throw
                        const std::string& message);

    // ── Statement parsers ───────────────────────────────────────────────
    std::unique_ptr<ASTNode> parseStatement();
    std::unique_ptr<ASTNode> parseVarDecl();
    std::unique_ptr<ASTNode> parseIfStatement();
    std::unique_ptr<ASTNode> parseWhileStatement();
    std::unique_ptr<ASTNode> parsePrintStatement();
    std::unique_ptr<ASTNode> parseBlock();
    std::unique_ptr<ASTNode> parseAssignmentOrExprStmt();

    // ── Expression parsers (precedence climbing) ────────────────────────
    std::unique_ptr<ASTNode> parseExpression();
    std::unique_ptr<ASTNode> parseLogicalOr();
    std::unique_ptr<ASTNode> parseLogicalAnd();
    std::unique_ptr<ASTNode> parseEquality();
    std::unique_ptr<ASTNode> parseComparison();
    std::unique_ptr<ASTNode> parseAdditive();
    std::unique_ptr<ASTNode> parseMultiplicative();
    std::unique_ptr<ASTNode> parseUnary();
    std::unique_ptr<ASTNode> parsePrimary();
};

} // namespace mc
