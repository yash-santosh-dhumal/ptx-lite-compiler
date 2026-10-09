#!/usr/bin/env python3
"""
=============================================================================
reference_compiler.py — Python 3 Reference Implementation & Golden Model
=============================================================================
Mirror implementation of the C++ Mini-Compiler pipeline:
  Stage 1: Lexer (Token classification, comments, line/col tracking)
  Stage 2: Parser (Recursive descent with precedence climbing, AST nodes)
  Stage 3: CodeGen (Stack-based virtual ISA bytecode emission & jump patching)
  Stage 4: Virtual Machine (Stack CPU, variable environment, bounds checking)

Serves as:
  1. The golden architectural reference model for differential compiler verification.
  2. Fallback verification engine when host environments enforce strict native
     execution policies (e.g. Windows AppControl / WDAC).

Part of the C-Subset Mini-Compiler project.
=============================================================================
"""

import sys
import enum
from typing import List, Optional, Tuple, Dict


# =============================================================================
# Stage 1: Lexer & Tokens
# =============================================================================

class TokenType(enum.Enum):
    INT_LITERAL = "INT_LITERAL"
    IDENTIFIER = "IDENTIFIER"
    KW_INT = "KW_INT"
    KW_IF = "KW_IF"
    KW_ELSE = "KW_ELSE"
    KW_WHILE = "KW_WHILE"
    KW_PRINT = "KW_PRINT"
    PLUS = "PLUS"
    MINUS = "MINUS"
    STAR = "STAR"
    SLASH = "SLASH"
    PERCENT = "PERCENT"
    ASSIGN = "ASSIGN"
    EQ = "EQ"
    NEQ = "NEQ"
    LT = "LT"
    LTEQ = "LTEQ"
    GT = "GT"
    GTEQ = "GTEQ"
    AND = "AND"
    OR = "OR"
    BANG = "BANG"
    LPAREN = "LPAREN"
    RPAREN = "RPAREN"
    LBRACE = "LBRACE"
    RBRACE = "RBRACE"
    SEMICOLON = "SEMICOLON"
    EOF = "EOF"


KEYWORDS = {
    "int": TokenType.KW_INT,
    "if": TokenType.KW_IF,
    "else": TokenType.KW_ELSE,
    "while": TokenType.KW_WHILE,
    "print": TokenType.KW_PRINT,
}


class Token:
    def __init__(self, t_type: TokenType, lexeme: str, line: int, col: int, int_val: int = 0):
        self.type = t_type
        self.lexeme = lexeme
        self.line = line
        self.col = col
        self.int_val = int_val

    def __repr__(self):
        return f"[{self.line}:{self.col}] {self.type.value} '{self.lexeme}'"


