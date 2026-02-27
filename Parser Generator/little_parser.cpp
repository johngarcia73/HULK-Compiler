#include <string>
#include <vector>
#include <stdexcept>
#include <iostream>

enum class Token {
    ID,
    PLUS,
    END
};

class Parser {
public:
    Parser(const std::vector<Token>& input)
        : tokens(input), pos(0) {}

    void parse() {
        // Iniciamos la derivación izquierda desde el símbolo inicial E
        E();

        // Correctitud: al terminar, solo es válido si estamos en $
        match(Token::END);

        std::cout << "Cadena aceptada\n";
    }

private:
    const std::vector<Token>& tokens;
    size_t pos;

    // ======= UTILIDADES =======

    Token lookahead() const {
        return tokens[pos];
    }

    void match(Token expected) {
        // Implementa la coincidencia terminal (consumo de entrada)
        if (lookahead() != expected) {
            throw std::runtime_error("Error de sintaxis");
        }
        pos++;
    }

    // ======= NO TERMINALES =======

    // E → T E'
    void E() {
        // Decisión LL(1):
        // FIRST(E) = { id }
        // Si el lookahead es id, esta producción es la única posible
        T();
        Eprime();
    }

    // E' → + T E' | ε
    void Eprime() {
        // Decisión LL(1) basada en FIRST(E')
        if (lookahead() == Token::PLUS) {
            // Caso FIRST(E') contiene '+'
            match(Token::PLUS);
            T();
            Eprime();
        }
        else {
            // Caso ε-producción
            // Teoría:
            // ε se elige si lookahead ∈ FOLLOW(E')
            // FOLLOW(E') = { $ }
            // No consumimos nada
        }
    }

    // T → id
    void T() {
        // FIRST(T) = { id }
        match(Token::ID);
    }
};



int main() {
    // Simula la entrada: id + id + id $
    std::vector<Token> input = {
        Token::ID,
        Token::PLUS,
        Token::ID,
        Token::PLUS,
        Token::ID,
        Token::END
    };

    Parser parser(input);

    try {
        parser.parse();
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
    }
}