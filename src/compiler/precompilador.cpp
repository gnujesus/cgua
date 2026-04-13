#include <iostream>
#include <fstream>
#include <vector>
#include "lexer.h"
#include "parser.h"
#include "ast.h"

using namespace std;

int main() {
    ifstream file("routes.cgua");

    if (!file.is_open()) {
        cout << "Error al abrir routes.cgua\n";
        return 1;
    }

    try {
        Scanner scanner(file);
        Parser parser(scanner);

        vector<RouteNode> rutas = parser.parsePrograma();

        cout << "Rutas parseadas:\n";
        for (const auto& ruta : rutas) {
            cout << "Metodo: " << ruta.metodo
                 << " | Ruta: " << ruta.ruta << endl;
        }
    }
    catch (const exception& e) {
        cerr << e.what() << endl;
        return 1;
    }

    return 0;
}
