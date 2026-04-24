# Guía para leer el precompiler de C-Gua

El precompiler es un pipeline clásico de compilador en miniatura. Léelo en este orden:

---

## 1. `ast.h` — El modelo de datos

Es el archivo más corto y define `RouteNode`, la estructura que atraviesa todo el pipeline. Antes de leer cualquier otra cosa, memoriza sus 6 campos:

| Campo | Ejemplo |
|---|---|
| `metodo` | `"get"`, `"post"`, etc. |
| `ruta` | `"api/users/:id"` |
| `returnType` | `"Response"` |
| `funcName` | `"getUsers"` |
| `rawParams` | `"Request req"` — texto verbatim, sin los paréntesis externos |
| `body` | `"return ...;"` — texto verbatim, sin las llaves externas |

Todo el precompiler existe para producir un `vector<RouteNode>` a partir de un `.cgua` y convertirlo en C++.

---

## 2. `precompilador.cpp` — El pipeline completo en 40 líneas

Lee esto segundo, aunque sea el `main`. Te da el mapa del viaje completo antes de entrar en detalles:

```
Archivo .cgua
    → ParallelScanner   (2 threads tokenizan en paralelo + raw-capture)
    → Parser            (tokens → vector<RouteNode>)
    → CodeGen           (vector<RouteNode> → string C++)
    → archivo .hpp
```

El `try/catch` envuelve todo — cualquier error en cualquier etapa termina aquí con un mensaje limpio y `return 1`. Los argumentos opcionales `argv[1]` y `argv[2]` permiten cambiar los archivos de entrada y salida desde la terminal.

---

## 3. `lexer.h` — Los contratos antes que la implementación

Hay cuatro conceptos que entender:

**`Token`** — la unidad atómica: tipo + valor + número de línea + offset.

```cpp
struct Token {
    TokenType   tipo;
    std::string valor;
    int         linea;
    std::size_t offset; // posicion en el buffer justo DESPUES de que el token termina
};
```

El campo `offset` es clave para el multithreading: permite al `ParallelScanner` saber exactamente dónde está en el buffer tras consumir un token.

**Las dos interfaces:**

| Interfaz | Métodos | Propósito |
|---|---|---|
| `IScanner` | `getNextToken()` | Consumir tokens uno a uno |
| `IFullScanner` | `getNextToken()` + `leerParamsRaw()` + `leerBodyRaw()` | Lo que el parser realmente necesita |

`IFullScanner` hereda de `IScanner`. El `Parser` habla exclusivamente a través de `IFullScanner`.

**Las tres implementaciones:**

| Clase | Hereda de | Cómo funciona | Para qué sirve |
|---|---|---|---|
| `Scanner` | `IFullScanner` | Lee un `istream` carácter por carácter | Tests, fuentes que no son archivos |
| `TokenStream` | `IScanner` | Envuelve un `vector<Token>` ya hecho | Replay de tokens |
| `ParallelScanner` | `IFullScanner` | 2 threads tokenizan + raw-capture desde buffer | El pipeline real |

---

## 4. `lexer.cpp` — En tres bloques separados

### Bloque 1: `Scanner`

Léelo como una máquina de estados sobre el stream de caracteres. Los métodos auxiliares se llaman antes de que `getNextToken()` los necesite:

- `peek()` / `advance()` / `isAtEnd()` → navegación básica del stream
- `skipWhitespace()` → salta espacios y trackea `line`
- `leerString()` / `leerNumero()` / `leerIdentificador()` → leen un token completo después de que `getNextToken` ya vio el primer carácter
- `leerParamsRaw()` / `leerBodyRaw()` → modo especial: capturan texto crudo hasta el delimitador balanceado

El punto clave de `leerParamsRaw` y `leerBodyRaw` es que rastrean **tres estados simultáneos**:

| Variable | Propósito |
|---|---|
| `depth` | Nivel de anidamiento de `()` o `{}` |
| `inString` | Si estamos dentro de un string literal |
| `escaped` | Si el siguiente carácter está escapado con `\` |

El delimitador de cierre solo termina la captura cuando `depth == 0` y no estamos dentro de un string.

### Bloque 2: `ParallelLexer`

Es el motor de tokenización paralela. El diseño tiene 4 fases:

1. **`buildTable()`** — construye una tabla de 256 entradas que clasifica cada byte posible en una `CharClass`. Se ejecuta una sola vez vía `std::call_once` (thread-safe).
2. **Constructor** — carga el archivo entero en `buffer` en una sola llamada `read`.
3. **`tokenize()`** — busca un punto de corte seguro cerca del medio del buffer (nunca dentro de un string, rastreando escapes), luego lanza 2 threads sobre `scanRange`.
4. **`scanRange()`** — la versión paralela del scanner: usa `charTable[c]` en lugar de `isalpha/isdigit`, todo en memoria. Cada token producido incluye su `offset` (posición en el buffer justo después del token).

`ParallelLexer` solo tokeniza — no hace raw-capture. Para eso existe `ParallelScanner`.

### Bloque 3: `ParallelScanner`

Es el adaptador que une `ParallelLexer` con el pipeline del parser. Su constructor hace tres cosas:

```cpp
ParallelLexer lexer(filepath);
tokens = lexer.tokenize();   // 2 threads tokenizan aqui
buffer = lexer.getBuffer();  // copia el buffer en memoria
```

Luego expone la misma interfaz que `Scanner`:

**`getNextToken()`** — devuelve `tokens[tokPos++]` y actualiza `bufPos = tok.offset`. Así el buffer siempre queda posicionado justo después del último token consumido.

**`leerParamsRaw()` / `leerBodyRaw()`** — llaman a `captureUntil()`, que lee caracteres directamente de `buffer[bufPos...]` hasta el delimitador balanceado. Al terminar, hace algo crítico:

```cpp
// Avanza tokPos mas alla de los tokens que fueron capturados verbatim
while (tokPos < tokens.size() && tokens[tokPos].offset <= bufPos)
    tokPos++;
