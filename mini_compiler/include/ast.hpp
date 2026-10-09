// =============================================================================
// ast.hpp — Abstract Syntax Tree Node Definitions
// =============================================================================
// Defines the full AST node hierarchy for the C-subset language:
//
//   Expressions: IntLiteralExpr, VarRefExpr, UnaryExpr, BinaryExpr
//   Statements:  VarDeclStmt, AssignStmt, BlockStmt, IfStmt, WhileStmt,
//                PrintStmt, Program (root)
//
// All child ownership is modeled with std::unique_ptr<ASTNode> — no raw
// pointer leaks, automatic cleanup via RAII. Every node records its source
// line/column for diagnostic messages downstream.
//
// Part of the C-Subset Mini-Compiler project.
// =============================================================================

#pragma once

#include <memory>
#include <vector>
#include <string>

namespace mc {

// ─────────────────────────────────────────────────────────────────────────────
// Helper: build an indentation string for pretty-printing.
// ─────────────────────────────────────────────────────────────────────────────
inline std::string astIndent(int level) {
    return std::string(static_cast<size_t>(level) * 2, ' ');
}

// ═════════════════════════════════════════════════════════════════════════════
//  BASE CLASS
// ═════════════════════════════════════════════════════════════════════════════

/// ASTNode — Abstract base for every node in the syntax tree.
struct ASTNode {
    int line   = 0;     // Source line where this construct begins
    int column = 0;     // Source column where this construct begins

    virtual ~ASTNode() = default;

    /// Pretty-print the subtree rooted at this node.
    /// @param indent  Current indentation depth (number of 2-space levels).
    virtual std::string toString(int indent = 0) const = 0;
};

// ═════════════════════════════════════════════════════════════════════════════
//  EXPRESSION NODES
// ═════════════════════════════════════════════════════════════════════════════

/// IntLiteralExpr — A 32-bit integer constant (e.g. `42`).
struct IntLiteralExpr : ASTNode {
    int value;

    IntLiteralExpr(int value, int line, int col) : value(value) {
        this->line = line;
        this->column = col;
    }

    std::string toString(int ind = 0) const override {
        return astIndent(ind) + "IntLiteral(" + std::to_string(value) + ")";
    }
};

/// VarRefExpr — A reference to a named variable (e.g. `x`).
struct VarRefExpr : ASTNode {
    std::string name;

    VarRefExpr(std::string name, int line, int col) : name(std::move(name)) {
        this->line = line;
        this->column = col;
    }

    std::string toString(int ind = 0) const override {
        return astIndent(ind) + "VarRef(" + name + ")";
    }
};

/// UnaryExpr — A prefix unary operator (e.g. `-x`, `!cond`).
struct UnaryExpr : ASTNode {
    std::string                op;       // "-" or "!"
    std::unique_ptr<ASTNode>   operand;

    UnaryExpr(std::string op, std::unique_ptr<ASTNode> operand, int line, int col)
        : op(std::move(op)), operand(std::move(operand))
    {
        this->line = line;
        this->column = col;
    }

    std::string toString(int ind = 0) const override {
        return astIndent(ind) + "Unary(" + op + ")\n"
             + operand->toString(ind + 1);
    }
};

/// BinaryExpr — A binary operation (e.g. `a + b`, `x >= 0`).
struct BinaryExpr : ASTNode {
    std::string                op;       // "+", "-", "*", "/", "%", "==", etc.
    std::unique_ptr<ASTNode>   left;
    std::unique_ptr<ASTNode>   right;

    BinaryExpr(std::string op,
               std::unique_ptr<ASTNode> left,
               std::unique_ptr<ASTNode> right,
               int line, int col)
        : op(std::move(op))
        , left(std::move(left))
        , right(std::move(right))
    {
        this->line = line;
        this->column = col;
    }

    std::string toString(int ind = 0) const override {
        return astIndent(ind) + "BinaryOp(" + op + ")\n"
             + left->toString(ind + 1) + "\n"
             + right->toString(ind + 1);
    }
};

// ═════════════════════════════════════════════════════════════════════════════
//  STATEMENT NODES
// ═════════════════════════════════════════════════════════════════════════════

/// VarDeclStmt — Variable declaration with initializer: `int x = <expr>;`
struct VarDeclStmt : ASTNode {
    std::string              name;
    std::unique_ptr<ASTNode> initializer;