class Lexer:
    def __init__(self, source: str):
        self.source = source
        self.pos = 0
        self.line = 1
        self.col = 1

    def current(self) -> str:
        return self.source[self.pos] if self.pos < len(self.source) else "\0"

    def peek(self) -> str:
        return self.source[self.pos + 1] if self.pos + 1 < len(self.source) else "\0"

    def advance(self) -> str:
        c = self.source[self.pos]
        self.pos += 1
        if c == "\n":
            self.line += 1
            self.col = 1
        else:
            self.col += 1
        return c

    def is_at_end(self) -> bool:
        return self.pos >= len(self.source)

    def tokenize(self) -> List[Token]:
        tokens = []
        while not self.is_at_end():
            # Skip whitespace
            while not self.is_at_end() and self.current().isspace():
                self.advance()
            if self.is_at_end():
                break

            # Comments
            if self.current() == "/" and self.peek() == "/":
                while not self.is_at_end() and self.current() != "\n":
                    self.advance()
                continue

            if self.current() == "/" and self.peek() == "*":
                start_l, start_c = self.line, self.col
                self.advance(); self.advance()
                closed = False
                while not self.is_at_end():
                    if self.current() == "*" and self.peek() == "/":
                        self.advance(); self.advance()
                        closed = True
                        break
                    self.advance()
                if not closed:
                    raise SyntaxError(f"Lexer Error [{start_l}:{start_c}]: Unterminated block comment")
                continue

            # Numbers
            if self.current().isdigit():
                start_c = self.col
                digits = ""
                while not self.is_at_end() and self.current().isdigit():
                    digits += self.advance()
                tokens.append(Token(TokenType.INT_LITERAL, digits, self.line, start_c, int(digits)))
                continue

            # Identifiers / Keywords
            if self.current().isalpha() or self.current() == "_":
                start_c = self.col
                ident = ""
                while not self.is_at_end() and (self.current().isalnum() or self.current() == "_"):
                    ident += self.advance()
                kw = KEYWORDS.get(ident, TokenType.IDENTIFIER)
                tokens.append(Token(kw, ident, self.line, start_c))
                continue

            # Operators & Delimiters
            start_l, start_c = self.line, self.col
            c = self.advance()

            if c == "+": tokens.append(Token(TokenType.PLUS, "+", start_l, start_c))
            elif c == "-": tokens.append(Token(TokenType.MINUS, "-", start_l, start_c))
            elif c == "*": tokens.append(Token(TokenType.STAR, "*", start_l, start_c))
            elif c == "/": tokens.append(Token(TokenType.SLASH, "/", start_l, start_c))
            elif c == "%": tokens.append(Token(TokenType.PERCENT, "%", start_l, start_c))
            elif c == "(": tokens.append(Token(TokenType.LPAREN, "(", start_l, start_c))
            elif c == ")": tokens.append(Token(TokenType.RPAREN, ")", start_l, start_c))
            elif c == "{": tokens.append(Token(TokenType.LBRACE, "{", start_l, start_c))
            elif c == "}": tokens.append(Token(TokenType.RBRACE, "}", start_l, start_c))
            elif c == ";": tokens.append(Token(TokenType.SEMICOLON, ";", start_l, start_c))
            elif c == "=":
                if self.current() == "=":
                    self.advance()
                    tokens.append(Token(TokenType.EQ, "==", start_l, start_c))
                else:
                    tokens.append(Token(TokenType.ASSIGN, "=", start_l, start_c))
            elif c == "!":
                if self.current() == "=":
                    self.advance()
                    tokens.append(Token(TokenType.NEQ, "!=", start_l, start_c))
                else:
                    tokens.append(Token(TokenType.BANG, "!", start_l, start_c))
            elif c == "<":
                if self.current() == "=":
                    self.advance()
                    tokens.append(Token(TokenType.LTEQ, "<=", start_l, start_c))
                else:
                    tokens.append(Token(TokenType.LT, "<", start_l, start_c))
            elif c == ">":
                if self.current() == "=":
                    self.advance()
                    tokens.append(Token(TokenType.GTEQ, ">=", start_l, start_c))
                else:
                    tokens.append(Token(TokenType.GT, ">", start_l, start_c))
            elif c == "&" and self.current() == "&":
                self.advance()
                tokens.append(Token(TokenType.AND, "&&", start_l, start_c))
            elif c == "|" and self.current() == "|":
                self.advance()
                tokens.append(Token(TokenType.OR, "||", start_l, start_c))
            else:
                raise SyntaxError(f"Lexer Error [{start_l}:{start_c}]: Unexpected character '{c}'")

        tokens.append(Token(TokenType.EOF, "", self.line, self.col))
        return tokens


# =============================================================================
# Stage 2: AST Nodes & Recursive-Descent Parser
# =============================================================================

class ASTNode:
    def __init__(self, line: int = 0, col: int = 0):
        self.line = line
        self.col = col

    def to_string(self, indent: int = 0) -> str:
        return " " * (indent * 2) + self.__class__.__name__

class IntLiteralExpr(ASTNode):
    def __init__(self, val: int, line: int, col: int):
        super().__init__(line, col)
        self.value = val

    def to_string(self, indent: int = 0) -> str:
        return " " * (indent * 2) + f"IntLiteral({self.value})"

class VarRefExpr(ASTNode):
    def __init__(self, name: str, line: int, col: int):
        super().__init__(line, col)
        self.name = name

    def to_string(self, indent: int = 0) -> str:
        return " " * (indent * 2) + f"VarRef({self.name})"

class UnaryExpr(ASTNode):
    def __init__(self, op: str, operand: ASTNode, line: int, col: int):
        super().__init__(line, col)
        self.op = op
        self.operand = operand

    def to_string(self, indent: int = 0) -> str:
        return " " * (indent * 2) + f"Unary({self.op})\n" + self.operand.to_string(indent + 1)

