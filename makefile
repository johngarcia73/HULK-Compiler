CXX = g++
CXXFLAGS = -Iinclude -std=c++17

SRCS = \
    Parser_Generator/AST_Builder/ast_builder.cpp \
    Parser_Generator/Parser_Building/lalr_algorithms.cpp \
    Parser_Generator/Parser_Building/parser_builder.cpp \
    Parser_Generator/Parser_Building/parser_runtime.cpp \
    Parser_Generator/Parser_Building/pipeline_test.cpp \
    Parser_Generator/utils/First_Comp/first.cpp \
    Parser_Generator/utils/Grammar/grammar.cpp \
    Lexer_Generator/dfa.cpp \
    Lexer_Generator/lexer.cpp \
    Lexer_Generator/nfa.cpp \
    Lexer_Generator/preprocessor.cpp \

OBJS = $(SRCS:.cpp=.o)
TARGET = test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $^ -o $@

clean:
	rm -f $(OBJS) $(TARGET)