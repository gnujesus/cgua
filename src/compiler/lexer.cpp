#include "lexer.h"
#include <cctype>

Scanner::Scanner(istream& in) : input(in), line(1) {}

char Scanner::peek() {
    return static_cast<char>(input.peek());
}

char Scanner::advance() {
    return static_cast<char>(input.get());
}

bool Scanner::isAtEnd() {
    return input.peek() == EOF;
}

void Scanner::skipWhitespace() {
    while (!isAtEnd() && isspace(peek())) {
        if (peek() == '\n') {
            line++;
        }
        advance();
    }
}

string Scanner::leerString() {
    string valor;

    while (!isAtEnd() && peek() != '"') {
        if (peek() == '\n') {
            line++;
        }
        valor += advance();
    }

    if (!isAtEnd() && peek() == '"') {
        advance(); // consume la comilla de cierre
    }

    return valor;
}

string Scanner::leerNumero(char primero) {
    string valor;
    valor += primero;

    while (!isAtEnd() && isdigit(peek())) {
        valor += advance();
    }

    return valor;
}

string Scanner::leerIdentificador(char primero) {
    string valor;
    valor += primero;

    while (!isAtEnd() && (isalnum(peek()) || peek() == '_')) {
        valor += advance();
    }

    return valor;
}

Token Scanner::getNextToken() {
    skipWhitespace();

    if (isAtEnd()) {
        return {TokenType::FIN_DE_ARCHIVO, "", line};
    }

    char c = advance();

    switch (c) {
        case '@':
            return {TokenType::ARROBA, "@", line};

        case '(':
            return {TokenType::PAR_IZQ, "(", line};

        case ')':
            return {TokenType::PAR_DER, ")", line};

        case '{':
            return {TokenType::LLAVE_IZQ, "{", line};

        case '}':
            return {TokenType::LLAVE_DER, "}", line};

        case ':':
            return {TokenType::DOS_PUNTOS, ":", line};

        case ',':
            return {TokenType::COMA, ",", line};

        case '"':
            return {TokenType::STRING, leerString(), line};

        default:
            if (isdigit(c)) {
                return {TokenType::NUMERO, leerNumero(c), line};
            }

            if (isalpha(c) || c == '_') {
                return {TokenType::IDENTIFICADOR, leerIdentificador(c), line};
            }

            return {TokenType::DESCONOCIDO, string(1, c), line};
    }
}

string nombreToken(TokenType t) {
    switch (t) {
        case TokenType::ARROBA: return "ARROBA";
        case TokenType::IDENTIFICADOR: return "IDENTIFICADOR";
        case TokenType::PAR_IZQ: return "PAR_IZQ";
        case TokenType::PAR_DER: return "PAR_DER";
        case TokenType::LLAVE_IZQ: return "LLAVE_IZQ";
        case TokenType::LLAVE_DER: return "LLAVE_DER";
        case TokenType::DOS_PUNTOS: return "DOS_PUNTOS";
        case TokenType::COMA: return "COMA";
        case TokenType::STRING: return "STRING";
        case TokenType::NUMERO: return "NUMERO";
        case TokenType::DESCONOCIDO: return "DESCONOCIDO";
        case TokenType::FIN_DE_ARCHIVO: return "FIN_DE_ARCHIVO";
        default: return "TOKEN_DESCONOCIDO";
    }
}