class BinaryExpr(ASTNode):
    def __init__(self, op: str, left: ASTNode, right: ASTNode, line: int, col: int):
        super().__init__(line, col)
        self.op = op
        self.left = left
        self.right = right

    def to_string(self, indent: int = 0) -> str:
        return " " * (indent * 2) + f"BinaryOp({self.op})\n" + self.left.to_string(indent + 1) + "\n" + self.right.to_string(indent + 1)

class VarDeclStmt(ASTNode):
    def __init__(self, name: str, init: ASTNode, line: int, col: int):
        super().__init__(line, col)
        self.name = name
        self.initializer = init

    def to_string(self, indent: int = 0) -> str:
        return " " * (indent * 2) + f"VarDecl({self.name})\n" + self.initializer.to_string(indent + 1)

class AssignStmt(ASTNode):
    def __init__(self, name: str, val: ASTNode, line: int, col: int):
        super().__init__(line, col)
        self.name = name
        self.value = val

    def to_string(self, indent: int = 0) -> str:
        return " " * (indent * 2) + f"Assign({self.name})\n" + self.value.to_string(indent + 1)

class BlockStmt(ASTNode):
    def __init__(self, stmts: List[ASTNode], line: int, col: int):
        super().__init__(line, col)
        self.statements = stmts

    def to_string(self, indent: int = 0) -> str:
        res = [" " * (indent * 2) + "Block"]
        for s in self.statements:
            res.append(s.to_string(indent + 1))
        return "\n".join(res)

class IfStmt(ASTNode):
    def __init__(self, cond: ASTNode, then_b: ASTNode, else_b: Optional[ASTNode], line: int, col: int):
        super().__init__(line, col)
        self.condition = cond
        self.then_branch = then_b
        self.else_branch = else_b

    def to_string(self, indent: int = 0) -> str:
        res = [" " * (indent * 2) + "If",
               " " * ((indent + 1) * 2) + "Condition:",
               self.condition.to_string(indent + 2),
               " " * ((indent + 1) * 2) + "Then:",
               self.then_branch.to_string(indent + 2)]
        if self.else_branch:
            res.append(" " * ((indent + 1) * 2) + "Else:")
            res.append(self.else_branch.to_string(indent + 2))
        return "\n".join(res)

class WhileStmt(ASTNode):
    def __init__(self, cond: ASTNode, body: ASTNode, line: int, col: int):
        super().__init__(line, col)
        self.condition = cond
        self.body = body

    def to_string(self, indent: int = 0) -> str:
        return "\n".join([
            " " * (indent * 2) + "While",
            " " * ((indent + 1) * 2) + "Condition:",
            self.condition.to_string(indent + 2),
            " " * ((indent + 1) * 2) + "Body:",
            self.body.to_string(indent + 2)
        ])

class PrintStmt(ASTNode):
    def __init__(self, expr: ASTNode, line: int, col: int):
        super().__init__(line, col)
        self.expression = expr

    def to_string(self, indent: int = 0) -> str:
        return " " * (indent * 2) + "Print\n" + self.expression.to_string(indent + 1)

class Program(ASTNode):
    def __init__(self, stmts: List[ASTNode]):
        super().__init__(0, 0)
        self.statements = stmts

    def to_string(self, indent: int = 0) -> str:
        res = [" " * (indent * 2) + "Program"]
        for s in self.statements:
            res.append(s.to_string(indent + 1))
        return "\n".join(res)


