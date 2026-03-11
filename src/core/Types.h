#ifndef CGUA_TYPES_H
#define CGUA_TYPES_H

#include <netdb.h>
#include <string>
#include <sys/socket.h>
#include <vector>

namespace Cgua {
	struct Socket {
		int sockfd;
		addrinfo *servinfo;
	};

	struct HttpResponse {
			// default values of the response
			std::string version = "HTTP/1.1";
			std::string status = "200 OK";
			std::string contentType = "text/plain";
			std::string body = "";

			// structure of the full string
			std::string toString() const {
					return version + " " + status + "\r\n" +
								 "Content-Type: " + contentType + "\r\n" +
								 "Content-Length: " + std::to_string(body.length()) + "\r\n" +
								 "\r\n" +
								 body;
			}
	};

	class App {
	public:
		App(): _listen_sockfd(-1){}
		void listen(std::string port);

	private: 
		int _listen_sockfd;
		void start_loop();
	};
}

#endif