    VarDeclStmt(std::string name, std::unique_ptr<ASTNode> init, int line, int col)
        : name(std::move(name)), initializer(std::move(init))
    {
        this->line = line;
        this->column = col;
    }

    std::string toString(int ind = 0) const override {
        return astIndent(ind) + "VarDecl(" + name + ")\n"
             + initializer->toString(ind + 1);
    }
};

/// AssignStmt — Variable assignment: `x = <expr>;`
struct AssignStmt : ASTNode {
    std::string              name;
    std::unique_ptr<ASTNode> value;

    AssignStmt(std::string name, std::unique_ptr<ASTNode> val, int line, int col)
        : name(std::move(name)), value(std::move(val))
    {
        this->line = line;
        this->column = col;
    }

    std::string toString(int ind = 0) const override {
        return astIndent(ind) + "Assign(" + name + ")\n"
             + value->toString(ind + 1);
    }
};

/// BlockStmt — A brace-delimited sequence of statements: `{ ... }`
struct BlockStmt : ASTNode {
    std::vector<std::unique_ptr<ASTNode>> statements;

    BlockStmt(std::vector<std::unique_ptr<ASTNode>> stmts, int line, int col)
        : statements(std::move(stmts))
    {
        this->line = line;
        this->column = col;
    }

    std::string toString(int ind = 0) const override {
        std::string result = astIndent(ind) + "Block";
        for (const auto& stmt : statements) {
            result += "\n" + stmt->toString(ind + 1);
        }
        return result;
    }
};

/// IfStmt — Conditional: `if (<cond>) { ... } else { ... }`
struct IfStmt : ASTNode {
    std::unique_ptr<ASTNode> condition;
    std::unique_ptr<ASTNode> then_branch;
    std::unique_ptr<ASTNode> else_branch;   // May be nullptr

    IfStmt(std::unique_ptr<ASTNode> cond,
           std::unique_ptr<ASTNode> then_b,
           std::unique_ptr<ASTNode> else_b,
           int line, int col)
        : condition(std::move(cond))
        , then_branch(std::move(then_b))
        , else_branch(std::move(else_b))
    {
        this->line = line;
        this->column = col;
    }

    std::string toString(int ind = 0) const override {
        std::string result = astIndent(ind) + "If\n";
        result += astIndent(ind + 1) + "Condition:\n" + condition->toString(ind + 2) + "\n";
        result += astIndent(ind + 1) + "Then:\n" + then_branch->toString(ind + 2);
        if (else_branch) {
            result += "\n" + astIndent(ind + 1) + "Else:\n" + else_branch->toString(ind + 2);
        }
        return result;
    }
};

/// WhileStmt — Loop: `while (<cond>) { ... }`
struct WhileStmt : ASTNode {
    std::unique_ptr<ASTNode> condition;
    std::unique_ptr<ASTNode> body;

    WhileStmt(std::unique_ptr<ASTNode> cond,
              std::unique_ptr<ASTNode> body,
              int line, int col)
        : condition(std::move(cond))
        , body(std::move(body))
    {
        this->line = line;
        this->column = col;
    }

    std::string toString(int ind = 0) const override {
        std::string result = astIndent(ind) + "While\n";
        result += astIndent(ind + 1) + "Condition:\n" + condition->toString(ind + 2) + "\n";
        result += astIndent(ind + 1) + "Body:\n" + body->toString(ind + 2);
        return result;
    }
};

/// PrintStmt — Output statement: `print(<expr>);`
struct PrintStmt : ASTNode {
    std::unique_ptr<ASTNode> expression;

    PrintStmt(std::unique_ptr<ASTNode> expr, int line, int col)
        : expression(std::move(expr))
    {
        this->line = line;
        this->column = col;
    }

    std::string toString(int ind = 0) const override {
        return astIndent(ind) + "Print\n"
             + expression->toString(ind + 1);
    }
};

// ═════════════════════════════════════════════════════════════════════════════
//  PROGRAM ROOT
// ═════════════════════════════════════════════════════════════════════════════

/// Program — The top-level AST node containing all statements.
struct Program : ASTNode {
    std::vector<std::unique_ptr<ASTNode>> statements;

    explicit Program(std::vector<std::unique_ptr<ASTNode>> stmts)
        : statements(std::move(stmts)) {}

    std::string toString(int ind = 0) const override {
        std::string result = astIndent(ind) + "Program";
        for (const auto& stmt : statements) {
            result += "\n" + stmt->toString(ind + 1);
        }
        return result;
    }
};

} // namespace mc
