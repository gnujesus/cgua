#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"
#include <vector>

class Parser {
private:
    Scanner& scanner;
    Token current;

public:
    Parser(Scanner& s);

    void advance();
    bool check(TokenType tipo);
    Token consume(TokenType tipo, const string& mensaje);
    bool esMetodoValido(const string& metodo);

    RouteNode parseDecoradorRuta();
    vector<RouteNode> parsePrograma();
};

#endif
