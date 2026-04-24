#include <iostream>
#include <vector>
#include <string>
#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "codegen.h"

// argc - argument count
// argv - argument vector
int main(int argc, char* argv[]) {

    // check if there are arguments, if not, take the default arguments
    std::string inputFile  = (argc > 1) ? argv[1] : "routes.cgua";
    std::string outputFile = (argc > 2) ? argv[2] : "routes_generated.hpp";

    try {
        // ── 1. Scan + parse (multithreaded) ──────────────────────────────────
        // ParallelScanner carga el archivo, tokeniza con dos threads, y expone
        // leerParamsRaw() / leerBodyRaw() para captura verbatim.
        // Si el archivo no existe, ParallelScanner lanza la excepcion.
        ParallelScanner scanner(inputFile);
        Parser          parser(scanner);

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
