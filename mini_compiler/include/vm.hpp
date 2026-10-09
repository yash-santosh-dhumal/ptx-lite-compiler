// =============================================================================
// vm.hpp — Stack-based Virtual Machine Execution Engine Interface
// =============================================================================
// Declares the VM class that executes bytecode instructions.  The VM
// maintains:
//   • An operand stack  (bounded to MAX_STACK_SIZE for overflow protection)
//   • A program counter (PC)
//   • A variable memory store  (string-keyed map for named variables)
//
// Runtime safety:  division-by-zero detection, stack overflow/underflow
// guards, and undeclared-variable checks all throw RuntimeError.
//
// Part of the C-Subset Mini-Compiler project.
// =============================================================================

#pragma once

#include "bytecode.hpp"
#include <vector>
#include <unordered_map>
#include <string>
#include <stdexcept>
#include <iostream>

namespace mc {

// ─────────────────────────────────────────────────────────────────────────────
// RuntimeError — Thrown when a program violates runtime invariants.
// ─────────────────────────────────────────────────────────────────────────────
class RuntimeError : public std::runtime_error {
public:
    explicit RuntimeError(const std::string& msg)
        : std::runtime_error(msg) {}
};

// ─────────────────────────────────────────────────────────────────────────────
// VM — Fetch-decode-execute loop over the bytecode instruction stream.
// ─────────────────────────────────────────────────────────────────────────────
class VM {
public:
    /// Maximum operand stack depth (safety bound).
    static constexpr size_t MAX_STACK_SIZE = 1024;

    /// Construct a VM loaded with the given program.
    /// @param program  The bytecode to execute.
    /// @param output   The output stream for OP_PRINT (default: std::cout).
    explicit VM(std::vector<Instruction> program,
                std::ostream& output = std::cout);

    /// Run the program until OP_HALT or end-of-program.
    /// Throws RuntimeError on any safety violation.
    void execute();

private:
    std::vector<Instruction>                program_;       // Bytecode image
    std::vector<int>                        stack_;         // Operand stack
    std::unordered_map<std::string, int>    variables_;     // Variable memory
    size_t                                  pc_;            // Program counter
    std::ostream&                           output_;        // Print target

    // ── Stack operations ────────────────────────────────────────────────
    void push(int value);       // Push with overflow check
    int  pop();                 // Pop  with underflow check
    int  peek() const;          // Peek with underflow check
};

} // namespace mc
