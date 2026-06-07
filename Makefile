CXX := g++
CXXFLAGS := -O2 -std=c++17 -Wall -Wextra

TARGET := hulk

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
	semantic/type_inference_visitor.cpp

.PHONY: build clean

build: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET)

clean:
	rm -f $(TARGET) output
