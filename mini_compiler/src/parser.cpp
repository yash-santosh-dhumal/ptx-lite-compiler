// =============================================================================
// parser.cpp — Recursive Descent Parser Implementation
// =============================================================================
// Consumes a token stream and builds an Abstract Syntax Tree using
// recursive descent with precedence climbing for expressions.
//
// Precedence levels (low → high):
//   1. Logical OR   (||)
//   2. Logical AND  (&&)
//   3. Equality     (== !=)
//   4. Comparison   (< <= > >=)
//   5. Additive     (+ -)
//   6. Multiplicative (* / %)
//   7. Unary        (- !)
//   8. Primary      (INT_LITERAL, IDENT, grouped)
//
// Each level is a separate function that calls the next-higher-precedence
// function, naturally encoding left-to-right associativity.
//
// Part of the C-Subset Mini-Compiler project.
// =============================================================================

#include "parser.hpp"

namespace mc {

// ═════════════════════════════════════════════════════════════════════════════
//  Construction
// ═════════════════════════════════════════════════════════════════════════════

Parser::Parser(std::vector<Token> tokens)
    : tokens_(std::move(tokens))
    , pos_(0)
{}

// ═════════════════════════════════════════════════════════════════════════════
//  Token navigation
// ═════════════════════════════════════════════════════════════════════════════

const Token& Parser::current() const {
    return tokens_[pos_];
}

const Token& Parser::peek() const {
    if (pos_ + 1 < tokens_.size()) return tokens_[pos_ + 1];
    return tokens_.back(); // EOF sentinel
}

const Token& Parser::advance() {
    const Token& tok = tokens_[pos_];
    if (pos_ < tokens_.size() - 1) {
        pos_++;
    }
    return tok;
}

bool Parser::check(TokenType type) const {
    return current().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

const Token& Parser::expect(TokenType type, const std::string& message) {
    if (check(type)) {
        return advance();
    }
    throw ParseError(
        message + " (got '" + current().lexeme + "')",
        current().line, current().column);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Top-level entry point
// ═════════════════════════════════════════════════════════════════════════════

std::unique_ptr<Program> Parser::parse() {
    std::vector<std::unique_ptr<ASTNode>> statements;
    while (!check(TokenType::END_OF_FILE)) {
        statements.push_back(parseStatement());
    }
    return std::make_unique<Program>(std::move(statements));
}

// ═════════════════════════════════════════════════════════════════════════════
//  Statement parsers
// ═════════════════════════════════════════════════════════════════════════════

std::unique_ptr<ASTNode> Parser::parseStatement() {
    if (check(TokenType::KW_INT))   return parseVarDecl();
    if (check(TokenType::KW_IF))    return parseIfStatement();
    if (check(TokenType::KW_WHILE)) return parseWhileStatement();
    if (check(TokenType::KW_PRINT)) return parsePrintStatement();
    if (check(TokenType::LBRACE))   return parseBlock();
    return parseAssignmentOrExprStmt();
}

// ── int <name> = <expr> ; ───────────────────────────────────────────────────

std::unique_ptr<ASTNode> Parser::parseVarDecl() {
    const Token& kw   = advance();  // consume "int"
    const Token& name = expect(TokenType::IDENTIFIER,
                               "Expected variable name after 'int'");
    expect(TokenType::ASSIGN, "Expected '=' in variable declaration");
    auto init = parseExpression();
    expect(TokenType::SEMICOLON, "Expected ';' after variable declaration");
    return std::make_unique<VarDeclStmt>(
        name.lexeme, std::move(init), kw.line, kw.column);
}

// ── if ( <expr> ) <block> [ else <block> ] ──────────────────────────────────

std::unique_ptr<ASTNode> Parser::parseIfStatement() {
    const Token& kw = advance();    // consume "if"
    expect(TokenType::LPAREN, "Expected '(' after 'if'");
    auto condition = parseExpression();
    expect(TokenType::RPAREN, "Expected ')' after if condition");
    auto then_branch = parseBlock();

    std::unique_ptr<ASTNode> else_branch = nullptr;
    if (match(TokenType::KW_ELSE)) {
        else_branch = parseBlock();
    }

    return std::make_unique<IfStmt>(
        std::move(condition),
        std::move(then_branch),
        std::move(else_branch),
        kw.line, kw.column);
}

// ── while ( <expr> ) <block> ────────────────────────────────────────────────

std::unique_ptr<ASTNode> Parser::parseWhileStatement() {
    const Token& kw = advance();    // consume "while"
    expect(TokenType::LPAREN, "Expected '(' after 'while'");
    auto condition = parseExpression();
    expect(TokenType::RPAREN, "Expected ')' after while condition");
    auto body = parseBlock();

    return std::make_unique<WhileStmt>(
        std::move(condition), std::move(body), kw.line, kw.column);
}

// ── print ( <expr> ) ; ──────────────────────────────────────────────────────

std::unique_ptr<ASTNode> Parser::parsePrintStatement() {
    const Token& kw = advance();    // consume "print"
    expect(TokenType::LPAREN, "Expected '(' after 'print'");
    auto expr = parseExpression();
    expect(TokenType::RPAREN, "Expected ')' after print expression");
    expect(TokenType::SEMICOLON, "Expected ';' after print statement");
    return std::make_unique<PrintStmt>(std::move(expr), kw.line, kw.column);
}

// ── { <statement>* } ────────────────────────────────────────────────────────

std::unique_ptr<ASTNode> Parser::parseBlock() {
    const Token& brace = expect(TokenType::LBRACE, "Expected '{'");
    std::vector<std::unique_ptr<ASTNode>> stmts;
    while (!check(TokenType::RBRACE) && !check(TokenType::END_OF_FILE)) {
        stmts.push_back(parseStatement());
    }
    expect(TokenType::RBRACE, "Expected '}'");
    return std::make_unique<BlockStmt>(std::move(stmts), brace.line, brace.column);
}

// ── <name> = <expr> ; ───────────────────────────────────────────────────────
// Falls through to a descriptive error if the token sequence doesn't match
// any known statement form.

std::unique_ptr<ASTNode> Parser::parseAssignmentOrExprStmt() {
    // Look ahead to see if this is IDENT = expr ;
    if (check(TokenType::IDENTIFIER) &&
        pos_ + 1 < tokens_.size() &&
        tokens_[pos_ + 1].type == TokenType::ASSIGN)
    {
        const Token& name = advance();  // consume identifier
        advance();                      // consume '='
        auto value = parseExpression();
        expect(TokenType::SEMICOLON, "Expected ';' after assignment");
        return std::make_unique<AssignStmt>(
            name.lexeme, std::move(value), name.line, name.column);
    }
    throw ParseError(
        "Unexpected token '" + current().lexeme + "': expected a statement",
        current().line, current().column);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Expression parsers — precedence climbing
// ═════════════════════════════════════════════════════════════════════════════

std::unique_ptr<ASTNode> Parser::parseExpression() {
    return parseLogicalOr();
}

// ── Level 1: || ─────────────────────────────────────────────────────────────
std::unique_ptr<ASTNode> Parser::parseLogicalOr() {
    auto left = parseLogicalAnd();
    while (check(TokenType::OR)) {
        const Token& op = advance();
        auto right = parseLogicalAnd();
        left = std::make_unique<BinaryExpr>(
            "||", std::move(left), std::move(right), op.line, op.column);
    }
    return left;
}

// ── Level 2: && ─────────────────────────────────────────────────────────────
std::unique_ptr<ASTNode> Parser::parseLogicalAnd() {
    auto left = parseEquality();
    while (check(TokenType::AND)) {
        const Token& op = advance();
        auto right = parseEquality();
        left = std::make_unique<BinaryExpr>(
            "&&", std::move(left), std::move(right), op.line, op.column);
    }
    return left;
}

// ── Level 3: == != ──────────────────────────────────────────────────────────
std::unique_ptr<ASTNode> Parser::parseEquality() {
    auto left = parseComparison();
    while (check(TokenType::EQ) || check(TokenType::NEQ)) {
        const Token& op = advance();
        auto right = parseComparison();
        left = std::make_unique<BinaryExpr>(
            op.lexeme, std::move(left), std::move(right), op.line, op.column);
    }
    return left;
}

// ── Level 4: < <= > >= ──────────────────────────────────────────────────────
std::unique_ptr<ASTNode> Parser::parseComparison() {
    auto left = parseAdditive();
    while (check(TokenType::LT) || check(TokenType::LTEQ) ||
           check(TokenType::GT) || check(TokenType::GTEQ))
    {
        const Token& op = advance();
        auto right = parseAdditive();
        left = std::make_unique<BinaryExpr>(
            op.lexeme, std::move(left), std::move(right), op.line, op.column);
    }
    return left;
}

// ── Level 5: + - ───────────────────────────────────────────────────────────
std::unique_ptr<ASTNode> Parser::parseAdditive() {
    auto left = parseMultiplicative();
    while (check(TokenType::PLUS) || check(TokenType::MINUS)) {
        const Token& op = advance();
        auto right = parseMultiplicative();
        left = std::make_unique<BinaryExpr>(
            op.lexeme, std::move(left), std::move(right), op.line, op.column);
    }
    return left;
}

// ── Level 6: * / % ─────────────────────────────────────────────────────────
std::unique_ptr<ASTNode> Parser::parseMultiplicative() {
    auto left = parseUnary();
    while (check(TokenType::STAR) || check(TokenType::SLASH) ||
           check(TokenType::PERCENT))
    {
        const Token& op = advance();
        auto right = parseUnary();
        left = std::make_unique<BinaryExpr>(
            op.lexeme, std::move(left), std::move(right), op.line, op.column);
    }
    return left;
}

// ── Level 7: - ! (prefix unary) ────────────────────────────────────────────
std::unique_ptr<ASTNode> Parser::parseUnary() {
    if (check(TokenType::MINUS)) {
        const Token& op = advance();
        auto operand = parseUnary();    // Right-recursive for right-associativity
        return std::make_unique<UnaryExpr>(
            "-", std::move(operand), op.line, op.column);
    }
    if (check(TokenType::BANG)) {
        const Token& op = advance();
        auto operand = parseUnary();
        return std::make_unique<UnaryExpr>(
            "!", std::move(operand), op.line, op.column);
    }
    return parsePrimary();
}

// ── Level 8: INT_LITERAL, IDENT, ( expr ) ──────────────────────────────────
std::unique_ptr<ASTNode> Parser::parsePrimary() {
    // Integer literal
    if (check(TokenType::INT_LITERAL)) {
        const Token& tok = advance();
        return std::make_unique<IntLiteralExpr>(tok.int_value, tok.line, tok.column);
    }

    // Variable reference
    if (check(TokenType::IDENTIFIER)) {
        const Token& tok = advance();
        return std::make_unique<VarRefExpr>(tok.lexeme, tok.line, tok.column);
    }

    // Grouped (parenthesized) expression
    if (match(TokenType::LPAREN)) {
        auto expr = parseExpression();
        expect(TokenType::RPAREN, "Expected ')' after expression");
        return expr;
    }

    throw ParseError(
        "Expected expression, got '" + current().lexeme + "'",
        current().line, current().column);
}

} // namespace mc
