CXX = g++
CXXFLAGS = -Iinclude -std=c++17

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
    ../ir_generator/ir_generator.cpp \
    pipeline_test.cpp \

OBJS = $(SRCS:.cpp=.o)
TARGET = test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $^ -o $@

clean:
	rm -f $(OBJS) $(TARGET)