#pragma once
#include "ast_node.hpp"
#include <string>
#include <vector>
#include <optional>

/*
 Value: representa un elemento en el RHS de la reducción.
 Puede ser:
  - node: un ASTNode* (resultado de una reducción anterior)
  - token_text: el lexema del token (por ejemplo para NUMBER o IDENTIFIER)
  - int_value: en caso de haber parseado ya como entero
*/
struct Value {
    ASTNode* node = nullptr;
    std::optional<std::string> token_text;
    std::optional<long long> int_value;

    static Value from_node(ASTNode* n) { Value v; v.node = n; return v; }
    static Value from_text(const std::string &s) { Value v; v.token_text = s; return v; }
    static Value from_int(long long x) { Value v; v.int_value = x; return v; }
};

/*
 ASTBuilder: interfaz concreta con implementación en ast_builder.cpp.
 build(production_id, rhs_values) -> ASTNode* (propiedad transferida: el caller ya no debe free los children porque el builder toma o reutiliza lo que haga falta)
*/
struct ASTBuilder {
    ASTBuilder() = default;
    ~ASTBuilder() = default;

    // Construye un nodo AST a partir de la reducción por "production_id".
    // rhs: vector de valores en el orden L->R del RHS.
    // Debe devolver un ASTNode* o nullptr si la producción no crea nodo (p. ej. epsilon).
    ASTNode* build(size_t production_id, const std::vector<Value>& rhs);
};