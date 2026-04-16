#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "codegen.h"

int main(int argc, char* argv[]) {
    std::string inputFile  = (argc > 1) ? argv[1] : "routes.cgua";
    std::string outputFile = (argc > 2) ? argv[2] : "routes_generated.hpp";

    try {
        // ── 1. Open source file ───────────────────────────────────────────────
        std::ifstream file(inputFile);
        if (!file.is_open())
            throw std::runtime_error("No se pudo abrir: " + inputFile);

        // ── 2. Scan + parse ───────────────────────────────────────────────────
        // Scanner is used directly (not via IScanner) so the parser can call
        // leerParamsRaw() / leerBodyRaw() for verbatim capture.
        Scanner scanner(file);
        Parser  parser(scanner);

        std::vector<RouteNode> routes = parser.parsePrograma();

        // ── 3. Summary ────────────────────────────────────────────────────────
        std::cout << "Rutas encontradas: " << routes.size() << "\n";
        for (const auto& r : routes) {
            std::cout << "  @" << r.metodo << "(\"" << r.ruta << "\")  →  "
                      << r.returnType << " " << r.funcName
                      << "(" << r.rawParams << ")\n";
        }

        // ── 4. Emit C++20 header ──────────────────────────────────────────────
        CodeGen::writeToFile(routes, outputFile);
        std::cout << "\nGenerado: " << outputFile << "\n";
    }
    catch (const std::exception& e) {
        std::cerr << "[ERROR] " << e.what() << "\n";
        return 1;
    }

    return 0;
}
