#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include "src/core/SocketUtils.h"
#include "src/core/Types.h"

int main(){
	Cgua::App app;
	app.listen("8080");
	return 0;
}
