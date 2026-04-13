#include <iostream>
#include <fstream>
#include <cctype>
#include <string>
using namespace std;

// opciones que tiene de ser el token
enum class TokenType {
    LLAVE_IZQ,
    LLAVE_DER,
    DOS_PUNTOS,
    COMA,
    STRING,
    NUMERO,
    DESCONOCIDO,
    FIN_DE_ARCHIVO
};

// estructura que tiene token
struct Token {
    TokenType tipo;
    string valor;
    int linea;
};

// clase de escaner
class Scanner {
private:
    istream& input;
    int line;

public:
    // constructor
    Scanner(istream& in) : input(in), line(1) {}

    // lee sin cambiar de posicion
    char peek() {
        return input.peek();
    }

    // avanza un caracter
    char advance() {
        return input.get();
    }

    bool isAtEnd() {
        return input.eof();
    }

    void skipWhitespace() {
        while (!isAtEnd() && isspace(peek())) {
            if (peek() == '\n') {
                line++;
            }
            advance();
        }
    }

    // lee el contenido entre comillas
    string leerString() {
        string valor = "";

        while (!isAtEnd() && peek() != '"') {
            if (peek() == '\n') {
                line++;
            }
            valor += advance();
        }

        // consumir la comilla de cierre si existe
        if (!isAtEnd() && peek() == '"') {
            advance();
        }

        return valor;
    }

    // lee numeros enteros
    string leerNumero(char primerDigito) {
        string valor = "";
        valor += primerDigito;

        while (!isAtEnd() && isdigit(peek())) {
            valor += advance();
        }

        return valor;
    }

    Token getNextToken() {
        skipWhitespace();

        if (isAtEnd()) {
            return {TokenType::FIN_DE_ARCHIVO, "", line};
        }

        char c = advance();

        switch (c) {
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

                return {TokenType::DESCONOCIDO, string(1, c), line};
        }
    }
};

int main() {
    ifstream file("input.txt");

    if (!file.is_open()) {
        cout << "Error al abrir el archivo\n";
        return 1;
    }

    Scanner scanner(file);

    while (true) {
        Token t = scanner.getNextToken();

        cout << "Tipo: " << (int)t.tipo
             << " Valor: " << t.valor
             << " Linea: " << t.linea << endl;

        if (t.tipo == TokenType::FIN_DE_ARCHIVO) {
            break;
        }
    }

    file.close();
    return 0;
}
