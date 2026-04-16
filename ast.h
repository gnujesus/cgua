#ifndef AST_H
#define AST_H

#include <string>

struct RouteNode {
    std::string metodo;      // http method: "get", "post", etc.
    std::string ruta;        // route path: "/api/users"
    std::string returnType;  // C++ return type: "Response"
    std::string funcName;    // handler function name: "getUsers"
    std::string rawParams;   // raw parameter list (without outer parens)
    std::string body;        // raw function body (without outer braces)
};

#endif
