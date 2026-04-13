#include "parser.h"
#include <stdexcept>

Parser::Parser(Scanner& s) : scanner(s) {
    advance();
}

void Parser::advance() {
    current = scanner.getNextToken();
}

bool Parser::check(TokenType tipo) {
    return current.tipo == tipo;
}

Token Parser::consume(TokenType tipo, const string& mensaje) {
    if (current.tipo != tipo) {
        throw runtime_error(
            "Error en linea " + to_string(current.linea) +
            ": " + mensaje +
            ". Token encontrado: '" + current.valor + "'"
        );
    }

    Token temp = current;
    advance();
    return temp;
}

bool Parser::esMetodoValido(const string& metodo) {
    return metodo == "get" ||
           metodo == "post" ||
           metodo == "put" ||
           metodo == "delete";
}

RouteNode Parser::parseDecoradorRuta() {
    consume(TokenType::ARROBA, "Se esperaba '@'");

    Token metodo = consume(TokenType::IDENTIFICADOR, "Se esperaba el nombre del metodo");

    if (!esMetodoValido(metodo.valor)) {
        throw runtime_error(
            "Error en linea " + to_string(metodo.linea) +
            ": metodo HTTP invalido '" + metodo.valor + "'"
        );
    }

    consume(TokenType::PAR_IZQ, "Se esperaba '(' despues del metodo");

    Token ruta = consume(TokenType::STRING, "Se esperaba la ruta en un string");

    consume(TokenType::PAR_DER, "Se esperaba ')' despues de la ruta");

    return {metodo.valor, ruta.valor};
}

vector<RouteNode> Parser::parsePrograma() {
    vector<RouteNode> rutas;

    while (!check(TokenType::FIN_DE_ARCHIVO)) {
        rutas.push_back(parseDecoradorRuta());
    }

    return rutas;
}
