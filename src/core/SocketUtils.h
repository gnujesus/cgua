#ifndef CGUA_SOCKETUTILS_H
#define CGUA_SOCKETUTILS_H

#include <iostream>
#include <cstring>
#include <cerrno>
#include <cstdarg>
#include <cstdlib>
#include <netdb.h>
#include <string>
#include "Types.h"

namespace SocketUtils{
	Cgua::Socket create_server_socket(std::string port); // socket(), bind(), listen()

	// implementation into the header file since 
	// this is using a template. the compiler needs this here.
	template<typename... Args>
	inline void error_n_die(const std::string &fmt, Args... args){
	// when a function fails, it sets errno to a number inmediately. 
	// this number corresponds to a value on <cerrno>, it has to be saved and 
	// checked inmediately before another function modifies the value
	int errno_save = errno;

	std::cerr << "[FATAL] ";
	std::cerr << fmt << " ";
	(std::cerr << ... << args);

	if(errno_save != 0){
		std::cerr << " | OS Error: ";
		std::cerr << std::strerror(errno_save);
	}

	std::cerr << std::endl;
	std::exit(EXIT_FAILURE);
	}
}

#endif
