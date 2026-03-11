#ifndef CGUA_TYPES_H
#define CGUA_TYPES_H

#include <netdb.h>
#include <string>
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
}

#endif
