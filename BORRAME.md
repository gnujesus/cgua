# C-gua — Guía de Implementación y Pruebas

> Este archivo documenta los componentes en el root del proyecto (lexer, parser,
> precompilador, generador de código) y explica cómo probar que todo funciona.
> **Bórralo** cuando ya no lo necesites.

---

## Mapa de archivos

```
cgua/
├── lexer.h / lexer.cpp        ← Análisis léxico (Scanner + ParallelLexer)
├── parser.h / parser.cpp      ← Análisis sintáctico → AST
├── ast.h                      ← Nodos del AST (RouteNode)
├── codegen.h / codegen.cpp    ← Generador de código C++
├── precompilador.cpp          ← main() del precompilador
├── routes.cgua                ← Archivo de rutas de ejemplo
├── routes_generated.hpp       ← Output del precompilador (auto-generado)
├── main.cpp                   ← main() del servidor HTTP
├── src/core/
│   ├── Types.h                ← Tipos: Socket, HttpResponse, App
│   ├── SocketUtils.h/cpp      ← Wrappers de sockets POSIX
│   └── App.cpp                ← Bucle de accept/recv/send
└── Makefile
```

---

## Componentes del precompilador

### 1. Lexer (`lexer.h` / `lexer.cpp`)

Transforma el texto fuente en una secuencia de `Token`.

**Tres clases:**

| Clase | Rol |
|---|---|
| `IScanner` | Interfaz abstracta con `getNextToken()`. Permite que el Parser no sepa si está leyendo desde un stream o desde un vector pre-tokenizado. |
| `Scanner` | Lee carácter a carácter desde cualquier `std::istream`. Útil para tests con `std::istringstream` sin tocar el disco. |
| `TokenStream` | Envuelve un `vector<Token>` (producido por `ParallelLexer`) y lo expone como `IScanner`. |
| `ParallelLexer` | Lee el archivo completo en memoria y tokeniza en dos hilos. |

#### ParallelLexer: programación dinámica + multithreading

**Tabla DP de clases de caracteres (`charTable[256]`)**

```
Problema:   en cada carácter hay que decidir si es dígito, letra, símbolo, etc.
Solución DP: precomputar una tabla de 256 entradas (una por valor ASCII) la
             primera vez que se construye un ParallelLexer.  Durante el escaneo,
             cada clasificación es `charTable[(unsigned char)c]` — O(1),
             sin ningún branch adicional.
```

Esto es *tabulation DP*: se resuelve el subproblema base (¿a qué clase pertenece
cada byte posible?) una sola vez y se almacena para reutilización constante.

**Flujo de tokenización en paralelo**

```
Buffer completo en memoria
        │
        ▼
  n/2 ──┤── encontrar límite seguro (no dentro de un string literal)
        │
   ┌────┴────┐
Thread 1   Thread 2
[0, mid)  [mid, n)
   └────┬────┘
        │ join()
        ▼
   merge(tokensA, tokensB)
        │
        ▼
   vector<Token> completo
```

- Cada hilo hace O(n/2) trabajo → complejidad total O(n/2) en tiempo de reloj.
- No hay estado mutable compartido durante el escaneo → sin mutex.
- El offset de línea para Thread 2 se calcula con un conteo de `\n` en O(n/2)
  antes de lanzar los hilos.

---

### 2. AST (`ast.h`)

Estructura plana, sin heap allocation adicional:

```cpp
struct RouteNode {
    std::string metodo;  // "get", "post", "put", "delete", "patch"
    std::string ruta;    // "/api/users/:id"
};
```

---

### 3. Parser (`parser.h` / `parser.cpp`)

Gramática reconocida:

```
programa       ::= decorador*
decorador      ::= '@' metodo '(' STRING ')'
metodo         ::= 'get' | 'post' | 'put' | 'delete' | 'patch'
```

Recibe un `IScanner&` — funciona con `Scanner` (stream) o `TokenStream`
(output del ParallelLexer) sin ningún cambio.

**Métodos clave:**

| Método | Descripción |
|---|---|
| `consume(tipo, msg)` | Consume el token actual si coincide con `tipo`; lanza `runtime_error` si no. |
| `parseDecoradorRuta()` | Parsea un decorador completo, devuelve `RouteNode`. |
| `parsePrograma()` | Itera hasta `FIN_DE_ARCHIVO`, devuelve `vector<RouteNode>`. |

---

### 4. Generador de código (`codegen.h` / `codegen.cpp`)

Toma el `vector<RouteNode>` del parser y emite un header C++:

```cpp
namespace cgua {
namespace generated {

struct Route { const char* method; const char* path; };

inline constexpr Route routes[] = {
    {"GET",  "/api/users"},
    {"POST", "/api/users"},
    // ...
};

inline constexpr std::size_t route_count = N;
}}
```

- `constexpr` → el compilador del host puede resolver la tabla en compile time.
- `inline` → el header puede ser incluido desde múltiples TUs sin ODR violation.
- Sin heap, sin constructores virtuales, costo en runtime = 0.

---

### 5. Precompilador (`precompilador.cpp`)

Orquesta el pipeline completo:

```
routes.cgua
    │
    └─▶ ParallelLexer::tokenize()
            │
            └─▶ TokenStream (IScanner)
                    │
                    └─▶ Parser::parsePrograma()
                                │
                                └─▶ CodeGen::writeToFile()
                                            │
                                            └─▶ routes_generated.hpp
```

Acepta argumentos:
```bash
./cgua_precomp [input.cgua] [output.hpp]
# defaults: routes.cgua → routes_generated.hpp
```

---

## Guía de pruebas

### Requisitos

```bash
g++ --version   # >= 9 (C++17 + std::thread)
make --version
```

### Compilar todo

```bash
make all
```

Produce dos binarios:
- `cgua_precomp` — el precompilador
- `cgua_server`  — el servidor HTTP

### Probar el precompilador

```bash
make test
```

Esto ejecuta `./cgua_precomp routes.cgua` y muestra el `.hpp` generado.
Deberías ver 12 rutas parseadas y el archivo `routes_generated.hpp` en el root.

**Prueba con archivo custom:**
```bash
echo '@get("/ping")\n@post("/echo")' > test.cgua
./cgua_precomp test.cgua out.hpp
cat out.hpp
```

**Prueba de error de sintaxis:**
```bash
echo '@invalidmethod("/ruta")' > bad.cgua
./cgua_precomp bad.cgua   # debe imprimir [ERROR] y salir con código 1
```

### Probar el servidor HTTP

```bash
./cgua_server
# En otra terminal:
curl http://localhost:8080
```

Responde con `GNU was here!` en texto plano.

### Integrar el header generado en el servidor

Una vez que `routes_generated.hpp` existe, el servidor puede incluirlo:

```cpp
// main.cpp (ejemplo de uso futuro)
#include "routes_generated.hpp"

// En tiempo de compilación:
static_assert(cgua::generated::route_count > 0, "Sin rutas registradas");

// En runtime (iteración de ejemplo):
for (std::size_t i = 0; i < cgua::generated::route_count; ++i) {
    const auto& r = cgua::generated::routes[i];
    std::printf("%s %s\n", r.method, r.path);
}
```

---

## Flujo de desarrollo recomendado

1. Edita `routes.cgua` con los decoradores de tu API.
2. Ejecuta `./cgua_precomp routes.cgua` → genera `routes_generated.hpp`.
3. El servidor incluye `routes_generated.hpp` y registra las rutas.
4. `make all` compila ambos con `-O2 -pthread`.

El precompilador es un paso de build previo a la compilación del servidor —
análogo a un `protoc` o un generador de código de ORM.
