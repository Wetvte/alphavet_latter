#ifndef API_SERVER
#define API_SERVER

#include "../include/api_server.h"
#include "db_connector.cpp"

APIServer::APIServer() {
    config = ConfigLoader::load("C:\\Users\\Admin\\Desktop\\WetvteGitClone\\alphavet_latter\\TestProcessing\\resources\\db_config.env");
    // ../resources/db_config.env не работает

    using namespace std;
    std::cout << config.get("DB_HOST") << ", " <<
        config.get("DB_PORT") << ", " <<
        config.get("DB_NAME") << ", " <<
        config.get("DB_USER") << ", " <<
        config.get("DB_PASS") << std::endl;

}

void APIServer::connect_to_db() {
    db_connector.connect(
        config.get("DB_HOST"),
        config.get("DB_PORT"),
        config.get("DB_NAME"),
        config.get("DB_USER"),
        config.get("DB_PASS")
    );
}

void APIServer::start() {
    setup_routes();
    server.listen("0.0.0.0", std::stoi(config.get("API_PORT")));
}

void APIServer::setup_routes() {
    using namespace httplib;

    server.Get("/test", [this](const Request& req, Response& res) {
        handle_test(req, res);
    });

    server.Get("/test_results", [this](const Request& req, Response& res) {
        handle_results(req, res);
    });
}

// Обработчик /test
void APIServer::handle_test(const httplib::Request& req, httplib::Response& res) {
     // БД в PostgreSQL заполнена вопросами, их получаем
    PGresult* result = db_connector.query("SELECT * FROM letter_quetions");
    
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        res.set_content("Не получен результат, ёпт", "text/plain"); // MIME‑тип явно говорит: это текст
        PQclear(result); // Освобождает, чтоб память была, а не не была
        return;
    }

    // Тестовый вывод вопросов для начала
    std::string response = "<h1>Available Tests</h1><ul>";
    for (int i = 0; i < PQntuples(result); ++i) {
        response += "<li>" + std::string(PQgetvalue(result, i, 1)) + "</li>";
    }
    response += "</ul>";

    res.set_content(response, "text/html"); // говорит, что это код html
    PQclear(result); // Освобождает, чтоб память была, а не не была
}

void APIServer::handle_results(const httplib::Request& req, httplib::Response& res) {
    // на страницу результатов должна передаваться переменная очков
    auto points_it = req.params.find("points");
    // Если не передана, то сворачиваемся
    if (points_it == req.params.end()) {
        res.set_content("Не указан результат по очкам", "text/plain");
        return;
    }

    // Тестовый результат
    int points = std::stoi(points_it->second);
    res.set_content(
        "<h1>Test Results</h1>"
        "<p>Points: " + std::to_string(points) + "</p>"
        "<p>Status: " + ((points >= 70) ? "Молодец" : "Не молодец") + "</p>",
        "text/html"
    );
}

// А ещё на сайте кириллица не поддерживается чото

#endif