class Parser:
    def __init__(self, tokens: List[Token]):
        self.tokens = tokens
        self.pos = 0

    def current(self) -> Token:
        return self.tokens[self.pos]

    def advance(self) -> Token:
        tok = self.tokens[self.pos]
        if self.pos < len(self.tokens) - 1:
            self.pos += 1
        return tok

    def check(self, t_type: TokenType) -> bool:
        return self.current().type == t_type

    def match(self, t_type: TokenType) -> bool:
        if self.check(t_type):
            self.advance()
            return True
        return False

    def expect(self, t_type: TokenType, msg: str) -> Token:
        if self.check(t_type):
            return self.advance()
        raise SyntaxError(f"Parse Error [{self.current().line}:{self.current().col}]: {msg} (got '{self.current().lexeme}')")

    def parse(self) -> Program:
        stmts = []
        while not self.check(TokenType.EOF):
            stmts.append(self.parse_statement())
        return Program(stmts)

    def parse_statement(self) -> ASTNode:
        if self.check(TokenType.KW_INT): return self.parse_var_decl()
        if self.check(TokenType.KW_IF): return self.parse_if()
        if self.check(TokenType.KW_WHILE): return self.parse_while()
        if self.check(TokenType.KW_PRINT): return self.parse_print()
        if self.check(TokenType.LBRACE): return self.parse_block()
        return self.parse_assign()

    def parse_var_decl(self) -> ASTNode:
        kw = self.advance()
        name = self.expect(TokenType.IDENTIFIER, "Expected variable name after 'int'")
        self.expect(TokenType.ASSIGN, "Expected '=' in variable declaration")
        init = self.parse_expr()
        self.expect(TokenType.SEMICOLON, "Expected ';' after variable declaration")
        return VarDeclStmt(name.lexeme, init, kw.line, kw.col)

    def parse_if(self) -> ASTNode:
        kw = self.advance()
        self.expect(TokenType.LPAREN, "Expected '(' after 'if'")
        cond = self.parse_expr()
        self.expect(TokenType.RPAREN, "Expected ')' after condition")
        then_b = self.parse_block()
        else_b = None
        if self.match(TokenType.KW_ELSE):
            else_b = self.parse_block()
        return IfStmt(cond, then_b, else_b, kw.line, kw.col)

    def parse_while(self) -> ASTNode:
        kw = self.advance()
        self.expect(TokenType.LPAREN, "Expected '(' after 'while'")
        cond = self.parse_expr()
        self.expect(TokenType.RPAREN, "Expected ')' after condition")
        body = self.parse_block()
        return WhileStmt(cond, body, kw.line, kw.col)

    def parse_print(self) -> ASTNode:
        kw = self.advance()
        self.expect(TokenType.LPAREN, "Expected '(' after 'print'")
        expr = self.parse_expr()
        self.expect(TokenType.RPAREN, "Expected ')' after expression")
        self.expect(TokenType.SEMICOLON, "Expected ';' after print")
        return PrintStmt(expr, kw.line, kw.col)

    def parse_block(self) -> ASTNode:
        brace = self.expect(TokenType.LBRACE, "Expected '{'")
        stmts = []
        while not self.check(TokenType.RBRACE) and not self.check(TokenType.EOF):
            stmts.append(self.parse_statement())
        self.expect(TokenType.RBRACE, "Expected '}'")
        return BlockStmt(stmts, brace.line, brace.col)

    def parse_assign(self) -> ASTNode:
        if self.check(TokenType.IDENTIFIER) and self.pos + 1 < len(self.tokens) and self.tokens[self.pos + 1].type == TokenType.ASSIGN:
            name = self.advance()
            self.advance()
            val = self.parse_expr()
            self.expect(TokenType.SEMICOLON, "Expected ';' after assignment")
            return AssignStmt(name.lexeme, val, name.line, name.col)
        raise SyntaxError(f"Parse Error [{self.current().line}:{self.current().col}]: Unexpected token '{self.current().lexeme}'")

    # Precedence climbing
    def parse_expr(self) -> ASTNode:
        return self.parse_logical_or()

    def parse_logical_or(self) -> ASTNode:
        left = self.parse_logical_and()
        while self.check(TokenType.OR):
            op = self.advance()
            right = self.parse_logical_and()
            left = BinaryExpr("||", left, right, op.line, op.col)
        return left

    def parse_logical_and(self) -> ASTNode:
        left = self.parse_equality()
        while self.check(TokenType.AND):
            op = self.advance()
            right = self.parse_equality()
            left = BinaryExpr("&&", left, right, op.line, op.col)
        return left

    def parse_equality(self) -> ASTNode:
        left = self.parse_comparison()
        while self.check(TokenType.EQ) or self.check(TokenType.NEQ):
            op = self.advance()
            right = self.parse_comparison()
            left = BinaryExpr(op.lexeme, left, right, op.line, op.col)
        return left

    def parse_comparison(self) -> ASTNode:
        left = self.parse_additive()
        while self.check(TokenType.LT) or self.check(TokenType.LTEQ) or self.check(TokenType.GT) or self.check(TokenType.GTEQ):
            op = self.advance()
            right = self.parse_additive()
            left = BinaryExpr(op.lexeme, left, right, op.line, op.col)
        return left

    def parse_additive(self) -> ASTNode:
        left = self.parse_multiplicative()
        while self.check(TokenType.PLUS) or self.check(TokenType.MINUS):
            op = self.advance()
            right = self.parse_multiplicative()
            left = BinaryExpr(op.lexeme, left, right, op.line, op.col)
        return left

    def parse_multiplicative(self) -> ASTNode:
        left = self.parse_unary()
        while self.check(TokenType.STAR) or self.check(TokenType.SLASH) or self.check(TokenType.PERCENT):
            op = self.advance()
            right = self.parse_unary()
            left = BinaryExpr(op.lexeme, left, right, op.line, op.col)
        return left

    def parse_unary(self) -> ASTNode:
        if self.check(TokenType.MINUS):
            op = self.advance()
            operand = self.parse_unary()
            return UnaryExpr("-", operand, op.line, op.col)
        if self.check(TokenType.BANG):
            op = self.advance()
            operand = self.parse_unary()
            return UnaryExpr("!", operand, op.line, op.col)
        return self.parse_primary()

    def parse_primary(self) -> ASTNode:
        if self.check(TokenType.INT_LITERAL):
            tok = self.advance()
            return IntLiteralExpr(tok.int_val, tok.line, tok.col)
        if self.check(TokenType.IDENTIFIER):
            tok = self.advance()
            return VarRefExpr(tok.lexeme, tok.line, tok.col)
        if self.match(TokenType.LPAREN):
            expr = self.parse_expr()
            self.expect(TokenType.RPAREN, "Expected ')'")
            return expr
        raise SyntaxError(f"Parse Error [{self.current().line}:{self.current().col}]: Expected expression, got '{self.current().lexeme}'")


