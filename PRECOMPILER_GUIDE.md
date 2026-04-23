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
    → Scanner          (caracteres → tokens)
    → Parser           (tokens → vector<RouteNode>)
    → CodeGen          (vector<RouteNode> → string C++)
    → archivo .hpp
```

El `try/catch` envuelve todo — cualquier error en cualquier etapa termina aquí con un mensaje limpio y `return 1`.

---

## 3. `lexer.h` — Los contratos antes que la implementación

Hay tres conceptos que entender:

**`Token`** — la unidad atómica: tipo + valor + número de línea.
Ejemplo: `{ARROBA, "@", 3}`

**`IScanner`** — interfaz con un solo método: `getNextToken()`. El parser habla a través de esta interfaz.

**Las tres implementaciones del scanner:**

| Clase | Cómo funciona | Para qué sirve |
|---|---|---|
| `Scanner` | Lee un `istream` carácter por carácter | El pipeline real |
| `TokenStream` | Envuelve un `vector<Token>` ya hecho | Tests, replay |
| `ParallelLexer` | Carga el archivo en memoria, lo divide en 2 y usa threads | Optimización (no conectado aún) |

`Scanner` tiene métodos extras que `IScanner` no tiene: `leerParamsRaw()` y `leerBodyRaw()`. Esto es intencional — el parser necesita captura verbatim del stream, y solo `Scanner` puede hacerlo.

---

## 4. `lexer.cpp` — En dos bloques separados

### Bloque 1: `Scanner`

Léelo como una máquina de estados sobre el stream de caracteres. Los métodos auxiliares se llaman antes de que `getNextToken()` los necesite:

- `peek()` / `advance()` / `isAtEnd()` → navegación básica del stream
- `skipWhitespace()` → salta espacios y trackea `line`
- `leerString()` / `leerNumero()` / `leerIdentificador()` → leen un token completo después de que `getNextToken` ya vio el primer carácter
- `leerParamsRaw()` / `leerBodyRaw()` → modo especial: ignoran la tokenización y capturan texto crudo hasta el delimitador balanceado

El punto clave de `leerParamsRaw` y `leerBodyRaw` es que rastrean **tres estados simultáneos**:

| Variable | Propósito |
|---|---|
| `depth` | Nivel de anidamiento de `()` o `{}` |
| `inString` | Si estamos dentro de un string literal |
| `escaped` | Si el siguiente carácter está escapado con `\` |

El delimitador de cierre solo termina la captura cuando `depth == 0` y no estamos dentro de un string.

### Bloque 2: `ParallelLexer`

Léelo como una optimización independiente. El diseño tiene 4 fases:

1. **`buildTable()`** — construye una tabla de 256 entradas que clasifica cada byte posible en una `CharClass`. Se ejecuta una sola vez vía `call_once`.
2. **Constructor** — carga el archivo entero en `buffer` en una sola llamada `read`.
3. **`tokenize()`** — busca un punto de corte seguro cerca del medio del buffer (nunca dentro de un string), luego lanza 2 threads sobre `scanRange`.
4. **`scanRange()`** — la versión paralela del scanner: usa `charTable[c]` en lugar de `isalpha/isdigit`, todo en memoria.

---

## 5. `parser.h` + `parser.cpp`

**`parser.h`** — define la clase `Parser`. El campo `scanner` es `Scanner&`, no `IScanner&`. Esto es necesario para poder llamar `leerParamsRaw()` y `leerBodyRaw()`, que no están en `IScanner`.

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

**Scanner produce tokens**
```
ARROBA        "@"
IDENTIFICADOR "get"
PAR_IZQ       "("
STRING        "api/users"
PAR_DER       ")"
IDENTIFICADOR "Response"
IDENTIFICADOR "getUsers"
... luego leerParamsRaw/leerBodyRaw capturan el resto
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

## Qué modificar si quieres extender el precompiler

| Quiero... | Archivo a modificar |
|---|---|
| Soportar nuevos tokens (ej. `//` comentarios) | `lexer.cpp` — `getNextToken()` y `scanRange()` |
| Añadir campos al AST (ej. middleware) | `ast.h` → `parser.cpp` → `codegen.cpp` |
| Cambiar el formato del header generado | `codegen.cpp` — `generate()` |
| Soportar sintaxis nueva (ej. `@middleware`) | `parser.cpp` — `parsePrograma()` + nuevo método |
| Conectar el `ParallelLexer` al pipeline | `precompilador.cpp` + añadir raw-capture a `ParallelLexer` |
