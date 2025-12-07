#include <iostream>
#include "src/config_loader.cpp"
#include "src/api_server.cpp"

using namespace std;

int main() {
    std::cout << "Hello!" << std::endl;
    APIServer server;
    std::cout << "API Server created !" << std::endl;
    server.connect_to_db();
    std::cout << "API Server connected to db!" << std::endl;
    server.start();
    std::cout << "API Server started!" << std::endl;
    
    return 0;
}
