#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// CodeGen — consumes the parsed AST, emits a C++ header
//
// Output format: a constexpr Route[] array inside cgua::generated.
// Zero runtime overhead — the route table is resolved entirely at
// compile time by the host application.
// ─────────────────────────────────────────────────────────────────────────────
class CodeGen {
public:
    // Returns the generated C++ source as a std::string.
    static std::string generate(const std::vector<RouteNode>& routes);

    // Writes the generated source to `outPath` (creates or overwrites).
    static void writeToFile(const std::vector<RouteNode>& routes,
                            const std::string& outPath);
};

#endif // CODEGEN_H
