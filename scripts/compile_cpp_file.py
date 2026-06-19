from shell import *
import os
import sys
sh = Shell()

source = sys.argv[1] if len(sys.argv) > 1 else "main.cpp"
out_exe = sys.argv[2] if len(sys.argv) > 2 else "main"
source = os.path.normpath(source)

_ =sh >> "rm hulk main runtime.o" 
_ = sh >> "make -f DMakefile" 
_ =sh >> f"cp hulk {out_exe}"


