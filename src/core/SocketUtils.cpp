#include "SocketUtils.h"
#include "Types.h"
#include <netdb.h>

// @params port 
// @return sockfd
Cgua::Socket SocketUtils::create_server_socket(std::string port){
	// AF_INET = ipv4
	// SOCK_STREAM = TCP_PROTOCOL (streams, contraty to datagrams which are UDP)

	int status;
	addrinfo hints;
	addrinfo *servinfo;


	// make sure struct is empty (remove any possibility of garbage values)
	memset(&hints, 0, sizeof(hints));

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	// this could be modified, since on error, errno will be set, so I don't have 
	// to pass the status.
	// just following the book y'know
	if((status = getaddrinfo(NULL, port.c_str(), &hints, &servinfo)) != 0){
		SocketUtils::error_n_die("Error while getting the address info", status);
	}

	// sockfd = socket file descriptor
	// sockets are just like files, you write on them
	// and each file has it's own "id", or file descriptor
	int sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

	Cgua::Socket sock = {};
	sock.sockfd = sockfd;
	sock.servinfo = servinfo;

	return sock;
}
