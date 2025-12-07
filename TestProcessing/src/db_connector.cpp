#ifndef DB_CONNECTOR
#define DB_CONNECTOR

#include "../include/db_connector.h"
#include <iostream>

DBConnector::DBConnector() : connection(nullptr) {}

DBConnector::~DBConnector() {
    if (connection) PQfinish(connection);
}

bool DBConnector::connect(const std::string& host, const std::string& port,
                       const std::string& dbname, const std::string& user,
                       const std::string& pass) {
    std::string conninfo = "host=" + host + " port=" + port +
                           " dbname=" + dbname + " user=" + user +
                           " password=" + pass;

    connection = PQconnectdb(conninfo.c_str());
    return PQstatus(connection) == CONNECTION_OK;
}

PGresult* DBConnector::query(const std::string& sql) {
    return PQexec(connection, sql.c_str());
}

bool DBConnector::execute(const std::string& sql) {
    PGresult* res = query(sql);
    bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return success;
}

#endif