# =============================================================================
# Stage 3: Virtual ISA & Code Generation
# =============================================================================

class Opcode(enum.Enum):
    OP_PUSH_CONST = "OP_PUSH_CONST"
    OP_LOAD_VAR = "OP_LOAD_VAR"
    OP_STORE_VAR = "OP_STORE_VAR"
    OP_ADD = "OP_ADD"
    OP_SUB = "OP_SUB"
    OP_MUL = "OP_MUL"
    OP_DIV = "OP_DIV"
    OP_MOD = "OP_MOD"
    OP_NEG = "OP_NEG"
    OP_NOT = "OP_NOT"
    OP_CMP_EQ = "OP_CMP_EQ"
    OP_CMP_NEQ = "OP_CMP_NEQ"
    OP_CMP_LT = "OP_CMP_LT"
    OP_CMP_LTEQ = "OP_CMP_LTEQ"
    OP_CMP_GT = "OP_CMP_GT"
    OP_CMP_GTEQ = "OP_CMP_GTEQ"
    OP_AND = "OP_AND"
    OP_OR = "OP_OR"
    OP_JUMP = "OP_JUMP"
    OP_JUMP_IF_FALSE = "OP_JUMP_IF_FALSE"
    OP_PRINT = "OP_PRINT"
    OP_HALT = "OP_HALT"

class Instruction:
    def __init__(self, op: Opcode, operand: int = 0, var_name: str = ""):
        self.op = op
        self.operand = operand
        self.var_name = var_name

    def __repr__(self):
        s = f"{self.op.value:<18}"
        if self.op == Opcode.OP_PUSH_CONST: s += f" {self.operand}"
        elif self.op in (Opcode.OP_LOAD_VAR, Opcode.OP_STORE_VAR): s += f" '{self.var_name}'"
        elif self.op in (Opcode.OP_JUMP, Opcode.OP_JUMP_IF_FALSE): s += f" -> {self.operand:04d}"
        return s

