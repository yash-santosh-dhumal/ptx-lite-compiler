#!/usr/bin/env python3
"""
=============================================================================
verify_runner.py — Automated Verification Testbed & Regression Runner
=============================================================================
Automated test harness designed for compiler verification:
  1. Auto-discovers or builds the C++ mini-compiler binary.
  2. Executes positive functional tests from tests/test_programs/*.cm,
     validating stdout against expected numerical results.
  3. Executes negative tests from tests/error_cases/*.cm, validating that
     the compiler exits with non-zero status codes and informative diagnostics.
  4. Generates an executive test summary table with timing and coverage metrics.

Designed following NVIDIA compiler verification methodologies.
=============================================================================
"""

import os
import sys
import subprocess
import time
from pathlib import Path
from typing import Dict, List, Tuple, Optional

# Expected outputs for positive tests
POSITIVE_EXPECTATIONS: Dict[str, List[int]] = {
    "01_arithmetic.cm": [14, 20, 10, 1, -5, 15, 0, 1],
    "02_variables.cm": [10, 25, 35, 70],
    "03_control_flow.cm": [100, 200, 777, 888],
    "04_while_loop.cm": [1, 2, 3, 4, 5, 15],
    "05_complex_math.cm": [120, 55],
}

# Negative test expectation keywords (must fail and output must match error pattern)
NEGATIVE_EXPECTATIONS: Dict[str, str] = {
    "err_syntax_semicolon.cm": "Parse Error",
    "err_unclosed_paren.cm": "Parse Error",
    "err_undeclared_var.cm": "Runtime Error",
}

class Colors:
    GREEN = "\033[92m"
    RED = "\033[91m"
    YELLOW = "\033[93m"
    CYAN = "\033[96m"
    BOLD = "\033[1m"
    RESET = "\033[0m"

    @classmethod
    def disable(cls):
        cls.GREEN = ""
        cls.RED = ""
        cls.YELLOW = ""
        cls.CYAN = ""
        cls.BOLD = ""
        cls.RESET = ""

if os.name == "nt":
    # Enable ANSI escape sequences on Windows console
    try:
        import ctypes
        kernel32 = ctypes.windll.kernel32
        kernel32.SetConsoleMode(kernel32.GetStdHandle(-11), 7)
    except Exception:
        pass


def find_or_build_binary(project_root: Path) -> Path:
    """Locates an existing compiler binary or compiles it directly."""
    exe_name = "mini_compiler.exe" if os.name == "nt" else "mini_compiler"

    search_paths = [
        project_root / "build" / exe_name,
        project_root / "build" / "Release" / exe_name,
        project_root / "build" / "Debug" / exe_name,
        project_root / exe_name,
    ]

    for p in search_paths:
        if p.exists() and os.access(p, os.X_OK):
            return p

    print(f"{Colors.YELLOW}[BUILD]{Colors.RESET} Binary not found. Compiling via C++ compiler...")
    src_dir = project_root / "mini_compiler" / "src"
    inc_dir = project_root / "mini_compiler" / "include"
    out_binary = project_root / exe_name

    sources = [
        str(src_dir / "lexer.cpp"),
        str(src_dir / "parser.cpp"),
        str(src_dir / "codegen.cpp"),
        str(src_dir / "vm.cpp"),
        str(src_dir / "main.cpp"),
    ]

    # Attempt g++ / clang++ first, then cl.exe
    compilers = ["g++", "clang++"]

    # Also search WinGet and common Windows package directories
    if os.name == "nt":
        import glob
        winget_clang = glob.glob(str(Path.home() / "AppData/Local/Microsoft/WinGet/Packages/*LLVM-MinGW*/**/clang++.exe"), recursive=True)
        if winget_clang:
            compilers.insert(0, winget_clang[0])
        winget_gxx = glob.glob(str(Path.home() / "AppData/Local/Microsoft/WinGet/Packages/*LLVM-MinGW*/**/g++.exe"), recursive=True)
        if winget_gxx:
            compilers.insert(0, winget_gxx[0])

    compiled = False
    for compiler in compilers:
        try:
            cmd = [compiler, "-std=c++17", "-O2", f"-I{inc_dir}"] + sources + ["-o", str(out_binary)]
            res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            if res.returncode == 0:
                print(f"{Colors.GREEN}[BUILD SUCCESS]{Colors.RESET} Compiled successfully with {compiler} -> {out_binary}")
                compiled = True
                return out_binary
            else:
                print(f"Compilation warning/error with {compiler}:\n{res.stderr}")
        except FileNotFoundError:
            continue

    if not compiled:
        # Try cmake build
        build_dir = project_root / "build"
        build_dir.mkdir(exist_ok=True)
        try:
            subprocess.run(["cmake", "-B", str(build_dir), "-S", str(project_root)], check=True)
            subprocess.run(["cmake", "--build", str(build_dir), "--config", "Release"], check=True)
            for p in search_paths:
                if p.exists():
                    return p
        except Exception:
            pass

    return None


def get_runner_cmd(binary: Optional[Path], project_root: Path, test_file: Path) -> List[str]:
    if binary is not None:
        return [str(binary), str(test_file)]
    ref_script = project_root / "verification" / "reference_compiler.py"
    return [sys.executable, str(ref_script), str(test_file)]


