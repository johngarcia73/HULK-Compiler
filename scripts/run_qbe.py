#!/usr/bin/env python3
import argparse
import os
import sys
import tempfile
from shell import Shell
from typing import Tuple, List


def get_usage():
    return """USAGE:
  run-ssa [OPTIONS] <INPUT_FILE> [-- <PROGRAM_ARGS>...]

  (expert alias: qir works exactly the same way)

ARGUMENTS:
  <INPUT_FILE>       Input .ssa file, or - to read from stdin
  <PROGRAM_ARGS>     Arguments passed to the running program
                     (only meaningful together with --run)

OPTIONS:
  -o, --output <FILE>   Write the result to <FILE>
                        (default: same as input name with the proper extension)
  -r, --run             Run the program in place right after building
  --link-c-file <FILE>  Compile and link the specified C file alongside QBE assembly (can be used multiple times)
  --link-object <FILE>  Link the specified .o object file (can be used multiple times)
  --use-runtime         Automatically link runtime/runtime.c

EXAMPLES:
  run-ssa program.ssa
  run-ssa --output mybinary program.ssa
  run-ssa -r program.ssa -- --verbose --help"""


def parse_cli(args_list: List[str]) -> Tuple[argparse.Namespace, List[str]]:
    program_args = []
    if "--" in args_list:
        split_idx = args_list.index("--")
        program_args = args_list[split_idx + 1:]
        args_list = args_list[:split_idx]

    parser = argparse.ArgumentParser(add_help=False, usage=argparse.SUPPRESS)
    parser.add_argument("input_file")
    parser.add_argument("-o", "--output")
    parser.add_argument("-r", "--run", action="store_true")
    parser.add_argument("--link-c-file", action="append", default=[])
    parser.add_argument("--link-object", action="append", default=[])
    parser.add_argument("--use-runtime", action="store_true")

    try:
        return parser.parse_args(args_list), program_args
    except SystemExit:
        print(get_usage())
        sys.exit(1)

def get_out_bin(input_file: str, out_arg: str) -> str:
    if out_arg:
        return out_arg
    if input_file == "-":
        return "a.out"
    base, _ = os.path.splitext(input_file)
    return base if base != input_file else f"{input_file}.out"

def compile_qbe(sh: Shell, ssa_path: str, asm_path: str) -> None:
    cmd_qbe = f"qbe -o {asm_path} {ssa_path}"
    res = sh >> cmd_qbe
    if res and res.startswith("Error:"):
        print("QBE compilation failed.")
        sys.exit(1)

def compile_cc(sh: Shell, asm_path: str, out_bin: str, args: argparse.Namespace) -> None:
    cc_args = ["cc", asm_path, "-o", out_bin]
    cc_args.extend(args.link_c_file)
    cc_args.extend(args.link_object)
    
    if args.use_runtime:
        cc_args.append(os.path.join("runtime", "runtime.c"))

    cmd_cc = " ".join(cc_args)
    res = sh >> cmd_cc
    if res and res.startswith("Error:"):
        print("C compilation/linking failed.")
        sys.exit(1)

def main() -> None:
    sh = Shell()

    if "-h" in sys.argv or "--help" in sys.argv:
        print(get_usage())
        sys.exit(0)

    args, program_args = parse_cli(sys.argv[1:])
    out_bin = get_out_bin(args.input_file, args.output)

    with tempfile.TemporaryDirectory() as tmpdir:
        ssa_path = args.input_file
        if args.input_file == "-":
            ssa_path = os.path.join(tmpdir, "input.ssa")
            with open(ssa_path, "w") as f:
                f.write(sys.stdin.read())

        asm_path = os.path.join(tmpdir, "output.s")
        compile_qbe(sh, ssa_path, asm_path)
        compile_cc(sh, asm_path, out_bin, args)

    if args.run:
        run_file = out_bin if os.path.isabs(out_bin) or \
                   out_bin.startswith(".") else f"./{out_bin}"
        
        cmd_run = run_file
        if program_args:
            cmd_run += " " + " ".join(program_args)

        _ = sh >> cmd_run

if __name__ == "__main__":
    main()
