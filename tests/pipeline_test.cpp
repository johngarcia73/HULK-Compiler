#include "../common/token.hpp"
#include "../parser/utils/Grammar/grammar.hpp"
#include "../parser/Parser_Generator/lalr_builder.hpp"
#include "../parser/Parser_Generator/parser_interface.hpp"
#include "../parser/AST_Builder/ast_node.hpp"
#include "../parser/AST_Builder/ast_builder.hpp"
#include "../parser/Parser_Generator/genparser.hpp"
#include "../lexer/lexer.hpp"
#include "../semantic/analyzer.hpp"
#include "../ir_generator/llvm_ir_generator.hpp"
#include "../ir_generator/llvm_ir_executor.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

std::string grammar_dir = "../parser/Parser_Generator/grammar.y";
std::string program_dir = "program.hulk";

int main() {
    try {
        std::cout << "\n=== FULL PIPELINE TEST (Parser + Semantic + LLVM IR) ===\n\n";

        // 1. oad grammar & build parser
        Grammar grammar = build_grammar_from_file(grammar_dir);
        std::unordered_map<std::string, uint32_t> name_to_id;
        for (size_t i = 0; i < grammar.symtab.size(); ++i)
            name_to_id[grammar.symtab[i].name] = i;

        uint32_t eof_id = name_to_id["$"];
        LALRParser parser = LALRParser::from_grammar(grammar, eof_id);
        parser.set_builder(std::make_unique<ASTBuilder>());

        // Lexer 
        Lexer lexer(default_token_specs());
        std::ifstream file(program_dir);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open " << program_dir << "\n";
            return 1;
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string input = buffer.str();
        file.close();
        std::cout << "Input program:\n" << input << "\n";

        // 3. Tokenization
        std::vector<Token> raw_tokens = lexer.tokenize(input);
        std::vector<Token> tokens;
        for (auto& t : raw_tokens) {
            std::string token_name = token_type_to_string(static_cast<TokenType>(t.symbol_id));
            auto it = name_to_id.find(token_name);
            if (it == name_to_id.end()) {
                std::cerr << "Unknown token: " << token_name << "\n";
                return 1;
            }
            tokens.emplace_back(it->second, t.value, t.line, t.column);
        }
        tokens.emplace_back(name_to_id["$"], "$", 0, 0);

        // 4. Parsing
        ParseResult result = parser.parse(tokens);
        if (!result.is_success()) {
            std::cerr << "Parse failed: " << result.error_message << "\n";
            return 1;
        }
        ProgramNode* program = dynamic_cast<ProgramNode*>(result.ast.get());
        if (!program) {
            std::cerr << "Root is not a ProgramNode\n";
            return 1;
        }

        // Semantic Analysis
        SemanticSymbolTable symTable;
        SemanticAnalyzer analyzer(symTable);
        analyzer.analyze(program);
        if (analyzer.hasErrors()) {
            std::cerr << "Semantic analysis failed:\n";
            analyzer.reportErrors();
            return 1;
        }
        std::cout << "Semantic analysis successful.\n";

        std::cout << "\n=== Abstract Syntax Tree (with types) ===\n";
        program->print(std::cout);
        std::cout << "\n";

        /*
        // LLVM IR Generation
        LLVMIRGenerator llvmGen;
        if (!llvmGen.generate(program)) {
            std::cerr << "LLVM IR generation failed.\n";
            return 1;
        }
        llvmGen.writeToFile("output.ll");
        std::cout << "LLVM IR written to output.ll\n";

        // Compile & execute using the executor
        LLVMExecutor executor;
        int exitCode = executor.compileAndRun(program, "output.ll", "program");
        if (exitCode >= 0)
            std::cout << "Program exited with code " << exitCode << "\n";

        return 0;
        */
    } catch (const std::exception& e) {
        std::cerr << "FATAL: " << e.what() << "\n";
        return 2;
    }
}