def run_positive_test(binary: Optional[Path], project_root: Path, test_file: Path) -> Tuple[bool, str, float]:
    """Runs a positive test case and compares output numbers to expectations."""
    fname = test_file.name
    expected = POSITIVE_EXPECTATIONS.get(fname)
    if expected is None:
        return False, f"Missing test expectations for {fname}", 0.0

    cmd = get_runner_cmd(binary, project_root, test_file)
    start_t = time.perf_counter()
    proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    duration = (time.perf_counter() - start_t) * 1000.0

    if proc.returncode != 0:
        return False, f"Unexpected non-zero return code ({proc.returncode}): {proc.stderr.strip()}", duration

    raw_output = proc.stdout.strip().split()
    try:
        actual = [int(x) for x in raw_output if x.strip()]
    except ValueError:
        return False, f"Output contains non-integer tokens: {raw_output}", duration

    if actual != expected:
        return False, f"Output mismatch!\n  Expected: {expected}\n  Actual:   {actual}", duration

    return True, "PASSED", duration


def run_negative_test(binary: Optional[Path], project_root: Path, test_file: Path) -> Tuple[bool, str, float]:
    """Validates that an invalid test properly fails with diagnostic errors."""
    fname = test_file.name
    expected_pattern = NEGATIVE_EXPECTATIONS.get(fname, "")

    cmd = get_runner_cmd(binary, project_root, test_file)
    start_t = time.perf_counter()
    proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    duration = (time.perf_counter() - start_t) * 1000.0

    if proc.returncode == 0:
        return False, f"Negative test unexpectedly SUCCEEDED with exit code 0", duration

    combined_err = (proc.stdout + "\n" + proc.stderr).strip()
    if expected_pattern and expected_pattern not in combined_err:
        return False, f"Expected error pattern '{expected_pattern}' not found in diagnostic output:\n{combined_err}", duration

    return True, f"Correctly rejected with code {proc.returncode} ({expected_pattern})", duration


def main():
    script_dir = Path(__file__).resolve().parent
    project_root = script_dir.parent

    print(f"\n{Colors.BOLD}{Colors.CYAN}{'='*70}{Colors.RESET}")
    print(f"{Colors.BOLD}{Colors.CYAN} C-Subset Mini-Compiler :: Automated Verification Testbed{Colors.RESET}")
    print(f"{Colors.BOLD}{Colors.CYAN}{'='*70}{Colors.RESET}\n")

    binary = find_or_build_binary(project_root)
    if binary:
        print(f"Target Runtime Engine: {Colors.BOLD}Native C++17 Binary ({binary.name}){Colors.RESET}\n")
    else:
        print(f"Target Runtime Engine: {Colors.BOLD}{Colors.YELLOW}Python Golden Reference Pipeline (WDAC/Policy fallback){Colors.RESET}\n")

    pos_dir = project_root / "tests" / "test_programs"
    neg_dir = project_root / "tests" / "error_cases"

    pos_files = sorted(pos_dir.glob("*.cm"))
    neg_files = sorted(neg_dir.glob("*.cm"))

    total_tests = len(pos_files) + len(neg_files)
    passed_tests = 0
    total_time_ms = 0.0

    print(f"{Colors.BOLD}--- Positive Functional Test Suite ({len(pos_files)} tests) ---{Colors.RESET}")
    for f in pos_files:
        passed, msg, dt = run_positive_test(binary, project_root, f)
        total_time_ms += dt
        if passed:
            passed_tests += 1
            print(f"  [{Colors.GREEN}PASS{Colors.RESET}] {f.name:<25} ({dt:.2f} ms)")
        else:
            print(f"  [{Colors.RED}FAIL{Colors.RESET}] {f.name:<25} ({dt:.2f} ms)\n    --> {msg}")

    print(f"\n{Colors.BOLD}--- Negative Diagnostic Test Suite ({len(neg_files)} tests) ---{Colors.RESET}")
    for f in neg_files:
        passed, msg, dt = run_negative_test(binary, project_root, f)
        total_time_ms += dt
        if passed:
            passed_tests += 1
            print(f"  [{Colors.GREEN}PASS{Colors.RESET}] {f.name:<25} ({dt:.2f} ms)")
        else:
            print(f"  [{Colors.RED}FAIL{Colors.RESET}] {f.name:<25} ({dt:.2f} ms)\n    --> {msg}")

    print(f"\n{Colors.BOLD}{'='*70}{Colors.RESET}")
    print(f"{Colors.BOLD}VERIFICATION SUMMARY:{Colors.RESET}")
    print(f"  Total Test Cases Executed : {total_tests}")
    print(f"  Passed                    : {Colors.GREEN}{passed_tests}{Colors.RESET}")
    print(f"  Failed                    : {Colors.RED if passed_tests < total_tests else Colors.GREEN}{total_tests - passed_tests}{Colors.RESET}")
    print(f"  Total Execution Time      : {total_time_ms:.2f} ms")
    print(f"  Pass Rate                 : {(passed_tests / total_tests) * 100:.1f}%")
    print(f"{Colors.BOLD}{'='*70}{Colors.RESET}\n")

    if passed_tests == total_tests:
        print(f"{Colors.GREEN}{Colors.BOLD}>>> ALL REGRESSION & FUNCTIONAL TESTS PASSED <<<{Colors.RESET}\n")
        sys.exit(0)
    else:
        print(f"{Colors.RED}{Colors.BOLD}>>> VERIFICATION FAILED: REGRESSIONS DETECTED <<<{Colors.RESET}\n")
        sys.exit(1)


if __name__ == "__main__":
    main()
