CXX = g++
# CXX and flags
CXXFLAGS = -Iinclude -std=c++17 $(shell llvm-config --cxxflags)

LDFLAGS = $(shell llvm-config --ldflags)
LDLIBS = $(shell llvm-config --libs core)

SRCS = \
    ../parser/AST_Builder/ast_builder.cpp \
    ../parser/AST_Builder/ast_node.cpp \
    ../parser/Parser_Generator/lalr_algorithms.cpp \
    ../parser/Parser_Generator/parser_builder.cpp \
    ../parser/Parser_Generator/parser_runtime.cpp \
    ../parser/utils/First_Comp/first.cpp \
    ../parser/utils/Grammar/grammar.cpp \
    ../lexer/Lexer_Generator/dfa.cpp \
    ../lexer/lexer.cpp \
    ../lexer/Lexer_Generator/nfa.cpp \
    ../lexer/Lexer_Generator/preprocessor.cpp \
    ../semantic/analyzer.cpp \
    ../semantic/type_inference_visitor.cpp \
    ../ir_generator/llvm_ir_generator.cpp \
    ../ir_generator/llvm_ir_executor.cpp \
    ../semantic/dependency_graph.cpp \
    pipeline_test.cpp

OBJS = $(SRCS:.cpp=.o)
TARGET = test

all: $(TARGET)

# Regla de enlace: añade LDFLAGS y LDLIBS
$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	rm -f $(OBJS) $(TARGET)