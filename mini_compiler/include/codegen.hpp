// =============================================================================
// codegen.hpp — AST-to-Bytecode Code Generator Interface
// =============================================================================
// Declares the CodeGenerator class that walks the AST and emits a linear
// sequence of stack-based bytecode instructions.  Jump targets are resolved
// via a patch-up strategy: emit a placeholder, then back-patch with the
// correct address once the target location is known.
//
// Part of the C-Subset Mini-Compiler project.
// =============================================================================

#pragma once

#include "ast.hpp"
#include "bytecode.hpp"
#include <vector>
#include <stdexcept>

namespace mc {

// ─────────────────────────────────────────────────────────────────────────────
// CodeGenError — Thrown when the code generator encounters an unsupported
//                node type or an internal inconsistency.
// ─────────────────────────────────────────────────────────────────────────────
class CodeGenError : public std::runtime_error {
public:
    int line;       // Source line (from the AST node)
    int column;     // Source column

    CodeGenError(const std::string& msg, int line, int column)
        : std::runtime_error(msg), line(line), column(column) {}
};

// ─────────────────────────────────────────────────────────────────────────────
// CodeGenerator — Traverses the AST and produces a vector of Instructions.
//
// Usage:
//     CodeGenerator cg;
//     auto bytecode = cg.generate(*ast);
// ─────────────────────────────────────────────────────────────────────────────
class CodeGenerator {
public:
    CodeGenerator() = default;

    /// Walk the entire program AST and return the emitted bytecode.
    /// The final instruction is always OP_HALT.
    std::vector<Instruction> generate(const Program& program);

private:
    std::vector<Instruction> instructions_;

    // ── Emission helpers ────────────────────────────────────────────────
    /// Append an instruction to the program.
    void emit(Opcode op, int operand = 0, const std::string& var_name = "");

    /// Emit a jump with a placeholder target; return the index for patching.
    size_t emitJump(Opcode op);

    /// Back-patch a previously emitted jump's target address.
    void patchJump(size_t index, int target);

    // ── AST traversal ───────────────────────────────────────────────────
    /// Generate code for an expression node (pushes result onto stack).
    void generateExpression(const ASTNode* node);

    /// Generate code for a statement node (may push/pop, emit jumps, etc.).
    void generateStatement(const ASTNode* node);
};

} // namespace mc
