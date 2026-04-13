#ifndef LEXER_H
#define LEXER_H

#include <iostream>
#include <string>
using namespace std;

enum class TokenType {
    ARROBA,
    IDENTIFICADOR,
    PAR_IZQ,
    PAR_DER,
    LLAVE_IZQ,
    LLAVE_DER,
    DOS_PUNTOS,
    COMA,
    STRING,
    NUMERO,
    DESCONOCIDO,
    FIN_DE_ARCHIVO
};

struct Token {
    TokenType tipo;
    string valor;
    int linea;
};

class Scanner {
private:
    istream& input;
    int line;

public:
    Scanner(istream& in);

    char peek();
    char advance();
    bool isAtEnd();
    void skipWhitespace();

    string leerString();
    string leerNumero(char primero);
    string leerIdentificador(char primero);

    Token getNextToken();
};

string nombreToken(TokenType t);

#endif
