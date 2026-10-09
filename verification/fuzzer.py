#!/usr/bin/env python3
"""
=============================================================================
fuzzer.py — Synthetic Test Generator & Differential Fuzzing Testbed
=============================================================================
Differential testing harness for stress and boundary testing:
  1. Synthesizes randomized, valid arithmetic programs with nested
     expressions, multiple variables, and control flow.
  2. Evaluates the ground truth in Python.
  3. Compiles and executes the generated test on mini_compiler VM.
  4. Cross-checks compiler execution against Python ground truth.

Inspired by compiler stress-testing and fuzzing techniques used at NVIDIA.
=============================================================================
"""

import os
import sys
import random
import subprocess
import tempfile
from pathlib import Path
from typing import Tuple, Optional

from verify_runner import find_or_build_binary, Colors


class SyntheticProgramGenerator:
    """Generates randomized valid programs with known ground-truth output."""

    def __init__(self, seed: int = None):
        if seed is not None:
            random.seed(seed)

    def random_arithmetic_expr(self, depth: int = 0, max_depth: int = 3) -> Tuple[str, int]:
        """Generates a random arithmetic expression and evaluates its integer value."""
        if depth >= max_depth or random.random() < 0.35:
            val = random.randint(1, 20)
            return str(val), val

        op = random.choice(["+", "-", "*"])
        left_str, _ = self.random_arithmetic_expr(depth + 1, max_depth)
        right_str, _ = self.random_arithmetic_expr(depth + 1, max_depth)

        # Wrap subexpressions randomly
        if random.random() < 0.5:
            left_str = f"({left_str})"
        if random.random() < 0.5:
            right_str = f"({right_str})"

        expr_str = f"{left_str} {op} {right_str}"
        # Evaluate ground-truth strictly using Python's precedence parser
        val = int(eval(expr_str))
        return expr_str, val

    def generate_program(self, num_vars: int = 4) -> Tuple[str, list]:
        """Generates a multi-step program with variables, expressions, and print statements."""
        lines = []
        expected_outputs = []
        var_env = {}

        # 1. Initialize variables with arithmetic
        for i in range(num_vars):
            vname = f"var_{i}"
            expr_str, val = self.random_arithmetic_expr(depth=0, max_depth=2)
            lines.append(f"int {vname} = {expr_str};")
            var_env[vname] = val

        # 2. Re-assignments mixing variables and constants
        var_names = list(var_env.keys())
        for _ in range(num_vars):
            target = random.choice(var_names)
            source = random.choice(var_names)
            op = random.choice(["+", "-", "*"])
            const_val = random.randint(1, 10)

            lines.append(f"{target} = {source} {op} {const_val};")
            if op == "+":
                var_env[target] = var_env[source] + const_val
            elif op == "-":
                var_env[target] = var_env[source] - const_val
            else:
                var_env[target] = var_env[source] * const_val

        # 3. Conditional block
        cond_var = random.choice(var_names)
        pivot = var_env[cond_var]
        lines.append(f"if ({cond_var} >= {pivot}) {{")
        bonus = random.randint(100, 200)
        lines.append(f"    print({bonus});")
        lines.append("} else {")
        lines.append("    print(0);")
        lines.append("}")
        expected_outputs.append(bonus)

        # 4. Print all final variables
        for v in var_names:
            lines.append(f"print({v});")
            expected_outputs.append(var_env[v])

        return "\n".join(lines), expected_outputs


def run_fuzz_iterations(binary: Optional[Path], iterations: int = 25):
    script_dir = Path(__file__).resolve().parent
    project_root = script_dir.parent

    print(f"\n{Colors.BOLD}{Colors.CYAN}{'='*70}{Colors.RESET}")
    print(f"{Colors.BOLD}{Colors.CYAN} Differential Fuzzing Testbed: Stressing Compiler & VM{Colors.RESET}")
    print(f"{Colors.BOLD}{Colors.CYAN}{'='*70}{Colors.RESET}\n")
    if binary:
        print(f"Target Runtime Engine: {Colors.BOLD}Native C++17 Binary ({binary.name}){Colors.RESET}")
    else:
        print(f"Target Runtime Engine: {Colors.BOLD}{Colors.YELLOW}Python Golden Reference Pipeline{Colors.RESET}")
    print(f"Executing {iterations} randomized synthetic program stress cycles...\n")

    gen = SyntheticProgramGenerator()
    passed_fuzz = 0

    with tempfile.TemporaryDirectory() as tmpdir:
        for i in range(1, iterations + 1):
            src_code, expected_vals = gen.generate_program(num_vars=random.randint(3, 6))
            test_file = Path(tmpdir) / f"fuzz_case_{i}.cm"
            test_file.write_text(src_code)

            if binary is not None:
                cmd = [str(binary), str(test_file)]
            else:
                ref_script = project_root / "verification" / "reference_compiler.py"
                cmd = [sys.executable, str(ref_script), str(test_file)]

            proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

            if proc.returncode != 0:
                print(f"[{Colors.RED}FUZZ CRASH{Colors.RESET}] Cycle #{i} returned non-zero code {proc.returncode}")
                print(f"Error output:\n{proc.stderr}")
                print(f"Source Program:\n{src_code}")
                sys.exit(1)

            actual_vals = [int(x) for x in proc.stdout.strip().split() if x.strip()]
            if actual_vals != expected_vals:
                print(f"[{Colors.RED}FUZZ MISMATCH{Colors.RESET}] Cycle #{i} Differential Discrepancy!")
                print(f"  Expected: {expected_vals}")
                print(f"  Actual:   {actual_vals}")
                print(f"Source Program:\n{src_code}")
                sys.exit(1)

            passed_fuzz += 1
            if i % 5 == 0 or i == iterations:
                print(f"  Cycle {i:>3}/{iterations}: [{Colors.GREEN}VERIFIED OK{Colors.RESET}] Differential match across {len(expected_vals)} outputs")

    print(f"\n{Colors.GREEN}{Colors.BOLD}>>> FUZZING COMPLETE: All {passed_fuzz}/{iterations} cycles verified successfully! <<<{Colors.RESET}\n")


if __name__ == "__main__":
    script_dir = Path(__file__).resolve().parent
    project_root = script_dir.parent
    binary_path = find_or_build_binary(project_root)
    run_fuzz_iterations(binary_path, iterations=25)
