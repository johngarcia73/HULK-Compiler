# Toolchain
CXX       := g++
CC        := gcc
CXXFLAGS  := -O2 -std=c++20 -Wall -Wextra
CFLAGS    := -O2 -Wall

# Directories
QBE_DIR   := deps/qbe
GC_DIR    := deps/bdwgc

# Outputs
QBE_BIN   := $(QBE_DIR)/qbe
GC_LIB    := $(GC_DIR)/libgc.a       # we’ll create from gc.a
RUNTIME_O := runtime.o               # compiled C runtime
TARGET    := hulk    

SRCS := \
	main.cpp \
	compiler/frontend_cache.cpp \
	compiler/frontend_pipeline.cpp \
	parser/AST_Builder/ast_builder.cpp \
	parser/AST_Builder/ast_node.cpp \
	parser/Parser_Generator/lalr_algorithms.cpp \
	parser/Parser_Generator/parser_builder.cpp \
	parser/Parser_Generator/parser_runtime.cpp \
	parser/utils/First_Comp/first.cpp \
	parser/utils/Grammar/grammar.cpp \
	lexer/Lexer_Generator/dfa.cpp \
	lexer/Lexer_Generator/nfa.cpp \
	lexer/Lexer_Generator/preprocessor.cpp \
	lexer/lexer.cpp \
	semantic/analyzer.cpp \
	semantic/dependency_graph.cpp \
	semantic/type_inference_visitor.cpp\
	ir_generator/lowering.cpp\
	ir_generator/scope_table.cpp\
	ir_generator/ir_generator.cpp \

.PHONY: all clean

all: qbe libgc $(RUNTIME_O) $(TARGET)   # everything

# 1. QBE
qbe: $(QBE_BIN)
$(QBE_BIN):
	$(MAKE) -C $(QBE_DIR)

# 2. Boehm GC static library
libgc: $(GC_LIB)
$(GC_LIB):
	$(MAKE) -C $(GC_DIR) -f Makefile.direct
	cp $(GC_DIR)/gc.a $(GC_LIB)

# 3. Runtime object file (compiled once, linked later)
$(RUNTIME_O): runtime/runtime.c | libgc
	$(CC) $(CFLAGS) -I$(GC_DIR)/include -c $< -o $@

# 4. Compiler binary (hulk)
$(TARGET): $(SRCS) | qbe libgc
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET)

# Clean
clean:
	rm -f $(TARGET) $(RUNTIME_O) output
	$(MAKE) -C $(QBE_DIR) clean
	$(MAKE) -C $(GC_DIR) -f Makefile.direct clean || true
	rm -f $(GC_LIB)
