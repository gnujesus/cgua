#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"
#include <string>
#include <utility>
#include <vector>

class Parser {
private:
    Scanner& scanner;  // Scanner (not IScanner) — needed for raw-capture methods
    Token    current;

    // Reconstruct a C++ type string from collected signature tokens.
    // sigTokens holds all tokens between the decorator's ')' and the function's '('.
    // funcNameIdx is the index of the last IDENTIFICADOR (the function name),
    // so everything before it forms the return type.
    static std::string reconstructType(
        const std::vector<std::pair<TokenType, std::string>>& sigTokens,
        int funcNameIdx);

public:
    explicit Parser(Scanner& s);

    void  advance();
    bool  check(TokenType tipo);
    Token consume(TokenType tipo, const std::string& mensaje);
    bool  esMetodoValido(const std::string& metodo);

    // Parses: @method("path") ReturnType funcName(params) { body }
    // Returns a fully populated RouteNode.
    RouteNode parseDecoradorRuta();

    std::vector<RouteNode> parsePrograma();
};

#endif // PARSER_H
