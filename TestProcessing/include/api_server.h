#ifndef API_SERVER_H
#define API_SERVER_H

#include "httplib.h"
#include "db_connector.h"
#include "config_loader.h"

class APIServer {
public:
    APIServer();
    void connect_to_db();
    void start();

private:
    Config config;
    httplib::Server server;
    DBConnector db_connector;

    void setup_routes();
    void handle_test(const httplib::Request& req, httplib::Response& res);
    void handle_results(const httplib::Request& req, httplib::Response& res);
};

#endif
