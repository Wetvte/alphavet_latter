#include "include/api_server.h"
#include <iostream>
#include <clocale>
#include <windows.h>


int main() {
    // Общая локализация
    setlocale(LC_ALL, "Russian");
    SetConsoleOutputCP(CP_UTF8);

    APIServer server;
    std::cout << "API Server created !" << std::endl;
    server.connect_to_db();
    std::cout << "API Server connected to PostgreSQL DB!" << std::endl;
    server.start();
    
    return 0;
}
