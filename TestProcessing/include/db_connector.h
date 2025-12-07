#ifndef DB_CONNECTOR_H
#define DB_CONNECTOR_H

#include "libpq-fe.h"
#include <string>

class DBConnector {
public:
    DBConnector();
    ~DBConnector();

    bool connect(const std::string& host, const std::string& port,
                const std::string& dbname, const std::string& user,
                const std::string& pass);

    PGresult* query(const std::string& sql);
    bool execute(const std::string& sql);

private:
    PGconn* connection;
};

#endif