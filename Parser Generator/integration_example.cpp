// ============================================================================
// integration_example.cpp
//
// Example showing integration between Lexer and Parser with AST construction
// ============================================================================

#include "grammar.hpp"
#include "lalr_builder.hpp"
#include "parser_interface.hpp"
#include "ast_builder.hpp"   // necesario para la interfaz ASTBuilder
#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <cassert>

// ============================================================================
// Definición de nodos AST concretos para expresiones aritméticas
// ============================================================================

class ExprNode : public ASTNode {
public:
    virtual ~ExprNode() = default;
    virtual int evaluate() const = 0;   // para comprobación
    virtual void print() const = 0;
};

class NumberNode : public ExprNode {
    int value;
public:
    NumberNode(int v) : value(v) {}
    int evaluate() const override { return value; }
    void print() const override { std::cout << value; }
};

class BinaryOpNode : public ExprNode {
    char op;
    std::unique_ptr<ExprNode> left;
    std::unique_ptr<ExprNode> right;
public:
    BinaryOpNode(char o, ExprNode* l, ExprNode* r) : op(o), left(l), right(r) {}
    int evaluate() const override {
        switch (op) {
            case '+': return left->evaluate() + right->evaluate();
            case '*': return left->evaluate() * right->evaluate();
            default: return 0;
        }
    }
    void print() const override {
        std::cout << "(";
        left->print();
        std::cout << " " << op << " ";
        right->print();
        std::cout << ")";
    }
};

// ============================================================================
// Builder concreto para construir el AST durante el parseo
// ============================================================================

class ExprASTBuilder : public ASTBuilder {
public:
    void onShift(uint32_t symbol, const Token& token, ASTNode*& result) override {
        if (symbol == 0) { // NUMBER, asumiendo que su ID es 0 (primero añadido)
            result = new NumberNode(std::stoi(token.value));
        } else {
            // Los operadores '+' y '*' no generan nodo propio
            result = nullptr;
        }
    }

    void onReduce(uint32_t prod_id, const std::vector<ASTNode*>& rhs, ASTNode*& result) override {
        // Según las producciones de la gramática:
        // prod_id 0: E → E + T
        // prod_id 1: E → T
        // prod_id 2: T → T * F
        // prod_id 3: T → F
        // prod_id 4: F → number
        // prod_id 5: S' → E

        if (prod_id == 0) { // E → E + T
            // rhs[0] = E (nodo), rhs[1] = '+' (nullptr), rhs[2] = T (nodo)
            result = new BinaryOpNode('+', static_cast<ExprNode*>(rhs[0]), static_cast<ExprNode*>(rhs[2]));
        }
        else if (prod_id == 1) { // E → T
            result = rhs[0]; // simplemente propaga el nodo de T
        }
        else if (prod_id == 2) { // T → T * F
            // rhs[0] = T, rhs[1] = '*' (nullptr), rhs[2] = F
            result = new BinaryOpNode('*', static_cast<ExprNode*>(rhs[0]), static_cast<ExprNode*>(rhs[2]));
        }
        else if (prod_id == 3) { // T → F
            result = rhs[0];
        }
        else if (prod_id == 4) { // F → number
            result = rhs[0]; // ya es un NumberNode
        }
        else if (prod_id == 5) { // S' → E
            result = rhs[0]; // la raíz del AST es el nodo de E
        }
        else {
            result = nullptr;
        }
    }
};

// ============================================================================
// Función auxiliar para imprimir el AST (opcional)
// ============================================================================

