#pragma once

#include <cstdint>
#include <vector>
#include "token.hpp"

// Clase base para todos los nodos del AST (el usuario debe heredar de ella)
class ASTNode {
public:
    virtual ~ASTNode() = default;
};

// Interfaz que debe implementar el usuario para construir el AST durante el parseo
class ASTBuilder {
public:
    virtual ~ASTBuilder() = default;

    // Llamado cuando se desplaza un terminal (shift).
    // symbol: identificador del terminal.
    // token:  el token completo (incluye valor, línea, columna).
    // result: debe asignarse con el nodo hoja correspondiente (puede ser nullptr si el terminal no genera nodo).
    virtual void onShift(uint32_t symbol, const Token& token, ASTNode*& result) = 0;

    // Llamado cuando se reduce una producción.
    // prod_id: identificador de la producción aplicada.
    // rhs:     nodos correspondientes a los símbolos de la parte derecha,
    //          en el mismo orden en que aparecen en la producción (los nullptr indican terminales sin nodo).
    // result:  debe asignarse con el nodo resultante para el lado izquierdo.
    virtual void onReduce(uint32_t prod_id, const std::vector<ASTNode*>& rhs, ASTNode*& result) = 0;
};