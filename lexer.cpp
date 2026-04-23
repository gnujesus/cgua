#include "lexer.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <thread>

// ─────────────────────────────────────────────────────────────────────────────
// Scanner
// ─────────────────────────────────────────────────────────────────────────────
Scanner::Scanner(std::istream& in) : input(in), line(1) {}

char Scanner::peek()    { return static_cast<char>(input.peek()); }
char Scanner::advance() { return static_cast<char>(input.get());  }
bool Scanner::isAtEnd() { return input.peek() == EOF; }

void Scanner::skipWhitespace() {
    while (!isAtEnd() && std::isspace(static_cast<unsigned char>(peek()))) {
        if (peek() == '\n') ++line;
        advance();
    }
}

std::string Scanner::leerString() {
    std::string val;
    // IA
    int startLine = line;
    while (!isAtEnd() && peek() != '"') {
        if (peek() == '\n') ++line;
        val += advance();
    }
    // IA
    if (isAtEnd())
        throw std::runtime_error(
            "Linea " + std::to_string(startLine) + ": string literal sin cerrar");
    advance(); // consume closing "
    return val;
}

std::string Scanner::leerNumero(char first) {
    std::string val(1, first);
    while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek())))
        val += advance();
    return val;
}

std::string Scanner::leerIdentificador(char first) {
    std::string val(1, first);
    while (!isAtEnd() &&
           (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_'))
        val += advance();
    return val;
}

// ─────────────────────────────────────────────────────────────────────────────
// Scanner::leerParamsRaw
//
// Assumes the opening '(' has already been consumed by the tokenizer.
// Reads raw chars until the matching ')' (tracking nested parens and string
// literals so that ')' inside a string doesn't close the parameter list).
// The closing ')' is consumed but not included in the result.
// ─────────────────────────────────────────────────────────────────────────────
std::string Scanner::leerParamsRaw() {
    std::string result;
    int  depth    = 0;
    bool inString = false;
    bool escaped  = false;
    // IA
    bool found    = false;

    while (!isAtEnd()) {
        char c = advance();
        if (c == '\n') ++line;

        if (escaped) {
            result += c;
            escaped = false;
            continue;
        }
        if (c == '\\' && inString) {
            escaped = true;
            result += c;
            continue;
        }

        if (inString) {
            result += c;
            if (c == '"') inString = false;
        } else if (c == '"') {
            inString = true;
            result += c;
        } else if (c == '(') {
            depth++;
            result += c;
        } else if (c == ')') {
            // IA
            if (depth == 0) { found = true; break; }
            depth--;
            result += c;
        } else {
            result += c;
        }
    }
    // IA
    if (!found)
        throw std::runtime_error(
            "Linea " + std::to_string(line) + ": lista de parametros sin cerrar ')'");
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Scanner::leerBodyRaw
//
// Assumes the opening '{' has already been consumed by the tokenizer.
// Reads raw chars until the matching '}' (tracking nested braces and string
// literals).  The closing '}' is consumed but not included in the result.
// ─────────────────────────────────────────────────────────────────────────────
std::string Scanner::leerBodyRaw() {
    std::string result;
    int  depth    = 0;
    bool inString = false;
    bool escaped  = false;
    // IA
    bool found    = false;

    while (!isAtEnd()) {
        char c = advance();
        if (c == '\n') ++line;

        if (escaped) {
            result += c;
            escaped = false;
            continue;
        }
        if (c == '\\' && inString) {
            escaped = true;
            result += c;
            continue;
        }

        if (inString) {
            result += c;
            if (c == '"') inString = false;
        } else if (c == '"') {
            inString = true;
            result += c;
        } else if (c == '{') {
            depth++;
            result += c;
        } else if (c == '}') {
            // IA
            if (depth == 0) { found = true; break; }
            depth--;
            result += c;
        } else {
            result += c;
        }
    }
    // IA
    if (!found)
        throw std::runtime_error(
            "Linea " + std::to_string(line) + ": cuerpo de funcion sin cerrar '}'");
    return result;
}

Token Scanner::getNextToken() {
    skipWhitespace();
    if (isAtEnd()) return {TokenType::FIN_DE_ARCHIVO, "", line};

    char c = advance();
    switch (c) {
        case '@': return {TokenType::ARROBA,    "@", line};
        case '(': return {TokenType::PAR_IZQ,   "(", line};
        case ')': return {TokenType::PAR_DER,   ")", line};
        case '{': return {TokenType::LLAVE_IZQ, "{", line};
        case '}': return {TokenType::LLAVE_DER, "}", line};
        case ',': return {TokenType::COMA,      ",", line};
        case '<': return {TokenType::MENOR_QUE, "<", line};
        case '>': return {TokenType::MAYOR_QUE, ">", line};
        case '&': return {TokenType::AMPERSAND, "&", line};
        case '*': return {TokenType::ASTERISCO, "*", line};
        case ':':
            if (!isAtEnd() && peek() == ':') {
                advance(); // consume second ':'
                return {TokenType::DOBLE_COLON, "::", line};
            }
            return {TokenType::DOS_PUNTOS, ":", line};
        case '"': return {TokenType::STRING, leerString(), line};
        default:
            if (std::isdigit(static_cast<unsigned char>(c)))
                return {TokenType::NUMERO, leerNumero(c), line};
            if (std::isalpha(static_cast<unsigned char>(c)) || c == '_')
                return {TokenType::IDENTIFICADOR, leerIdentificador(c), line};
            return {TokenType::DESCONOCIDO, std::string(1, c), line};
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Token helpers
// ─────────────────────────────────────────────────────────────────────────────
std::string nombreToken(TokenType t) {
    switch (t) {
        case TokenType::ARROBA:         return "ARROBA";
        case TokenType::IDENTIFICADOR:  return "IDENTIFICADOR";
        case TokenType::PAR_IZQ:        return "PAR_IZQ";
        case TokenType::PAR_DER:        return "PAR_DER";
        case TokenType::LLAVE_IZQ:      return "LLAVE_IZQ";
        case TokenType::LLAVE_DER:      return "LLAVE_DER";
        case TokenType::DOS_PUNTOS:     return "DOS_PUNTOS";
        case TokenType::DOBLE_COLON:    return "DOBLE_COLON";
        case TokenType::COMA:           return "COMA";
        case TokenType::MENOR_QUE:      return "MENOR_QUE";
        case TokenType::MAYOR_QUE:      return "MAYOR_QUE";
        case TokenType::AMPERSAND:      return "AMPERSAND";
        case TokenType::ASTERISCO:      return "ASTERISCO";
        case TokenType::STRING:         return "STRING";
        case TokenType::NUMERO:         return "NUMERO";
        case TokenType::DESCONOCIDO:    return "DESCONOCIDO";
        case TokenType::FIN_DE_ARCHIVO: return "FIN_DE_ARCHIVO";
        default:                        return "TOKEN_DESCONOCIDO";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// TokenStream
// ─────────────────────────────────────────────────────────────────────────────
TokenStream::TokenStream(std::vector<Token> toks)
    : tokens(std::move(toks)), pos(0) {}

Token TokenStream::getNextToken() {
    if (pos >= tokens.size())
        return {TokenType::FIN_DE_ARCHIVO, "", 0};
    return tokens[pos++];
}

// ─────────────────────────────────────────────────────────────────────────────
// ParallelLexer — static DP table
// ─────────────────────────────────────────────────────────────────────────────
ParallelLexer::CharClass  ParallelLexer::charTable[256];
// IA
std::once_flag            ParallelLexer::tableOnce;

void ParallelLexer::buildTable() {
    for (int i = 0; i < 256; ++i) charTable[i] = CC_OTHER;

    charTable[static_cast<unsigned char>(' ')]  = CC_SPACE;
    charTable[static_cast<unsigned char>('\t')] = CC_SPACE;
    charTable[static_cast<unsigned char>('\r')] = CC_SPACE;
    charTable[static_cast<unsigned char>('\n')] = CC_NEWLINE;

    for (int c = '0'; c <= '9'; ++c) charTable[c] = CC_DIGIT;
    for (int c = 'a'; c <= 'z'; ++c) charTable[c] = CC_ALPHA;
    for (int c = 'A'; c <= 'Z'; ++c) charTable[c] = CC_ALPHA;
    charTable[static_cast<unsigned char>('_')] = CC_ALPHA;

    charTable[static_cast<unsigned char>('@')] = CC_AT;
    charTable[static_cast<unsigned char>('"')] = CC_QUOTE;
    charTable[static_cast<unsigned char>('(')] = CC_LPAREN;
    charTable[static_cast<unsigned char>(')')] = CC_RPAREN;
    charTable[static_cast<unsigned char>('{')] = CC_LBRACE;
    charTable[static_cast<unsigned char>('}')] = CC_RBRACE;
    charTable[static_cast<unsigned char>(':')] = CC_COLON;
    charTable[static_cast<unsigned char>(',')] = CC_COMMA;
}

ParallelLexer::ParallelLexer(const std::string& filepath) {
    // IA
    std::call_once(tableOnce, buildTable);

    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open())
        throw std::runtime_error("No se pudo abrir: " + filepath);

    auto size = file.tellg();
    if (size <= 0) return;

    file.seekg(0, std::ios::beg);
    buffer.resize(static_cast<std::size_t>(size));
    if (!file.read(buffer.data(), size))
        throw std::runtime_error("Error de lectura: " + filepath);
}

std::vector<Token> ParallelLexer::scanRange(std::size_t from,
                                             std::size_t to,
                                             int         lineStart) const {
    std::vector<Token> tokens;
    tokens.reserve((to - from) / 6);

    int         line = lineStart;
    std::size_t i    = from;

    while (i < to) {
        CharClass cc = charTable[static_cast<unsigned char>(buffer[i])];

        if (cc == CC_SPACE)   { ++i; continue; }
        if (cc == CC_NEWLINE) { ++line; ++i; continue; }

        switch (cc) {
            case CC_AT:     tokens.push_back({TokenType::ARROBA,    "@", line}); ++i; break;
            case CC_LPAREN: tokens.push_back({TokenType::PAR_IZQ,   "(", line}); ++i; break;
            case CC_RPAREN: tokens.push_back({TokenType::PAR_DER,   ")", line}); ++i; break;
            case CC_LBRACE: tokens.push_back({TokenType::LLAVE_IZQ, "{", line}); ++i; break;
            case CC_RBRACE: tokens.push_back({TokenType::LLAVE_DER, "}", line}); ++i; break;
            case CC_COMMA:  tokens.push_back({TokenType::COMA,      ",", line}); ++i; break;
            case CC_COLON: {
                if (i + 1 < to && buffer[i + 1] == ':') {
                    tokens.push_back({TokenType::DOBLE_COLON, "::", line});
                    i += 2;
                } else {
                    tokens.push_back({TokenType::DOS_PUNTOS, ":", line});
                    ++i;
                }
                break;
            }
            case CC_QUOTE: {
                ++i; // skip opening "
                // IA
                std::string val;
                bool        esc = false;
                while (i < to) {
                    unsigned char uc = static_cast<unsigned char>(buffer[i]);
                    if (esc) { val += buffer[i++]; esc = false; continue; }
                    if (buffer[i] == '\\') { val += buffer[i++]; esc = true; continue; }
                    if (charTable[uc] == CC_NEWLINE) ++line;
                    if (charTable[uc] == CC_QUOTE)  { ++i; break; }
                    val += buffer[i++];
                }
                tokens.push_back({TokenType::STRING, std::move(val), line});
                break;
            }
            case CC_DIGIT: {
                std::size_t start = i;
                while (i < to &&
                       charTable[static_cast<unsigned char>(buffer[i])] == CC_DIGIT) ++i;
                tokens.push_back({TokenType::NUMERO,
                                  std::string(buffer.data() + start, i - start),
                                  line});
                break;
            }
            case CC_ALPHA: {
                std::size_t start = i;
                CharClass   cur;
                while (i < to &&
                       ((cur = charTable[static_cast<unsigned char>(buffer[i])]) == CC_ALPHA ||
                        cur == CC_DIGIT)) ++i;
                tokens.push_back({TokenType::IDENTIFICADOR,
                                  std::string(buffer.data() + start, i - start),
                                  line});
                break;
            }
            default:
                tokens.push_back({TokenType::DESCONOCIDO,
                                  std::string(1, buffer[i]),
                                  line});
                ++i;
                break;
        }
    }

    return tokens;
}

std::vector<Token> ParallelLexer::tokenize() {
    if (buffer.empty())
        return {{TokenType::FIN_DE_ARCHIVO, "", 1}};

    std::size_t mid     = buffer.size() / 2;
    // IA
    bool        inStr   = false;
    bool        escaped = false;

    for (std::size_t k = 0; k < mid; ++k) {
        if (escaped)                    { escaped = false; continue; }
        if (buffer[k] == '\\' && inStr) { escaped = true;  continue; }
        if (buffer[k] == '"')           inStr = !inStr;
    }

    while (mid < buffer.size()) {
        if (escaped)                         { escaped = false; ++mid; continue; }
        if (buffer[mid] == '\\' && inStr)    { escaped = true;  ++mid; continue; }
        if (buffer[mid] == '"') inStr = !inStr;
        if (!inStr) {
            CharClass cc = charTable[static_cast<unsigned char>(buffer[mid])];
            if (cc == CC_SPACE || cc == CC_NEWLINE) break;
        }
        ++mid;
    }

    int lineOffset2 = 1;
    for (std::size_t k = 0; k < mid; ++k)
        if (buffer[k] == '\n') ++lineOffset2;

    std::vector<Token> tokensA, tokensB;

    std::thread t1([&]() { tokensA = scanRange(0,   mid,           1);           });
    std::thread t2([&]() { tokensB = scanRange(mid, buffer.size(), lineOffset2); });

    t1.join();
    t2.join();

    tokensA.reserve(tokensA.size() + tokensB.size() + 1);
    tokensA.insert(tokensA.end(),
                   std::make_move_iterator(tokensB.begin()),
                   std::make_move_iterator(tokensB.end()));

    int lastLine = tokensA.empty() ? 1 : tokensA.back().linea;
    tokensA.push_back({TokenType::FIN_DE_ARCHIVO, "", lastLine});

    return tokensA;
}
