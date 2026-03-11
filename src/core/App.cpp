#include "SocketUtils.h"
#include "Types.h"

#ifdef _WIN32
	#include <winsock2.h>
	#pragma comment(lib, "ws2_32.lib")
#else 
	#include <unistd.h>
#endif

void Cgua::App::listen(std::string port){
	Cgua::Socket servsock = SocketUtils::create_server_socket(port);
	_listen_sockfd = servsock.sockfd;


	if(bind(servsock.sockfd, servsock.servinfo->ai_addr, servsock.servinfo->ai_addrlen) < 0){
		SocketUtils::error_n_die("Bind error");
	}

	if(::listen(servsock.sockfd, 128) < 0) {
		SocketUtils::error_n_die("Socket Error");
	}

	freeaddrinfo(servsock.servinfo);

	std::printf("C-gua is listening on port %s...\n", port.c_str());
	this->start_loop();
}

void Cgua::App::start_loop(){
	while(true){
		// their addr info
		sockaddr_storage their_addr;
		socklen_t addr_size;

		// connected_sockfd - file descriptor of the socket that just connected to the server
		int connected_sockfd = 0;

		addr_size = sizeof(their_addr);

		// 1. connect the socket
		connected_sockfd = accept(_listen_sockfd, (sockaddr *)&their_addr, &addr_size);

		// 2. receive
		char buf[1024];
		int recv_status;

		recv_status = recv(connected_sockfd, buf, sizeof(buf), 0);

		if(recv_status < 0){
			SocketUtils::error_n_die("Error receiving from the client", recv_status);
		} else if(recv_status == 0){
			SocketUtils::error_n_die("Client closed connection!", recv_status);
		} else {
			std::string req(buf, recv_status);
			std::printf("Received %d bytes of data\nMsg: %s", recv_status, req.c_str());
		}
		
		// 3. answer
		HttpResponse res = {};
		res.body = "GNU was here!";
		int res_len, bytes_sent;

		std::string raw_res = res.toString();

		res_len = raw_res .length();

		// msg.c_str() because the send method was made in c, and doesn't support
		// c++ strings. fir this, we convert the string into an array of chars 
		// (or pointer of type char*, whatever)
		if((bytes_sent = send(connected_sockfd, raw_res.c_str(), res_len, 0)) < 0){
			SocketUtils::error_n_die("Error sending message", bytes_sent);
		}
		
		// 4. close
		close(connected_sockfd);
	}
}	
