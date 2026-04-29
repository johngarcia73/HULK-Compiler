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

# Unit tests target: builds a unit_tests binary including the project sources
UNIT_TARGET = unit_tests

.PHONY: unit
unit: $(UNIT_TARGET)

$(UNIT_TARGET):
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(filter-out pipeline_test.cpp,$(SRCS)) unit/main.cpp unit/test_lexer.cpp unit/test_parser.cpp unit/test_generators.cpp unit/test_semantic.cpp $(LDLIBS) -o $(UNIT_TARGET)

# Run unit tests (passes ARGS to the test binary)
.PHONY: run-unit
run-unit: $(UNIT_TARGET)
	./$(UNIT_TARGET) $(ARGS)

# Generator helpers (create generated artifacts only when requested)
.PHONY: gen-lexer
gen-lexer:
	mkdir -p gen
	$(CXX) $(CXXFLAGS) -I.. ../lexer/Lexer_Generator/test.cpp ../lexer/Lexer_Generator/nfa.cpp ../lexer/Lexer_Generator/preprocessor.cpp ../lexer/Lexer_Generator/dfa.cpp ../lexer/lexer.cpp $(LDLIBS) -o gen/lexer_gen

.PHONY: gen-parser
gen-parser:
	@echo "Parser generator target: build tools under parser/Parser_Generator if needed."
	@echo "Use 'make -C parser/Parser_Generator' or run the project-level build to regenerate parser artifacts."