class CodeGenerator:
    def __init__(self):
        self.instructions: List[Instruction] = []

    def emit(self, op: Opcode, operand: int = 0, var_name: str = ""):
        self.instructions.append(Instruction(op, operand, var_name))

    def emit_jump(self, op: Opcode) -> int:
        self.instructions.append(Instruction(op, -1))
        return len(self.instructions) - 1

    def patch_jump(self, idx: int, target: int):
        self.instructions[idx].operand = target

    def generate(self, program: Program) -> List[Instruction]:
        self.instructions = []
        for stmt in program.statements:
            self.generate_stmt(stmt)
        self.emit(Opcode.OP_HALT)
        return self.instructions

    def generate_expr(self, node: ASTNode):
        if isinstance(node, IntLiteralExpr):
            self.emit(Opcode.OP_PUSH_CONST, node.value)
        elif isinstance(node, VarRefExpr):
            self.emit(Opcode.OP_LOAD_VAR, 0, node.name)
        elif isinstance(node, UnaryExpr):
            self.generate_expr(node.operand)
            if node.op == "-": self.emit(Opcode.OP_NEG)
            elif node.op == "!": self.emit(Opcode.OP_NOT)
        elif isinstance(node, BinaryExpr):
            self.generate_expr(node.left)
            self.generate_expr(node.right)
            op_map = {
                "+": Opcode.OP_ADD, "-": Opcode.OP_SUB, "*": Opcode.OP_MUL,
                "/": Opcode.OP_DIV, "%": Opcode.OP_MOD, "==": Opcode.OP_CMP_EQ,
                "!=": Opcode.OP_CMP_NEQ, "<": Opcode.OP_CMP_LT, "<=": Opcode.OP_CMP_LTEQ,
                ">": Opcode.OP_CMP_GT, ">=": Opcode.OP_CMP_GTEQ, "&&": Opcode.OP_AND,
                "||": Opcode.OP_OR
            }
            self.emit(op_map[node.op])

    def generate_stmt(self, node: ASTNode):
        if isinstance(node, VarDeclStmt):
            self.generate_expr(node.initializer)
            self.emit(Opcode.OP_STORE_VAR, 0, node.name)
        elif isinstance(node, AssignStmt):
            self.generate_expr(node.value)
            self.emit(Opcode.OP_STORE_VAR, 0, node.name)
        elif isinstance(node, PrintStmt):
            self.generate_expr(node.expression)
            self.emit(Opcode.OP_PRINT)
        elif isinstance(node, BlockStmt):
            for s in node.statements:
                self.generate_stmt(s)
        elif isinstance(node, IfStmt):
            self.generate_expr(node.condition)
            jump_to_else = self.emit_jump(Opcode.OP_JUMP_IF_FALSE)
            self.generate_stmt(node.then_branch)
            if node.else_branch:
                jump_over_else = self.emit_jump(Opcode.OP_JUMP)
                self.patch_jump(jump_to_else, len(self.instructions))
                self.generate_stmt(node.else_branch)
                self.patch_jump(jump_over_else, len(self.instructions))
            else:
                self.patch_jump(jump_to_else, len(self.instructions))
        elif isinstance(node, WhileStmt):
            loop_start = len(self.instructions)
            self.generate_expr(node.condition)
            jump_to_end = self.emit_jump(Opcode.OP_JUMP_IF_FALSE)
            self.generate_stmt(node.body)
            self.emit(Opcode.OP_JUMP, loop_start)
            self.patch_jump(jump_to_end, len(self.instructions))


# =============================================================================
# Stage 4: Virtual Machine Execution Engine
# =============================================================================

