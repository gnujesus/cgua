#include "parser.h"
#include <stdexcept>

Parser::Parser(IFullScanner& s) : scanner(s) {
    advance();
}

void Parser::advance() {
    current = scanner.getNextToken();
}

bool Parser::check(TokenType tipo) {
    return current.tipo == tipo;
}

Token Parser::consume(TokenType tipo, const std::string& mensaje) {
    if (current.tipo != tipo) {
        throw std::runtime_error(
            "Error en linea " + std::to_string(current.linea) +
            ": " + mensaje +
            ". Token encontrado: '" + current.valor + "'"
        );
    }
    Token temp = current;
    advance();
    return temp;
}

bool Parser::esMetodoValido(const std::string& metodo) {
    return metodo == "get"    ||
           metodo == "post"   ||
           metodo == "put"    ||
           metodo == "delete" ||
           metodo == "patch";
}

// ─────────────────────────────────────────────────────────────────────────────
// Parser::reconstructType
//
// Builds a C++ type string from tokens 0..(funcNameIdx-1).
// Handles qualified names (std::string), templates (vector<T>),
// references (T&), pointers (T*), and const qualifiers.
// ─────────────────────────────────────────────────────────────────────────────
std::string Parser::reconstructType(
    const std::vector<std::pair<TokenType, std::string>>& sigTokens,
    int funcNameIdx)
{
    std::string result;

    auto needsSpaceBefore = [](char last) {
        return last != '<' && last != ':' && last != '&' && last != '*';
    };

    for (int i = 0; i < funcNameIdx; ++i) {
        const auto& [type, val] = sigTokens[i];
        switch (type) {
            case TokenType::IDENTIFICADOR:
                if (!result.empty() && needsSpaceBefore(result.back()))
                    result += ' ';
                result += val;
                break;
            case TokenType::DOBLE_COLON:  result += "::"; break;
            case TokenType::MENOR_QUE:    result += "<";  break;
            case TokenType::MAYOR_QUE:    result += ">";  break;
            case TokenType::AMPERSAND:    result += "&";  break;
            case TokenType::ASTERISCO:    result += "*";  break;
            default: break;
        }
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Parser::parseDecoradorRuta
//
// Parses the full annotated handler:
//
//   @method("path")
//   ReturnType funcName(params) {
//       body
//   }
//
// Steps:
//   1. Consume @method("path")
//   2. Collect signature tokens until the function's '('
//   3. Split signature into returnType + funcName
//   4. Switch to raw-capture mode: read params verbatim until matching ')'
//   5. Switch to raw-capture mode: read body verbatim until matching '}'
// ─────────────────────────────────────────────────────────────────────────────
RouteNode Parser::parseDecoradorRuta() {
    // ── 1. Decorator ─────────────────────────────────────────────────────────
    consume(TokenType::ARROBA, "Se esperaba '@'");

    Token metodoTok = consume(TokenType::IDENTIFICADOR,
                              "Se esperaba el nombre del metodo HTTP");
    if (!esMetodoValido(metodoTok.valor)) {
        // IA
        throw std::runtime_error(
            "Error en linea " + std::to_string(metodoTok.linea) +
            ": metodo HTTP invalido '" + metodoTok.valor +
            "'. Metodos validos (en minusculas): get, post, put, delete, patch"
        );
    }

    consume(TokenType::PAR_IZQ, "Se esperaba '(' despues del metodo");
    Token rutaTok = consume(TokenType::STRING, "Se esperaba la ruta como string");
    consume(TokenType::PAR_DER, "Se esperaba ')' para cerrar el decorador");

    // ── 2. Signature tokens (up to the function's '(') ───────────────────────
    // After the last consume(), 'current' holds the first token of the function
    // signature (the return type's leading identifier or qualifier).
    std::vector<std::pair<TokenType, std::string>> sigTokens;

    while (!check(TokenType::PAR_IZQ) && !check(TokenType::FIN_DE_ARCHIVO)) {
        sigTokens.push_back({current.tipo, current.valor});
        advance();
    }

    if (check(TokenType::FIN_DE_ARCHIVO)) {
        throw std::runtime_error(
            "Se esperaba la firma de la funcion despues del decorador @" +
            metodoTok.valor + "(\"" + rutaTok.valor + "\")"
        );
    }

    // ── 3. Split signature → returnType + funcName ───────────────────────────
    int funcNameIdx = -1;
    for (int i = static_cast<int>(sigTokens.size()) - 1; i >= 0; --i) {
        if (sigTokens[i].first == TokenType::IDENTIFICADOR) {
            funcNameIdx = i;
            break;
        }
    }
    if (funcNameIdx < 0) {
        throw std::runtime_error(
            "No se pudo determinar el nombre de la funcion para la ruta '" +
            rutaTok.valor + "'"
        );
    }

    std::string funcName   = sigTokens[funcNameIdx].second;
    std::string returnType = reconstructType(sigTokens, funcNameIdx);

    // IA
    if (returnType.empty())
        throw std::runtime_error(
            "Falta tipo de retorno para la funcion '" + funcName +
            "' en la ruta '" + rutaTok.valor + "'. " +
            "Sintaxis esperada: tipo_retorno nombre_funcion(params)"
        );

    // ── 4. Raw params ─────────────────────────────────────────────────────────
    // current == PAR_IZQ  →  the stream is positioned right after '('.
    // We call leerParamsRaw() WITHOUT advancing first so that no tokens are
    // consumed ahead of the raw read.
    // After leerParamsRaw() the stream sits right after the closing ')'.
    std::string rawParams = scanner.leerParamsRaw();
    advance(); // tokenize the first token after ')' (should be LLAVE_IZQ)

    // ── 5. Raw body ───────────────────────────────────────────────────────────
    // current == LLAVE_IZQ  →  the stream is positioned right after '{'.
    if (!check(TokenType::LLAVE_IZQ)) {
        throw std::runtime_error(
            "Error en linea " + std::to_string(current.linea) +
            ": Se esperaba '{' para el cuerpo de '" + funcName + "'"
        );
    }
    // leerBodyRaw() reads from the stream (already past '{') until matching '}'.
    std::string body = scanner.leerBodyRaw();
    advance(); // tokenize the first token after '}' (next '@' or EOF)

    return {metodoTok.valor, rutaTok.valor, returnType, funcName, rawParams, body};
}

std::vector<RouteNode> Parser::parsePrograma() {
    std::vector<RouteNode> rutas;

    while (!check(TokenType::FIN_DE_ARCHIVO)) {
        rutas.push_back(parseDecoradorRuta());
    }

    return rutas;
}
