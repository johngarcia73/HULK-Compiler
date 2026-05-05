from shell import *
import os
import sys

s = Shell()

source = sys.argv[1] if len(sys.argv) > 1 else "main.cpp"
out_exe = sys.argv[2] if len(sys.argv) > 2 else "my_program"
source = os.path.normpath(source)

# Find headers linked to the source file
raw_deps = s >> f"g++ -MM {source}"

# Simple Python logic to find matching .cpp files
all_files = raw_deps.replace("\\", "").split()
cpp_files = [source]

for f in all_files:
    if f.endswith(".hpp"):
        cpp_candidate = f.replace(".hpp", ".cpp")
        if os.path.exists(cpp_candidate):
            cpp_files.append(cpp_candidate)

# Compile only what we found
print(f"Compiling: {cpp_files}")
out = s >> f"g++ -std=c++20 -O0 -g {' '.join(cpp_files)} -o {out_exe}"
print(out)