class VirtualMachine:
    MAX_STACK = 1024

    def __init__(self, program: List[Instruction], out_stream=sys.stdout):
        self.program = program
        self.stack: List[int] = []
        self.vars: Dict[str, int] = {}
        self.pc = 0
        self.out = out_stream

    def push(self, val: int):
        if len(self.stack) >= self.MAX_STACK:
            raise RuntimeError(f"VM Error: Stack overflow (limit {self.MAX_STACK})")
        self.stack.append(val)

    def pop(self) -> int:
        if not self.stack:
            raise RuntimeError(f"VM Error: Stack underflow at PC={self.pc}")
        return self.stack.pop()

    def execute(self):
        self.pc = 0
        while self.pc < len(self.program):
            instr = self.program[self.pc]
            op = instr.op

            if op == Opcode.OP_PUSH_CONST:
                self.push(instr.operand)
                self.pc += 1
            elif op == Opcode.OP_LOAD_VAR:
                if instr.var_name not in self.vars:
                    raise RuntimeError(f"Runtime Error: Access to undeclared variable '{instr.var_name}' at PC={self.pc}")
                self.push(self.vars[instr.var_name])
                self.pc += 1
            elif op == Opcode.OP_STORE_VAR:
                self.vars[instr.var_name] = self.pop()
                self.pc += 1
            elif op == Opcode.OP_ADD:
                b, a = self.pop(), self.pop()
                self.push(a + b); self.pc += 1
            elif op == Opcode.OP_SUB:
                b, a = self.pop(), self.pop()
                self.push(a - b); self.pc += 1
            elif op == Opcode.OP_MUL:
                b, a = self.pop(), self.pop()
                self.push(a * b); self.pc += 1
            elif op == Opcode.OP_DIV:
                b, a = self.pop(), self.pop()
                if b == 0: raise RuntimeError(f"Runtime Error: Division by zero at PC={self.pc}")
                self.push(int(a / b)); self.pc += 1
            elif op == Opcode.OP_MOD:
                b, a = self.pop(), self.pop()
                if b == 0: raise RuntimeError(f"Runtime Error: Modulo by zero at PC={self.pc}")
                self.push(a % b); self.pc += 1
            elif op == Opcode.OP_NEG:
                self.push(-self.pop()); self.pc += 1
            elif op == Opcode.OP_NOT:
                self.push(1 if self.pop() == 0 else 0); self.pc += 1
            elif op == Opcode.OP_CMP_EQ:
                b, a = self.pop(), self.pop()
                self.push(1 if a == b else 0); self.pc += 1
            elif op == Opcode.OP_CMP_NEQ:
                b, a = self.pop(), self.pop()
                self.push(1 if a != b else 0); self.pc += 1
            elif op == Opcode.OP_CMP_LT:
                b, a = self.pop(), self.pop()
                self.push(1 if a < b else 0); self.pc += 1
            elif op == Opcode.OP_CMP_LTEQ:
                b, a = self.pop(), self.pop()
                self.push(1 if a <= b else 0); self.pc += 1
            elif op == Opcode.OP_CMP_GT:
                b, a = self.pop(), self.pop()
                self.push(1 if a > b else 0); self.pc += 1
            elif op == Opcode.OP_CMP_GTEQ:
                b, a = self.pop(), self.pop()
                self.push(1 if a >= b else 0); self.pc += 1
            elif op == Opcode.OP_AND:
                b, a = self.pop(), self.pop()
                self.push(1 if (a != 0 and b != 0) else 0); self.pc += 1
            elif op == Opcode.OP_OR:
                b, a = self.pop(), self.pop()
                self.push(1 if (a != 0 or b != 0) else 0); self.pc += 1
            elif op == Opcode.OP_JUMP:
                self.pc = instr.operand
            elif op == Opcode.OP_JUMP_IF_FALSE:
                c = self.pop()
                if c == 0:
                    self.pc = instr.operand
                else:
                    self.pc += 1
            elif op == Opcode.OP_PRINT:
                val = self.pop()
                self.out.write(f"{val}\n")
                self.pc += 1
            elif op == Opcode.OP_HALT:
                return


def run_pipeline(source_code: str, flags: dict = None) -> int:
    if flags is None:
        flags = {"run": True}

    tokens = Lexer(source_code).tokenize()
    if flags.get("lex"):
        print("=== [Stage 1: Lexer Tokens] ===")
        for t in tokens: print(t)
        if not (flags.get("ast") or flags.get("bytecode") or flags.get("run")):
            return 0

    ast = Parser(tokens).parse()
    if flags.get("ast"):
        print("=== [Stage 2: Abstract Syntax Tree] ===")
        print(ast.to_string(0))
        if not (flags.get("bytecode") or flags.get("run")):
            return 0

    bytecode = CodeGenerator().generate(ast)
    if flags.get("bytecode"):
        print("=== [Stage 3: Virtual ISA Bytecode] ===")
        for i, instr in enumerate(bytecode):
            print(f"{i:04d}  {repr(instr)}")
        if not flags.get("run"):
            return 0

    if flags.get("run", True):
        vm = VirtualMachine(bytecode)
        vm.execute()
    return 0


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: reference_compiler.py [--lex] [--ast] [--dump-bytecode] [--run] <file.cm>")
        sys.exit(1)

    stage_flags = {}
    filepath = None
    for a in sys.argv[1:]:
        if a == "--lex": stage_flags["lex"] = True
        elif a == "--ast": stage_flags["ast"] = True
        elif a == "--dump-bytecode": stage_flags["bytecode"] = True
        elif a == "--run": stage_flags["run"] = True
        elif not a.startswith("-"): filepath = a

    if not filepath:
        print("Error: No input file.")
        sys.exit(1)

    with open(filepath, "r", encoding="utf-8") as f:
        src = f.read()

    try:
        run_pipeline(src, stage_flags)
    except Exception as e:
        sys.stderr.write(f"{e}\n")
        sys.exit(1)
