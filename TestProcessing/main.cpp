#include <iostream>
#include "src/config_loader.cpp"
#include "src/api_server.cpp"

using namespace std;

int main() {
    APIServer server;
    std::cout << "API Server created !" << std::endl;
    server.connectToDatabase();
    std::cout << "API Server connected to PostgreSQL DB!" << std::endl;
    server.start();
    std::cout << "API Server started!" << std::endl;
    
    return 0;
}

/*
g++ main.cpp -o app.exe ^
  -IC:"C:\Program Files\PostgreSQL\18\include\" ^
  -LC:"C:\Program Files\PostgreSQL\18\lib\" ^
  -lpq
*/