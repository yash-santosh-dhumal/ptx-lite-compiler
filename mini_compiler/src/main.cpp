// =============================================================================
// main.cpp — CLI Driver for C-Subset Mini-Compiler
// =============================================================================
// Command-line interface supporting compiler pipeline inspection flags:
//   --lex            : Dump token stream and exit
//   --ast            : Dump pretty-printed Abstract Syntax Tree and exit
//   --dump-bytecode  : Disassemble and dump generated virtual ISA bytecode
//   --run            : Execute bytecode on the Virtual Machine (default)
//   --help, -h       : Display usage information
//
// Returns 0 on success, non-zero exit code on syntax/type/runtime errors.
//
// Part of the C-Subset Mini-Compiler project.
// =============================================================================

#include "lexer.hpp"
#include "parser.hpp"
#include "codegen.hpp"
#include "vm.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

void printUsage(const char* progName) {
    std::cout << "===============================================================\n"
              << " C-Subset Mini-Compiler & Virtual Machine Runtime Engine\n"
              << " Designed for Compiler & Systems Verification\n"
              << "===============================================================\n"
              << "Usage: " << progName << " [options] <source_file.cm>\n\n"
              << "Pipeline Inspection Options:\n"
              << "  --lex            Tokenize input and dump lexical tokens to stdout\n"
              << "  --ast            Parse tokens and dump AST tree hierarchy\n"
              << "  --dump-bytecode  Compile AST and disassemble virtual ISA bytecode\n"
              << "  --run            Execute bytecode on virtual machine (default)\n"
              << "  --all            Run all pipeline stages sequentially\n"
              << "  -h, --help       Display this help message\n\n"
              << "Examples:\n"
              << "  " << progName << " tests/test_programs/01_arithmetic.cm\n"
              << "  " << progName << " --ast tests/test_programs/03_control_flow.cm\n"
              << "  " << progName << " --dump-bytecode tests/test_programs/04_while_loop.cm\n";
}

std::string readFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open source file: " + filepath);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string sourcePath;
    bool flagLex = false;
    bool flagAst = false;
    bool flagBytecode = false;
    bool flagRun = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--lex") {
            flagLex = true;
        } else if (arg == "--ast") {
            flagAst = true;
        } else if (arg == "--dump-bytecode") {
            flagBytecode = true;
        } else if (arg == "--run") {
            flagRun = true;
        } else if (arg == "--all") {
            flagLex = true;
            flagAst = true;
            flagBytecode = true;
            flagRun = true;
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "Error: Unrecognized option '" << arg << "'\n";
            printUsage(argv[0]);
            return 1;
        } else {
            sourcePath = arg;
        }
    }

    if (sourcePath.empty()) {
        std::cerr << "Error: No source file specified.\n";
        printUsage(argv[0]);
        return 1;
    }

    // Default action if no specific stage flag is set
    if (!flagLex && !flagAst && !flagBytecode && !flagRun) {
        flagRun = true;
    }

    try {
        std::string source = readFile(sourcePath);

        // -------------------------------------------------------------
        // Stage 1: Lexical Analysis
        // -------------------------------------------------------------
        mc::Lexer lexer(source);
        std::vector<mc::Token> tokens = lexer.tokenize();

        if (flagLex) {
            std::cout << "=== [Stage 1: Lexer Tokens] ===\n";
            for (const auto& tok : tokens) {
                std::cout << tok << "\n";
            }
            if (!flagAst && !flagBytecode && !flagRun) {
                return 0;
            }
        }

        // -------------------------------------------------------------
        // Stage 2: Recursive Descent Parsing (AST Construction)
        // -------------------------------------------------------------
        mc::Parser parser(tokens);
        std::unique_ptr<mc::Program> ast = parser.parse();

        if (flagAst) {
            std::cout << "=== [Stage 2: Abstract Syntax Tree] ===\n";
            std::cout << ast->toString(0) << "\n";
            if (!flagBytecode && !flagRun) {
                return 0;
            }
        }

        // -------------------------------------------------------------
        // Stage 3: Code Generation (Virtual ISA Bytecode Emission)
        // -------------------------------------------------------------
        mc::CodeGenerator codegen;
        std::vector<mc::Instruction> bytecode = codegen.generate(*ast);

        if (flagBytecode) {
            std::cout << "=== [Stage 3: Virtual ISA Bytecode] ===\n";
            mc::disassemble(bytecode, std::cout);
            if (!flagRun) {
                return 0;
            }
        }

        // -------------------------------------------------------------
        // Stage 4: Virtual Machine Execution
        // -------------------------------------------------------------
        if (flagRun) {
            if (flagLex || flagAst || flagBytecode) {
                std::cout << "=== [Stage 4: VM Execution Output] ===\n";
            }
            mc::VM vm(bytecode, std::cout);
            vm.execute();
        }

        return 0;

    } catch (const mc::LexerError& e) {
        std::cerr << "Lexer Error [" << e.line << ":" << e.column << "]: " << e.what() << "\n";
        return 2;
    } catch (const mc::ParseError& e) {
        std::cerr << "Parse Error [" << e.line << ":" << e.column << "]: " << e.what() << "\n";
        return 3;
    } catch (const mc::CodeGenError& e) {
        std::cerr << "CodeGen Error [" << e.line << ":" << e.column << "]: " << e.what() << "\n";
        return 4;
    } catch (const mc::RuntimeError& e) {
        std::cerr << "Runtime Error: " << e.what() << "\n";
        return 5;
    } catch (const std::exception& e) {
        std::cerr << "General Fatal Error: " << e.what() << "\n";
        return 1;
    }
}
