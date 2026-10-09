// =============================================================================
// codegen.cpp — AST-to-Bytecode Code Generator Implementation
// =============================================================================
// Traverses the AST depth-first and emits a flat sequence of bytecode
// instructions.  Expression nodes push their result onto the operand stack;
// statement nodes orchestrate control flow via OP_JUMP / OP_JUMP_IF_FALSE
// with a patch-up strategy for forward references.
//
// Also implements the bytecode disassembler (opcodeToString, disassemble)
// declared in bytecode.hpp.
//
// Part of the C-Subset Mini-Compiler project.
// =============================================================================

#include "codegen.hpp"
#include <iomanip>

namespace mc {

// ═════════════════════════════════════════════════════════════════════════════
//  Bytecode utility implementations  (declared in bytecode.hpp)
// ═════════════════════════════════════════════════════════════════════════════

const char* opcodeToString(Opcode op) {
    switch (op) {
        case Opcode::OP_PUSH_CONST:    return "OP_PUSH_CONST";
        case Opcode::OP_LOAD_VAR:      return "OP_LOAD_VAR";
        case Opcode::OP_STORE_VAR:     return "OP_STORE_VAR";
        case Opcode::OP_ADD:           return "OP_ADD";
        case Opcode::OP_SUB:           return "OP_SUB";
        case Opcode::OP_MUL:           return "OP_MUL";
        case Opcode::OP_DIV:           return "OP_DIV";
        case Opcode::OP_MOD:           return "OP_MOD";
        case Opcode::OP_NEG:           return "OP_NEG";
        case Opcode::OP_NOT:           return "OP_NOT";
        case Opcode::OP_CMP_EQ:        return "OP_CMP_EQ";
        case Opcode::OP_CMP_NEQ:       return "OP_CMP_NEQ";
        case Opcode::OP_CMP_LT:        return "OP_CMP_LT";
        case Opcode::OP_CMP_LTEQ:      return "OP_CMP_LTEQ";
        case Opcode::OP_CMP_GT:        return "OP_CMP_GT";
        case Opcode::OP_CMP_GTEQ:      return "OP_CMP_GTEQ";
        case Opcode::OP_AND:           return "OP_AND";
        case Opcode::OP_OR:            return "OP_OR";
        case Opcode::OP_JUMP:          return "OP_JUMP";
        case Opcode::OP_JUMP_IF_FALSE: return "OP_JUMP_IF_FALSE";
        case Opcode::OP_PRINT:         return "OP_PRINT";
        case Opcode::OP_HALT:          return "OP_HALT";
        default:                       return "UNKNOWN";
    }
}

void disassemble(const std::vector<Instruction>& program, std::ostream& os) {
    for (size_t i = 0; i < program.size(); ++i) {
        const auto& instr = program[i];
        os << std::setw(4) << std::setfill('0') << i
           << "  " << std::setfill(' ') << std::left << std::setw(20)
           << opcodeToString(instr.op);

        switch (instr.op) {
            case Opcode::OP_PUSH_CONST:
                os << " " << instr.operand;
                break;
            case Opcode::OP_LOAD_VAR:
            case Opcode::OP_STORE_VAR:
                os << " '" << instr.var_name << "'";
                break;
            case Opcode::OP_JUMP:
            case Opcode::OP_JUMP_IF_FALSE:
                os << " -> " << std::setw(4) << std::setfill('0') << instr.operand;
                break;
            default:
                break;
        }
        os << std::setfill(' ') << "\n";
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  CodeGenerator — public interface
// ═════════════════════════════════════════════════════════════════════════════

std::vector<Instruction> CodeGenerator::generate(const Program& program) {
    instructions_.clear();
    for (const auto& stmt : program.statements) {
        generateStatement(stmt.get());
    }
    emit(Opcode::OP_HALT);
    return instructions_;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Emission helpers
// ═════════════════════════════════════════════════════════════════════════════

void CodeGenerator::emit(Opcode op, int operand, const std::string& var_name) {
    instructions_.emplace_back(op, operand, var_name);
}

size_t CodeGenerator::emitJump(Opcode op) {
    // Emit a jump with a placeholder (-1) target that will be patched later.
    instructions_.emplace_back(op, -1);
    return instructions_.size() - 1;
}

void CodeGenerator::patchJump(size_t index, int target) {
    instructions_[index].operand = target;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Expression code generation
// ═════════════════════════════════════════════════════════════════════════════
// Each expression node pushes exactly one value onto the operand stack.

void CodeGenerator::generateExpression(const ASTNode* node) {
    // ── Integer literal ─────────────────────────────────────────────────
    if (const auto* p = dynamic_cast<const IntLiteralExpr*>(node)) {
        emit(Opcode::OP_PUSH_CONST, p->value);
        return;
    }

    // ── Variable reference ──────────────────────────────────────────────
    if (const auto* p = dynamic_cast<const VarRefExpr*>(node)) {
        emit(Opcode::OP_LOAD_VAR, 0, p->name);
        return;
    }

    // ── Unary operator ──────────────────────────────────────────────────
    if (const auto* p = dynamic_cast<const UnaryExpr*>(node)) {
        generateExpression(p->operand.get());
        if      (p->op == "-") emit(Opcode::OP_NEG);
        else if (p->op == "!") emit(Opcode::OP_NOT);
        else throw CodeGenError("Unknown unary operator: " + p->op,
                                p->line, p->column);
        return;
    }

    // ── Binary operator ─────────────────────────────────────────────────
    if (const auto* p = dynamic_cast<const BinaryExpr*>(node)) {
        // Evaluate left, then right — stack order: [left, right] with right on top
        generateExpression(p->left.get());
        generateExpression(p->right.get());

        // Map the operator string to the corresponding opcode
        if      (p->op == "+")  emit(Opcode::OP_ADD);
        else if (p->op == "-")  emit(Opcode::OP_SUB);
        else if (p->op == "*")  emit(Opcode::OP_MUL);
        else if (p->op == "/")  emit(Opcode::OP_DIV);
        else if (p->op == "%")  emit(Opcode::OP_MOD);
        else if (p->op == "==") emit(Opcode::OP_CMP_EQ);
        else if (p->op == "!=") emit(Opcode::OP_CMP_NEQ);
        else if (p->op == "<")  emit(Opcode::OP_CMP_LT);
        else if (p->op == "<=") emit(Opcode::OP_CMP_LTEQ);
        else if (p->op == ">")  emit(Opcode::OP_CMP_GT);
        else if (p->op == ">=") emit(Opcode::OP_CMP_GTEQ);
        else if (p->op == "&&") emit(Opcode::OP_AND);
        else if (p->op == "||") emit(Opcode::OP_OR);
        else throw CodeGenError("Unknown binary operator: " + p->op,
                                p->line, p->column);
        return;
    }

    throw CodeGenError("Unknown expression node type", node->line, node->column);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Statement code generation
// ═════════════════════════════════════════════════════════════════════════════

void CodeGenerator::generateStatement(const ASTNode* node) {
    // ── Variable declaration ────────────────────────────────────────────
    if (const auto* p = dynamic_cast<const VarDeclStmt*>(node)) {
        generateExpression(p->initializer.get());
        emit(Opcode::OP_STORE_VAR, 0, p->name);
        return;
    }

    // ── Assignment ──────────────────────────────────────────────────────
    if (const auto* p = dynamic_cast<const AssignStmt*>(node)) {
        generateExpression(p->value.get());
        emit(Opcode::OP_STORE_VAR, 0, p->name);
        return;
    }

    // ── Print ───────────────────────────────────────────────────────────
    if (const auto* p = dynamic_cast<const PrintStmt*>(node)) {
        generateExpression(p->expression.get());
        emit(Opcode::OP_PRINT);
        return;
    }

    // ── Block ───────────────────────────────────────────────────────────
    if (const auto* p = dynamic_cast<const BlockStmt*>(node)) {
        for (const auto& stmt : p->statements) {
            generateStatement(stmt.get());
        }
        return;
    }

    // ── If / If-Else ────────────────────────────────────────────────────
    //
    //  Layout with else:          Layout without else:
    //    <condition>                 <condition>
    //    JUMP_IF_FALSE → else        JUMP_IF_FALSE → end
    //    <then body>                 <then body>
    //    JUMP → end               end:
    //  else:
    //    <else body>
    //  end:
    //
    if (const auto* p = dynamic_cast<const IfStmt*>(node)) {
        generateExpression(p->condition.get());
        size_t jumpToElse = emitJump(Opcode::OP_JUMP_IF_FALSE);

        generateStatement(p->then_branch.get());

        if (p->else_branch) {
            size_t jumpOverElse = emitJump(Opcode::OP_JUMP);
            patchJump(jumpToElse, static_cast<int>(instructions_.size()));
            generateStatement(p->else_branch.get());
            patchJump(jumpOverElse, static_cast<int>(instructions_.size()));
        } else {
            patchJump(jumpToElse, static_cast<int>(instructions_.size()));
        }
        return;
    }

    // ── While loop ──────────────────────────────────────────────────────
    //
    //  Layout:
    //  loopStart:
    //    <condition>
    //    JUMP_IF_FALSE → loopEnd
    //    <body>
    //    JUMP → loopStart
    //  loopEnd:
    //
    if (const auto* p = dynamic_cast<const WhileStmt*>(node)) {
        int loopStart = static_cast<int>(instructions_.size());

        generateExpression(p->condition.get());
        size_t jumpToEnd = emitJump(Opcode::OP_JUMP_IF_FALSE);

        generateStatement(p->body.get());
        emit(Opcode::OP_JUMP, loopStart);

        patchJump(jumpToEnd, static_cast<int>(instructions_.size()));
        return;
    }

    // ── Program root ────────────────────────────────────────────────────
    if (const auto* p = dynamic_cast<const Program*>(node)) {
        for (const auto& stmt : p->statements) {
            generateStatement(stmt.get());
        }
        return;
    }

    throw CodeGenError("Unknown statement node type", node->line, node->column);
}

} // namespace mc
