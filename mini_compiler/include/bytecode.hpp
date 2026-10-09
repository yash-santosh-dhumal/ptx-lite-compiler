// =============================================================================
// bytecode.hpp — Opcode Enum & Instruction Struct (Virtual ISA)
// =============================================================================
// Defines the stack-based virtual instruction set used as the intermediate
// representation (IR) between the AST and execution.  Conceptually similar
// to how NVIDIA's PTX serves as a virtual ISA between CUDA and hardware —
// this bytecode is target-independent and can be executed by our VM.
//
// Instruction format:
//   { opcode, integer_operand, variable_name }
//   • integer_operand is used for constants and jump targets.
//   • variable_name   is used for LOAD_VAR / STORE_VAR.
//
// Part of the C-Subset Mini-Compiler project.
// =============================================================================

#pragma once

#include <string>
#include <vector>
#include <ostream>

namespace mc {

// ─────────────────────────────────────────────────────────────────────────────
// Opcode — Every virtual instruction the code generator can emit.
// ─────────────────────────────────────────────────────────────────────────────
enum class Opcode {
    // ── Data movement ───────────────────────────────────────────────────
    OP_PUSH_CONST,          // Push an integer constant onto the operand stack
    OP_LOAD_VAR,            // Load a variable's value onto the stack
    OP_STORE_VAR,           // Pop the top of stack and store into a variable

    // ── Arithmetic ──────────────────────────────────────────────────────
    OP_ADD,                 // Pop two, push (second + top)
    OP_SUB,                 // Pop two, push (second - top)
    OP_MUL,                 // Pop two, push (second * top)
    OP_DIV,                 // Pop two, push (second / top)  — checks div-by-zero
    OP_MOD,                 // Pop two, push (second % top)  — checks mod-by-zero
    OP_NEG,                 // Negate the top of stack

    // ── Logical ─────────────────────────────────────────────────────────
    OP_NOT,                 // Logical NOT: push (top == 0 ? 1 : 0)
    OP_AND,                 // Logical AND: push ((a != 0 && b != 0) ? 1 : 0)
    OP_OR,                  // Logical OR:  push ((a != 0 || b != 0) ? 1 : 0)

    // ── Comparison ──────────────────────────────────────────────────────
    OP_CMP_EQ,              // Pop two, push 1 if equal,     else 0
    OP_CMP_NEQ,             // Pop two, push 1 if not equal, else 0
    OP_CMP_LT,              // Pop two, push 1 if second <  top, else 0
    OP_CMP_LTEQ,            // Pop two, push 1 if second <= top, else 0
    OP_CMP_GT,              // Pop two, push 1 if second >  top, else 0
    OP_CMP_GTEQ,            // Pop two, push 1 if second >= top, else 0

    // ── Control flow ────────────────────────────────────────────────────
    OP_JUMP,                // Unconditional jump to operand address
    OP_JUMP_IF_FALSE,       // Pop top; if 0, jump to operand address

    // ── I/O & lifecycle ─────────────────────────────────────────────────
    OP_PRINT,               // Pop top and print its value followed by newline
    OP_HALT                 // Terminate execution
};

// ─────────────────────────────────────────────────────────────────────────────
// Instruction — A single bytecode instruction with optional operands.
// ─────────────────────────────────────────────────────────────────────────────
struct Instruction {
    Opcode      op;
    int         operand;    // Integer operand (constants, jump targets)
    std::string var_name;   // Variable name  (for LOAD_VAR, STORE_VAR)

    Instruction(Opcode op, int operand = 0, std::string var_name = "")
        : op(op), operand(operand), var_name(std::move(var_name)) {}
};

// ─────────────────────────────────────────────────────────────────────────────
// Utility: Convert an Opcode to its mnemonic string.
// ─────────────────────────────────────────────────────────────────────────────
const char* opcodeToString(Opcode op);

// ─────────────────────────────────────────────────────────────────────────────
// Disassemble a bytecode program to the given output stream.
// Format: "  ADDR  OPCODE  [operand]"
// ─────────────────────────────────────────────────────────────────────────────
void disassemble(const std::vector<Instruction>& program, std::ostream& os);

} // namespace mc
