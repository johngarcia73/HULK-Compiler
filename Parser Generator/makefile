CXX = g++
CXXFLAGS = -Iinclude -std=c++17

SRCS = AST_Builder/ast_builder.cpp \
	   AST_Builder/ast_node.cpp \
       Parser_Building/lalr_algorithms.cpp \
	   Parser_Building/parser_builder.cpp \
	   Parser_Building/parser_runtime.cpp \
       Parser_Building/integration_example.cpp \
       utils/First_Comp/first.cpp \
	   utils/Grammar/grammar.cpp \

OBJS = $(SRCS:.cpp=.o)
TARGET = test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $^ -o $@

clean:
	rm -f $(OBJS) $(TARGET)