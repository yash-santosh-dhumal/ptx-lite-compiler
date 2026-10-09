// =============================================================================
// vm.cpp — Stack-based Virtual Machine Execution Engine Implementation
// =============================================================================
// Simulates a runtime virtual CPU executing the stack bytecode.
// Manages:
//   - Operand stack (with bounds checking: underflow and overflow)
//   - Variable store (dictionary of name -> 32-bit int)
//   - Instruction pointer / Program counter (PC)
//   - Diagnostic runtime errors: division/mod by zero, undeclared variables
//
// Part of the C-Subset Mini-Compiler project.
// =============================================================================

#include "vm.hpp"

namespace mc {

VM::VM(std::vector<Instruction> program, std::ostream& output)
    : program_(std::move(program))
    , pc_(0)
    , output_(output) {
    stack_.reserve(64);
}

void VM::push(int value) {
    if (stack_.size() >= MAX_STACK_SIZE) {
        throw RuntimeError("VM Error: Operand stack overflow (max capacity " +
                           std::to_string(MAX_STACK_SIZE) + " exceeded)");
    }
    stack_.push_back(value);
}

int VM::pop() {
    if (stack_.empty()) {
        throw RuntimeError("VM Error: Operand stack underflow on instruction at PC=" +
                           std::to_string(pc_));
    }
    int val = stack_.back();
    stack_.pop_back();
    return val;
}

int VM::peek() const {
    if (stack_.empty()) {
        throw RuntimeError("VM Error: Operand stack underflow peek at PC=" +
                           std::to_string(pc_));
    }
    return stack_.back();
}

void VM::execute() {
    pc_ = 0;
    while (pc_ < program_.size()) {
        const auto& instr = program_[pc_];

        switch (instr.op) {
            case Opcode::OP_PUSH_CONST: {
                push(instr.operand);
                pc_++;
                break;
            }

            case Opcode::OP_LOAD_VAR: {
                auto it = variables_.find(instr.var_name);
                if (it == variables_.end()) {
                    throw RuntimeError("VM Error: Access to undeclared variable '" +
                                       instr.var_name + "' at PC=" + std::to_string(pc_));
                }
                push(it->second);
                pc_++;
                break;
            }

            case Opcode::OP_STORE_VAR: {
                int val = pop();
                variables_[instr.var_name] = val;
                pc_++;
                break;
            }

            case Opcode::OP_ADD: {
                int b = pop();
                int a = pop();
                push(a + b);
                pc_++;
                break;
            }

            case Opcode::OP_SUB: {
                int b = pop();
                int a = pop();
                push(a - b);
                pc_++;
                break;
            }

            case Opcode::OP_MUL: {
                int b = pop();
                int a = pop();
                push(a * b);
                pc_++;
                break;
            }

            case Opcode::OP_DIV: {
                int b = pop();
                int a = pop();
                if (b == 0) {
                    throw RuntimeError("VM Error: Division by zero at PC=" + std::to_string(pc_));
                }
                push(a / b);
                pc_++;
                break;
            }

            case Opcode::OP_MOD: {
                int b = pop();
                int a = pop();
                if (b == 0) {
                    throw RuntimeError("VM Error: Modulo by zero at PC=" + std::to_string(pc_));
                }
                push(a % b);
                pc_++;
                break;
            }

            case Opcode::OP_NEG: {
                int val = pop();
                push(-val);
                pc_++;
                break;
            }

            case Opcode::OP_NOT: {
                int val = pop();
                push(val == 0 ? 1 : 0);
                pc_++;
                break;
            }

            case Opcode::OP_CMP_EQ: {
                int b = pop();
                int a = pop();
                push(a == b ? 1 : 0);
                pc_++;
                break;
            }

            case Opcode::OP_CMP_NEQ: {
                int b = pop();
                int a = pop();
                push(a != b ? 1 : 0);
                pc_++;
                break;
            }

            case Opcode::OP_CMP_LT: {
                int b = pop();
                int a = pop();
                push(a < b ? 1 : 0);
                pc_++;
                break;
            }

            case Opcode::OP_CMP_LTEQ: {
                int b = pop();
                int a = pop();
                push(a <= b ? 1 : 0);
                pc_++;
                break;
            }

            case Opcode::OP_CMP_GT: {
                int b = pop();
                int a = pop();
                push(a > b ? 1 : 0);
                pc_++;
                break;
            }

            case Opcode::OP_CMP_GTEQ: {
                int b = pop();
                int a = pop();
                push(a >= b ? 1 : 0);
                pc_++;
                break;
            }

            case Opcode::OP_AND: {
                int b = pop();
                int a = pop();
                push((a != 0 && b != 0) ? 1 : 0);
                pc_++;
                break;
            }

            case Opcode::OP_OR: {
                int b = pop();
                int a = pop();
                push((a != 0 || b != 0) ? 1 : 0);
                pc_++;
                break;
            }

            case Opcode::OP_JUMP: {
                if (instr.operand < 0 || static_cast<size_t>(instr.operand) >= program_.size()) {
                    throw RuntimeError("VM Error: Invalid jump destination address " +
                                       std::to_string(instr.operand) + " at PC=" + std::to_string(pc_));
                }
                pc_ = static_cast<size_t>(instr.operand);
                break;
            }

            case Opcode::OP_JUMP_IF_FALSE: {
                int condition = pop();
                if (condition == 0) {
                    if (instr.operand < 0 || static_cast<size_t>(instr.operand) >= program_.size()) {
                        throw RuntimeError("VM Error: Invalid conditional jump destination address " +
                                           std::to_string(instr.operand) + " at PC=" + std::to_string(pc_));
                    }
                    pc_ = static_cast<size_t>(instr.operand);
                } else {
                    pc_++;
                }
                break;
            }

            case Opcode::OP_PRINT: {
                int val = pop();
                output_ << val << "\n";
                pc_++;
                break;
            }

            case Opcode::OP_HALT: {
                return;
            }

            default:
                throw RuntimeError("VM Error: Unknown opcode encountered at PC=" + std::to_string(pc_));
        }
    }
}

} // namespace mc