void printAST(const ASTNode* node) {
    if (!node) return;
    auto expr = dynamic_cast<const ExprNode*>(node);
    if (expr) {
        expr->print();
    } else {
        std::cout << "<unknown node>";
    }
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "\n=== LALR(1) Parser - AST Integration Example ===\n\n";

    // Create grammar
    Grammar grammar;

    // Add terminals (from lexer)
    uint32_t NUMBER = grammar.symtab.add(SymbolKind::Terminal, "number");
    uint32_t PLUS   = grammar.symtab.add(SymbolKind::Terminal, "+");
    uint32_t TIMES  = grammar.symtab.add(SymbolKind::Terminal, "*");
    uint32_t DOLLAR = grammar.symtab.add(SymbolKind::End, "$");

    // Add non-terminals
    uint32_t E = grammar.symtab.add(SymbolKind::NonTerminal, "E");
    uint32_t T = grammar.symtab.add(SymbolKind::NonTerminal, "T");
    uint32_t F = grammar.symtab.add(SymbolKind::NonTerminal, "F");
    uint32_t S_prime = grammar.symtab.add(SymbolKind::NonTerminal, "S'");

    // Add productions
    // E -> E + T | T
    grammar.add_production(E, {E, PLUS, T});  // prod 0
    grammar.add_production(E, {T});           // prod 1
    // T -> T * F | F
    grammar.add_production(T, {T, TIMES, F}); // prod 2
    grammar.add_production(T, {F});           // prod 3
    // F -> number
    grammar.add_production(F, {NUMBER});      // prod 4
    // S' -> E
    grammar.add_production(S_prime, {E});     // prod 5

    grammar.start_symbol = S_prime;
    grammar.build_indices();

    // Generate parser
    LALRParser parser = LALRParser::from_grammar(grammar, DOLLAR);
    std::cout << "✓ Parser generated: " << parser.state_count() << " states\n";
    std::cout << "  Symbols in grammar: " << grammar.symtab.size() << "\n";
    std::cout << "  Productions: " << grammar.get_productions().size() << "\n";
    std::cout << "  NUMBER symbol ID: " << NUMBER << "\n\n";

    // (Opcional) Depuración del estado 0 de la tabla ACTION
    // Nota: _tables es privado; si quieres acceder, necesitas un getter o hacerlo público.
    // Aquí lo dejamos comentado para evitar errores de compilación.
    /*
    std::cout << "Estado 0:\n";
    for (const auto& kv : parser._tables.ACTION[0]) {
        std::cout << "  Símbolo " << kv.first << " -> ";
        if (kv.second.kind == ActionKind::Shift) std::cout << "Shift a " << kv.second.value;
        else if (kv.second.kind == ActionKind::Reduce) std::cout << "Reduce prod " << kv.second.value;
        else if (kv.second.kind == ActionKind::Accept) std::cout << "Accept";
        else std::cout << "Error";
        std::cout << "\n";
    }
    */

    // Configurar el builder para construir el AST
    auto builder = std::make_unique<ExprASTBuilder>();
    parser.set_builder(std::move(builder));

    // Example: Parse "2 + 3 * 4"
    std::vector<Token> tokens = {
        Token(NUMBER, "2", 1, 1),
        Token(PLUS,   "+", 1, 3),
        Token(NUMBER, "3", 1, 5),
        Token(TIMES,  "*", 1, 7),
        Token(NUMBER, "4", 1, 9),
    };

    std::cout << "Parsing: 2 + 3 * 4\n";
    ParseResult result = parser.parse(tokens);

    if (result.is_success()) {
        std::cout << "✓ Parse succeeded!\n";
        std::cout << "  Reductions: ";
        for (size_t i = 0; i < result.reduction_sequence.size(); ++i) {
            if (i > 0) std::cout << " ";
            std::cout << "P" << result.reduction_sequence[i];
        }
        std::cout << "\n";

        // Verificar que se construyó el AST
        if (result.ast) {
            std::cout << "  AST construido: ";
            printAST(result.ast.get());
            std::cout << "\n";

            // Evaluar el AST para comprobar que el resultado es 14 (2 + 3*4 = 14)
            ExprNode* root = dynamic_cast<ExprNode*>(result.ast.get());
            if (root) {
                int value = root->evaluate();
                std::cout << "  Evaluación: " << value << "\n";
                if (value == 14) {
                    std::cout << "  ✓ El AST es correcto (2 + 3*4 = 14)\n";
                } else {
                    std::cout << "  ✗ Error: se esperaba 14, pero se obtuvo " << value << "\n";
                }
            } else {
                std::cout << "  ✗ El nodo raíz no es una expresión válida\n";
            }
        } else {
            std::cout << "  No se construyó AST (builder no configurado o error)\n";
        }
    } else {
        std::cout << "✗ Parse failed: " << result.error_message << "\n";
    }

    std::cout << "\n";
    return 0;
}