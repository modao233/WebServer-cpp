#include "./webserver/webserver.h"

int main(){
	Config config;
	
	config.init_config();

	//config.print();

	WebServer server(config);
	std::cout << "init server";
	server.Start();
	
	return 0;
}