```

Sin esto, el parser pediría el siguiente token y `tokPos` estaría apuntando al interior de los params/body que ya fueron capturados. Este avance re-sincroniza el stream de tokens con la posición real en el buffer.

---

## 5. `parser.h` + `parser.cpp`

**`parser.h`** — el `Parser` toma `IFullScanner&`, no `Scanner&`. Esto lo hace compatible tanto con `Scanner` como con `ParallelScanner`.

**`parser.cpp`** — toda la lógica real está en `parseDecoradorRuta()`. Léelo siguiendo sus 5 pasos:

```
Paso 1 → consume @metodo("ruta")
           ↓
Paso 2 → recolecta tokens de la firma hasta encontrar '('
           ↓
Paso 3 → separa returnType del funcName
          (el último IDENTIFICADOR antes de '(' es el funcName,
           todo lo anterior es el tipo de retorno)
           ↓
Paso 4 → leerParamsRaw()  ← cambia de tokens a caracteres crudos
           ↓
Paso 5 → leerBodyRaw()    ← ídem
```

**Sobre el paso 3:** `reconstructType` recibe los tokens de la firma y el índice del último `IDENTIFICADOR`. Itera desde el inicio hasta ese índice (sin incluirlo) y reconstruye el tipo pegando los tokens:

| Token | Resultado |
|---|---|
| `DOBLE_COLON` | `::` |
| `MENOR_QUE` | `<` |
| `MAYOR_QUE` | `>` |
| `IDENTIFICADOR` | el valor tal cual |

`parsePrograma()` simplemente llama `parseDecoradorRuta()` en loop hasta EOF — el archivo entero es una secuencia plana de rutas.

---

## 6. `codegen.h` + `codegen.cpp`

**`codegen.h`** — dos métodos estáticos: `generate()` devuelve un `string`, `writeToFile()` lo escribe. Sin estado.

**`codegen.cpp`** — `generate()` construye el header en partes. Léelo en orden de las secciones que emite:

| Sección | Qué es |
|---|---|
| `#pragma once` + includes | Cabecera del archivo dentro del namespace `cgua` |
| `FixedString<N>` | Wrapper para usar strings como parámetros de template (C++20 NTTP) |
| `Route<M,P,Fn>` | Descriptor compile-time de una ruta |
| Handlers | Una función `inline` por cada `RouteNode` |
| `RouteTable` | `std::tuple<Route<...>, Route<...>, ...>` |
| `route_count` | `constexpr std::size_t` con el total de rutas |

La idea central: **todo es `constexpr`**. El compilador del proyecto que use el header generado resuelve la tabla de rutas completamente en tiempo de compilación — cero overhead en runtime.

---

## El flujo de datos completo de un solo handler

Para anclar todo, sigue este handler del principio al fin:

**`routes.cgua`**
```
@get("api/users")
Response getUsers(Request req) {
    return Response::ok(db.all<User>());
}
```

**`ParallelScanner` — dos threads tokenizan el archivo**
```
Thread 1 (primera mitad):          Thread 2 (segunda mitad):
ARROBA        "@"    offset=1      ... continua desde el punto de corte
IDENTIFICADOR "get"  offset=4
PAR_IZQ       "("   offset=5
STRING        "api/users" offset=15
PAR_DER       ")"   offset=16
IDENTIFICADOR "Response" offset=25
IDENTIFICADOR "getUsers" offset=34
PAR_IZQ       "("   offset=35     ← bufPos se setea aqui antes de leerParamsRaw
...
```

**Parser produce `RouteNode`**
```
{
  metodo:     "get",
  ruta:       "api/users",
  returnType: "Response",
  funcName:   "getUsers",
  rawParams:  "Request req",
  body:       "\n    return Response::ok(db.all<User>());\n"
}
```

**CodeGen emite en el header**
```cpp
inline Response getUsers(Request req) {
    return Response::ok(db.all<User>());
}

// y en RouteTable:
Route<"GET", "api/users", getUsers>
```

---

## Por qué el multithreading funciona sin locks

Los dos threads de `ParallelLexer` operan sobre rangos **completamente separados** del mismo buffer de solo lectura. No escriben en posiciones compartidas — cada thread produce su propio `vector<Token>`. No hay memoria mutable compartida, así que no hacen falta mutex ni atomics.

El único cuidado es el punto de corte: `tokenize()` busca un boundary seguro (whitespace fuera de un string literal, rastreando escapes) para que ningún thread empiece en medio de un token.

---

## Qué modificar si quieres extender el precompiler

| Quiero... | Archivo a modificar |
|---|---|
| Soportar nuevos tokens (ej. `//` comentarios) | `lexer.cpp` — `getNextToken()` y `scanRange()` |
| Añadir campos al AST (ej. middleware) | `ast.h` → `parser.cpp` → `codegen.cpp` |
| Cambiar el formato del header generado | `codegen.cpp` — `generate()` |
| Soportar sintaxis nueva (ej. `@middleware`) | `parser.cpp` — `parsePrograma()` + nuevo método |
| Añadir un tercer thread al lexer | `lexer.cpp` — `ParallelLexer::tokenize()` |
