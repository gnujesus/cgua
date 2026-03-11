#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include "src/core/SocketUtils.h"
#include "src/core/Types.h"

int main(){
	// their address
	sockaddr_storage their_addr;
	socklen_t addr_size;

	Cgua::Socket servsock = SocketUtils::create_server_socket("8080");

	// connected_sockfd - file descriptor of the socket that just connected to the server
	int connected_sockfd = 0;

	bind(servsock.sockfd, servsock.servinfo->ai_addr, servsock.servinfo->ai_addrlen);

	// only if you're the client and want to connect 
	// connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);

	if(listen(servsock.sockfd, 128) < 0) {
		SocketUtils::error_n_die("Socket Error");
	}

	addr_size = sizeof(their_addr);
	connected_sockfd = accept(servsock.sockfd, (sockaddr *)&their_addr, &addr_size);

	// recv - receive step
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

	// send - answer Step
	Cgua::HttpResponse res = {};
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



	// free it after you're finished
	freeaddrinfo(servsock.servinfo);
	return 0;
}
