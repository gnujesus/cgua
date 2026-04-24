#ifndef LEXER_H
#define LEXER_H

#include <cstdint>
#include <cstddef>
#include <istream>
#include <string>
#include <vector>
// IA
#include <mutex>

// ─────────────────────────────────────────────────────────────────────────────
// Token types
// ─────────────────────────────────────────────────────────────────────────────
enum class TokenType {
    ARROBA,
    IDENTIFICADOR,
    PAR_IZQ,
    PAR_DER,
    LLAVE_IZQ,
    LLAVE_DER,
    DOS_PUNTOS,
    DOBLE_COLON,   // ::  (used in C++ qualified names like std::string)
    COMA,
    MENOR_QUE,     // <   (used in template types like vector<T>)
    MAYOR_QUE,     // >
    AMPERSAND,     // &   (used in reference types)
    ASTERISCO,     // *   (used in pointer types)
    STRING,
    NUMERO,
    DESCONOCIDO,
    FIN_DE_ARCHIVO
};

struct Token {
    TokenType   tipo;
    std::string valor;
    int         linea;
    std::size_t offset = 0; // posicion en el buffer justo despues de que el token termina
};

std::string nombreToken(TokenType t);

// ─────────────────────────────────────────────────────────────────────────────
// IScanner — abstract interface used by the parser for token-level access
// ─────────────────────────────────────────────────────────────────────────────
class IScanner {
public:
    virtual Token getNextToken() = 0;
    virtual ~IScanner() = default;
};

// ─────────────────────────────────────────────────────────────────────────────
// IFullScanner — extiende IScanner con raw-capture que necesita el parser
// ─────────────────────────────────────────────────────────────────────────────
class IFullScanner : public IScanner {
public:
    virtual std::string leerParamsRaw() = 0;
    virtual std::string leerBodyRaw()   = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// Scanner — streaming, single-threaded (istream-based)
//
// Extends IScanner with raw-capture methods that read character-by-character
// from the underlying stream.  These are used by the parser to capture
// function parameter lists and bodies verbatim, preserving whitespace and
// formatting for direct inclusion in the generated C++ output.
// ─────────────────────────────────────────────────────────────────────────────
class Scanner : public IFullScanner {
private:
    std::istream& input;
    int           line;

public:
    explicit Scanner(std::istream& in);

    char peek();
    char advance();
    bool isAtEnd();
    void skipWhitespace();

    std::string leerString();
    std::string leerNumero(char primero);
    std::string leerIdentificador(char primero);

    // ── Raw-capture helpers ───────────────────────────────────────────────────
    // These assume that the opening delimiter has already been consumed by the
    // tokenizer (i.e. the stream is positioned right after '(' or '{').

    // Reads raw characters until the matching ')' (tracking nested parens).
    // The closing ')' is consumed but NOT included in the returned string.
    std::string leerParamsRaw() override;

    // Reads raw characters until the matching '}' (tracking nested braces,
    // respecting string literals).
    // The closing '}' is consumed but NOT included in the returned string.
    std::string leerBodyRaw()   override;

    Token getNextToken()        override;
};

// ─────────────────────────────────────────────────────────────────────────────
// TokenStream — wraps a pre-tokenized vector<Token>
// ─────────────────────────────────────────────────────────────────────────────
class TokenStream : public IScanner {
private:
    std::vector<Token> tokens;
    std::size_t        pos;

public:
    explicit TokenStream(std::vector<Token> toks);
    Token getNextToken() override;
};

// ─────────────────────────────────────────────────────────────────────────────
// ParallelLexer — multithreaded + DP char-class table
//
// Design:
//   1. The entire file is loaded into a contiguous in-memory buffer in one
//      read(2) call — no per-character syscall overhead.
//
//   2. A 256-entry CharClass lookup table is computed ONCE on first use
//      (tabulation DP: build the full table upfront so every character
//      classification during scanning is a simple table[c] lookup — O(1),
//      branch-free).
//
//   3. The buffer is split near its midpoint.  The split point is nudged
//      forward to the next whitespace boundary so neither thread starts
//      mid-token.  A quick O(n/2) newline count provides the correct line
//      offset for Thread 2.
//
//   4. Two std::threads scan their halves independently (no shared mutable
//      state, no locks needed).  Each thread does O(n/2) work.
//
//   5. Results are merged in order.  Total wall-clock scan time ≈ O(n/2).
//
// NOTE: Usado por ParallelScanner, que añade raw-capture y funciona como
// el scanner principal del pipeline.
// ─────────────────────────────────────────────────────────────────────────────
class ParallelLexer {
public:
    enum CharClass : uint8_t {
        CC_SPACE = 0,
        CC_ALPHA,
        CC_DIGIT,
        CC_AT,
        CC_QUOTE,
        CC_LPAREN,
        CC_RPAREN,
        CC_LBRACE,
        CC_RBRACE,
        CC_COLON,
        CC_COMMA,
        CC_NEWLINE,
        CC_OTHER
    };

private:
    static CharClass      charTable[256];
    // IA
    static std::once_flag tableOnce;
    static void           buildTable();

    std::string buffer;

    std::vector<Token> scanRange(std::size_t from,
                                 std::size_t to,
                                 int         lineStart) const;

public:
    explicit ParallelLexer(const std::string& filepath);
    std::vector<Token> tokenize();
    const std::string& getBuffer() const { return buffer; }
};

// ─────────────────────────────────────────────────────────────────────────────
// ParallelScanner — tokenizacion multithreaded + raw-capture desde buffer
//
// Usa ParallelLexer internamente para tokenizar con 2 threads.
// Expone leerParamsRaw() / leerBodyRaw() leyendo del buffer en memoria.
// Reemplaza directamente a Scanner en el pipeline del precompiler.
// ─────────────────────────────────────────────────────────────────────────────
class ParallelScanner : public IFullScanner {
    std::string        buffer;
    std::vector<Token> tokens;
    std::size_t        tokPos;
    std::size_t        bufPos;

    std::string captureUntil(char openDelim, char closeDelim);

public:
    explicit ParallelScanner(const std::string& filepath);
    Token       getNextToken() override;
    std::string leerParamsRaw() override;
    std::string leerBodyRaw()   override;
};

#endif // LEXER_H
