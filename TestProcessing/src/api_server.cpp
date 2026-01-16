#ifndef API_SERVER
#define API_SERVER

#include "../include/api_server.h"
#include <iostream>
#include <csignal>
#include <algorithm>

APIServer::APIServer() {
    config =
        ConfigLoader::load("C:\\Users\\Admin\\Desktop\\WetvteGitClone\\alphavet_latter\\TestProcessing\\config\\test_config.env");
    // ../resources/db_config.env не работает

    std::cout<< "Получены данные сервера из окружения: " << config.get("DB_HOST") << ", " <<
        config.get("DB_PORT") << ", " <<
        config.get("DB_NAME") << ", " <<
        config.get("DB_USER") << ", " <<
        config.get("DB_PASS") << std::endl;
}

void APIServer::connect_to_db() {
    dbConnector.connect(
        config.get("DB_HOST"),
        config.get("DB_PORT"),
        config.get("DB_NAME"),
        config.get("DB_USER"),
        config.get("DB_PASS")
    );
}

// Старт сервера
APIServer* APIServer::instance = nullptr;
void APIServer::start() {
    // Помечаем статический указатель на единственный экземпляр
    instance = this;
    std::cout << "API Server starting!" << std::endl;

    // Устанавливаем обработчики
    setup_routes();
    std::cout << "API Server routes setuped!" << std::endl;

    // Регистрируем обработчик сигнала Ctrl+C (^C) для завершения работы сервера
    std::signal(SIGINT, [](int signal) {
        if (signal == SIGINT) {
            std::cout << "\n------------------------\nПолучен запрос на остановку сервера...\n------------------------\n\n";
            // Устанавливаем флаг
            instance->shutdown();
        }
    });
    std::cout << "API Server registred shutdown signal!" << std::endl;

    // Получаем порт и хост
    std::string host = config.get("API_HOST");
    std::string port = config.get("API_PORT");

    // Запускаем сервер
    std::cout << "API Server " << host << " listening port " << port << "..." << std::endl;
    
    bool started = server.listen(host, std::stoi(port));

    if (started) {
        // Ждём сигнала остановки
        while (!shutdowned) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    else {
        std::cout << "API Server did not started! End." << std::endl;
        return;
    }
}
// Остановка сервера
void APIServer::shutdown() {
    std::cout << "API Server shutdowning!" << std::endl;

    dbConnector.disconnect();
    std::cout << "Disconnected from DB!" << std::endl;

    server.stop();
    std::cout << "API Server stopped!" << std::endl;

    shutdowned = true;
}
// Токены
Token APIServer::get_request_token(const httplib::Request &req) {
    std::cout << "Получаем токен с запроса" << std::endl;
    auto header_it = req.headers.find("Authorization");
    if (header_it == req.headers.end()) {
        std::cout << "Заголовок не найден" << std::endl;
        return Token{}; // заголовок не найден
    }
    std::string auth_header = header_it->second;
    return get_header_token(auth_header);
}
Token APIServer::get_header_token(const std::string &auth_header) {
    std::cout << "Получаем токен с заголовка" << std::endl;
    // Проверяем префикс "Bearer"
    const std::string prefix = "Bearer";
    if (auth_header.size() < prefix.size() + 1) {
        std::cout << "Заголовок слишком короток" << std::endl;
        return Token{}; // неверный формат
    }

    // Извлекаем токен (после префикса)
    std::string token_string = auth_header.substr(prefix.size() + 1);
    std::cout << ">---------- Получаем токен из заголовка ----------<" << std::endl;
    std::cout << "Из заголовка получен токен: " << token_string << ". Декодируем." << std::endl;
    return TokenDecoder::decode(token_string, config.get("OWN_TOKEN_GENERATE_KEY"));
}
// Тело и параметры запроса
nlohmann::json APIServer::get_body_json(const std::string &body) {
    nlohmann::json json;
    try {
        json = nlohmann::json::parse(body);
    } catch (const nlohmann::json::exception&) {}
    return json;
}
nlohmann::json APIServer::get_query_json(const std::string& query) {
    nlohmann::json json;

    // Декодируем сначала в норм строку
    std::string params;
    for (size_t i = 0; i < query.length(); ++i) {
        if (query[i] == '%') {
            if (i + 2 < query.length()) {
                // Читаем 2 символа после '%'
                std::string hex = query.substr(i + 1, 2);
                // Преобразуем шестнадцатеричное число в байт
                char decodedChar = static_cast<char>(std::stoi(hex, nullptr, 16));
                params += decodedChar;
                i += 2; // Пропускаем обработанные символы
            } else {
                // Некорректная последовательность — оставляем как есть
                params += query[i];
            }
        } else if (query[i] == '+') {
            // В form-urlencoded '+' означает пробел
            params += ' ';
        } else {
            params += query[i];
        }
    }

    // Делаем, делаем (парсим в жсон)
    std::stringstream ss(params);
    std::string pair;
    while (std::getline(ss, pair, '&')) {
        size_t pos = pair.find('=');
        if (pos != std::string::npos) {
            std::string key = pair.substr(0, pos);
            std::string value = pair.substr(pos + 1);
            json[key] = value;
        } else {
            // Если нет =, считаем, что значение пустое
            json[pair] = "";
        }
    }

    return json;
}
// У современных библиотек httplib query не нужен, есть params
nlohmann::json APIServer::get_params_json(const httplib::Params& params) {
    nlohmann::json result;
    for (const auto& [key, value] : params) {
        result[key] = value;
    }
    std::cout << "Получены значение из ссылки:" << result.dump() << std::endl;
    return result;
}
// Ответ запросу
/* Памятка
switch (status) {
            case 400: des = " (некорректный JSON)."; break;
            case 401: des = " (Недействительный токен)."; break;
            case 403: des = " (Отсутствие прав на действие)."; break;
            case 404: des = " (Команда не распознана или запись не найдена)."; break;
            case 406: des = " (Провалено обновление базы данных)."; break;
            case 409: des = " (Нарушен уникальный ключ)."; break;
            case 503: des = " (База данных недоступна)."; break;
        }
*/
// Методы
void APIServer::write_response(httplib::Response &res, const std::string &message, const int &status) {
    nlohmann::json json;
    write_response(res, json, message, status);
}
void APIServer::write_response(httplib::Response &res, nlohmann::json json, const std::string &message, const int &status) {
    res.set_header("Content-Type", "application/json");
    res.status = status;
    json["message"] = message;
    try {
        std::string json_dump = json.dump();
        std::cout << "Возвращаем json: " << json_dump << std::endl;
        res.set_content(json_dump, "application/json");
        std::cout << "--------- Запрос завершён! Итог - " << res.status << "---------" << std::endl;
    }
    catch (const std::exception& e) {
        std::cout << e.what() << std::endl;
        res.status = 501;
    }
}
// Установка обработчиков
void APIServer::setup_routes() {
    /*
        В get запросах, содержащих в адресе data, проверяется содержимое тела на наличие параметра filter и его значения.
        Если он равен * или all, обработка для всех доступных данных.
        Если хоть один параметр фильтра не может быть доступен из-за разрешения, статус 403 

        Роль Admin может всё ^_^ (почти)
    */

    /* Обратная созависимость таблиц:
        У пользователя нет посторонних данных
        У дисциплин есть данные о студентах и преподавателях, а также о тестах
        У тестов есть данные о вопросах, которые они содержат, о попытках их прохождения (ещё об авторе)
        У вопросов нет посторонних ссылок
        У попыток есть данные о содержащихся вопросах, авторе с тестом, на который ссылкаются и ответах
        У ответов только данные о вопросе, на который они ссылаются
    */

    /* Методы и права в них:
        Post - /users/note - user:note
        Get - /users/list - users:list
        Get - /users/data - user:roles:read, user:disciplines:read, user:tries:read
        Post - /users/fullname - user:fullname:write
        Post - /users/roles - user:roles:write
        Post - /users/nickname - user:fullname:write
        Post - /users/status - user:status:write, user:block

        Get - /disciplines/list
        Get - /disciplines/data - discipline:teacherslist, discipline:testslist, discipline:studentslist
        Post - /disciplines/create - discipline:create
        Post - /disciplines/delete - discipline:delete
        Post - /disciplines/status - discipline:delete
        Post - /disciplines/text - discipline:text:write
        Post - /disciplines/tests/add - test:create, discipline:test:add
        Post - /disciplines/users/add - discipline:student:add, discipline:teacher:add
        Post - /disciplines/users/remove - discipline:user:remove

        Get - /tests/data - test:tries:read
        Post - /tests/text - test:text:write
        Post - /tests/status - test:status:write
        Post - /tests/questions/add - test:question:add
        Post - /tests/questions/remove - test:question:remove
        Post - /tests/questions/update - test:question:update
        Get - /tests/tries/user - test:tries:read

        Get - /questions - questions:list
        Get - /questions/data - question:read
        Post - /questions/create - question:create
        Post - /questions/update - test:question:update
        Post - /questions/status, question:delete

        Post - /tries/start
        Post - tries/answer/change
        Post - /tries/answer/delete
        Post - /tries/stop
        Get - /tries/view - test:tries:read
        
        Post - /news/send - news:write
        Get - /news/list/recipient
        Post - /news/status - news:write
    */

    server.Get("/", [](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            "<html>"
            "<head><title>Проверка сервера</title></head>"
            "<body>"
            "<h1>Сервер работает!</h1>"
            "</body>"
            "</html>",
            "text/html"
        );
    });
    
    // Пользователи
    // Записывает пользователя
    server.Post("/users/note", [this](const httplib::Request& req, httplib::Response& res) {
        // Получение токена
        Token token = get_request_token(req);
        // Проверка токена и разрешений
        if (!token.is_valid()) {
            write_response(res, "Токен недействителен.", 401);
            return;
        }
        if (!token.has_permission("users:note")) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }
        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        std::cout << "--- " << body_json.dump() << std::endl;
        DBRow settings;
        settings.set("id", body_json["id"].get<std::string>());
        settings.set("email", body_json["email"].get<std::string>());
        settings.set("first_name", body_json["first_name"].get<std::string>());
        settings.set("last_name", body_json["last_name"].get<std::string>());
        settings.set("patronimyc", body_json["patronimyc"].get<std::string>());
        settings.set("nickname", body_json["nickname"].get<std::string>());
        settings.set("roles", dbConnector.convert_to_string_format(body_json["roles"].get<std::vector<std::string>>()));
        DBRow result_row = dbConnector.insert_row("users", settings);
        if (result_row.is_empty())
            write_response(res, "Ошибка записи пользователя.", 500);
        else 
            write_response(res, "Пользователь успешно добавлен.", 200);
    });
    // Возвращает массив содержащий ФИО и ID каждого пользователя зарегистрированного в системе
    server.Get("/users/list", [this](const httplib::Request& req, httplib::Response& res) {
        // Получение токена
        Token token = get_request_token(req);
        // Проверка токена и разрешений
        if (!token.is_valid()) {
            write_response(res, "Токен недействителен.", 401);
            return;
        }
        if (!token.has_permission("users:list")) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }
        
        // Получает список
        std::vector<DBRow> list = dbConnector.get_rows("users", "id, first_name, last_name, patronimyc", "");
        // Формирует json для списка
        nlohmann::json json;
        json["users"] = nlohmann::json::array();
        for (DBRow& user_row : list) {
            // Формирует json для отдельной строки
            nlohmann::json row_json;
            std::string first = user_row.get<std::string>("first_name");
            std::string last = user_row.get<std::string>("last_name");
            std::string patronimyc = user_row.get<std::string>("patronimyc");
            std::string fullname = build_fullname(first, last, patronimyc);
            row_json["user_id"] = user_row.get<std::string>("id");
            row_json["first_name"] = first;
            row_json["last_name"] = last;
            row_json["patronimyc"] = patronimyc;
            row_json["fullname"] = fullname;
            json["users"].push_back(row_json);
        }
        // Отправляет
        write_response(res, json, "Список пользователей успешно получен.");
    });
    // Возвращает информацию пользователя по его ID. Возвращается только та информация которую запросили из следующей: список дисциплин, список // прохождений тестов (попыток))
    server.Get("/users/data", [this](const httplib::Request& req, httplib::Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен недействителен.", 401);
            return;
        }
        // Получение параметров, проверка
        nlohmann::json params_json = get_params_json(req.params);
        if (!params_json.contains("user_id") || !params_json.contains("filter")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }
        std::string filter = params_json["filter"].get<std::string>();
        if (filter == "*" || filter == "all") filter = "nickname fullname status roles email disciplines tests tries";
        
        // Айди пользователя
        std::string user_id = params_json["user_id"].get<std::string>();

        // Получает пользователя из БД
        DBRow user_row = dbConnector.get_row("users", "*", "id='"+user_id+"'");
        std::cout << "Got user: " << user_row << std::endl;

        if (user_row.is_empty()) {
            write_response(res, "Пользователь не найден.", 404);
            return;
        }

        // Проверяет наличие разрешений для всех выбранных фильтров, требующих это
        // Роли
        if (filter.find("roles") != std::string::npos) {
            // Само разрешение
            if (!token.has_permission("user:roles:read"))
            /* Изначально Свои и Чужие нельзя */
            // но Я сделал так, что можно, и, если админ, то ну любые то же же да
            { write_response(res, "Недостаточно прав.", 403); return; }
        }
        // Дисциплины
        if (filter.find("disciplines") != std::string::npos) {
            // Само разрешение
            if (!token.has_permission("user:disciplines:read") ||
            // Можно смотреть дисциалины только себе и админу
            (user_id != token.get_user() && !token.has_role("Admin"))) { write_response(res, "Недостаточно прав.", 403); return; }
        }
        // Попытки прохождения тестов
        if (filter.find("tries") != std::string::npos || filter.find("tests") != std::string::npos) {
            // Само разрешение
            if (!token.has_permission("user:tries:read") ||
            // Можно смотреть дисциалины только себе и админу
            (user_id != token.get_user() && !token.has_role("Admin"))) { write_response(res, "Недостаточно прав.", 403); return; }
        }

        // Собирает json
        nlohmann::json json;
        if (filter.find("nickname") != std::string::npos) {
            json["nickname"] = user_row.get<std::string>("nickname");
        }
        if (filter.find("fullname") != std::string::npos) {
            // Формируем ФИО
            std::string fullname;
            if (user_row.is_empty())
                fullname = "Неизвестно";
            else {
                std::string first = user_row.get<std::string>("first_name");
                std::string last = user_row.get<std::string>("last_name");
                std::string patronimyc = user_row.get<std::string>("patronimyc");
                fullname = build_fullname(first, last, patronimyc);
                json["first_name"] = first;
                json["last_name"] = last;
                json["patronimyc"] = patronimyc;
            }
            // Добавляет в json
            json["fullname"] = fullname;
        }
        if (filter.find("status") != std::string::npos) {
            json["status"] = user_row.get<std::string>("status");
        }
        if (filter.find("roles") != std::string::npos) {
            std::vector<std::string> roles = user_row.get<std::vector<std::string>>("roles");
            json["roles"] = roles;
        }
        if (filter.find("email") != std::string::npos) {
            json["email"] = user_row.get<std::string>("email");
        }
        if (filter.find("disciplines") != std::string::npos) {
            json["disciplines"] = nlohmann::json::array();
            // Получаем строки данных о неудалённых дисциплинах с пользователем
            std::vector<DBRow> disciplines_rows =
                dbConnector.get_rows("disciplines", "id, title, status, teachers",
                "'"+user_id+"'=ANY(teachers) OR "+"'"+user_id+"'=ANY(students)");
            for (DBRow& discipline_row : disciplines_rows) {
                nlohmann::json row_json;
                // Получаем статус и скипаем, если удалена, а автор запроса - не админ
                std::string status = discipline_row.get<std::string>("status");
                if (status == "Deleted" && !token.has_role("Admin")) continue;
                // Сохраняем в ответ заголовок и статус дисциплины, а также преподавателей.
                row_json["discipline_id"] = discipline_row.get<int>("id");
                row_json["title"] = discipline_row.get<std::string>("title");
                row_json["status"] = status;
                row_json["teachers"] = discipline_row.get<std::vector<std::string>>("teachers");
                json["disciplines"].push_back(row_json);
            }
        }
        if (filter.find("tests") != std::string::npos) {
            json["tests"] = nlohmann::json::array();
            // Получаем данные о попытках
            std::vector<DBRow> tries_rows = dbConnector.get_rows("tries", "id", "author='"+user_id+"'");
            std::vector<int> tries_ids;
            for (DBRow& try_row : tries_rows)
                tries_ids.push_back(try_row.get<int>("id"));
            // Получаем строки данных о тестах
            std::vector<DBRow> tests_rows = dbConnector.get_rows("tests", "*", "tries && ARRAY["+build_range(tries_ids)+"]");
            for (DBRow& test_row : tests_rows) {
                int test_id = test_row.get<int>("id");
                // Получает дисциплину, соответствующую тесту
                DBRow discipline_row = dbConnector.get_row("disciplines", "id, title, status", std::to_string(test_id)+"=ANY(tests)");
                int discipline_id = discipline_row.get<int>("id");
                nlohmann::json row_json;
                // Получаем статусы и скипаем, если дисциплина удалена или тест удалён, а автор запроса - не админ
                std::string test_status = test_row.get<std::string>("status");
                std::string discipline_status = discipline_row.get<std::string>("status");
                if ((test_status == "Deleted" || discipline_status == "Deleted") && !token.has_role("Admin")) continue;
                // Записываем в ответ данные о тесте и его дисциплине
                row_json["test_id"] = test_id;
                row_json["test_title"] = test_row.get<std::string>("title");
                row_json["test_status"] = test_status;
                row_json["test_discipline_id"] = discipline_id;
                row_json["test_discipline_title"] = discipline_row.get<std::string>("title");
                row_json["test_discipline_status"] = discipline_status;
                json["tests"].push_back(row_json);
            }
        }
        if (filter.find("tries") != std::string::npos) {
            json["tries"] = nlohmann::json::array();
            // Получаем строки данных о попытках
            std::vector<DBRow> tries_rows = dbConnector.get_rows("tries", "*", "author='"+user_id+"'");
            for (DBRow& try_row : tries_rows) {
                int try_id = try_row.get<int>("id");
                // Получает тест, соответствующий попытке
                DBRow test_row = dbConnector.get_row("tests", "id, title, status", "id="+try_row.get<std::string>("test"));
                int test_id = test_row.get<int>("id");
                // Получает дисциплину, соответствующую тесту
                DBRow discipline_row = dbConnector.get_row("disciplines", "id, title, status", std::to_string(test_id)+"=ANY(tests)");
                int discipline_id = discipline_row.get<int>("id");
                nlohmann::json row_json;
                // Получаем статусы и скипаем, если дисциплина удалена или тест удалён, а автор запроса - не админ
                std::string test_status = test_row.get<std::string>("status");
                std::string discipline_status = discipline_row.get<std::string>("status");
                if ((test_status == "Deleted" || discipline_status == "Deleted") && !token.has_role("Admin")) continue;
                // Записываем в ответ данные о тесте и его дисциплине
                row_json["try_id"] = try_id;
                row_json["test_id"] = test_id;
                row_json["test_title"] = test_row.get<std::string>("title");
                row_json["test_status"] = test_status;
                row_json["test_discipline_id"] = discipline_id;
                row_json["test_discipline_title"] = discipline_row.get<std::string>("title");
                row_json["test_discipline_status"] = discipline_status;
                // статус и результат самой попытки
                row_json["status"] = try_row.get<std::string>("status");
                row_json["points"] = try_row.get<int>("points");
                row_json["max_points"] = try_row.get<int>("max_points");
                row_json["score_percent"] = try_row.get<int>("score_percent");
                json["tries"].push_back(row_json);
            }
        }
        // Отправляет ответ
        write_response(res, json, "Данные о пользователе успешно получены.");
    });
    // Заменяет ФИО пользователя на указанное по его ID
    server.Post("/users/fullname", [this](const httplib::Request& req, httplib::Response& res) {
        Token token = get_request_token(req);
        // Проверка токена и разрешений
        if (!token.is_valid()) {
            write_response(res, "Токен недействителен.", 401);
            return;
        }
        if (!token.has_permission("user:fullname:write")) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("user_id") ||
            !body_json.contains("first_name") || !body_json.contains("last_name") || !body_json.contains("patronimyc")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }
        
        // Получаем айди пользователя
        std::string user_id = body_json["user_id"].get<std::string>();
        // Менеть можно только себе
        if (user_id != token.get_user()) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }

        // Получаем данные
        std::string first_name = body_json["first_name"].get<std::string>();
        std::string last_name = body_json["last_name"].get<std::string>();
        std::string patronimyc = body_json["patronimyc"].get<std::string>();
        
        // Отправляем изменение модулю авторизации
        nlohmann::json send_json;
        send_json["user_id"] = user_id;
        send_json["first_name"] = first_name;
        send_json["last_name"] = last_name;
        send_json["patronimyc"] = patronimyc;
        std::optional<HttpResponse> response = DoPostRequestToService(
            config.get("AUTHORIZATION_MODULE_HOST"), std::stoi(config.get("AUTHORIZATION_MODULE_PORT")), "users/data/change",
                "Bearer", token.get_as_string(), send_json);
        // Обработка неудачного запроса
        if (response == std::nullopt) {
            write_response(res, "Ошибка отправки запроса.", 500);
            return;
        }
        if (response->status != 200) {
            write_response(res, response->body["message"], response->status);
            return;
        }
        
        // Если получилось, вставляем в БД
        DBRow settings;
        settings.set("first_name", first_name);
        settings.set("last_name", last_name);
        settings.set("patronimyc", patronimyc); 
        int status = dbConnector.update_rows("users", settings, "id='"+user_id+"'");
        if (status == 200) write_response(res, "Успешное обновление ФИО.");
        else write_response(res, "Ошибка при обновлении базы данных.", status);
    });
    // Заменяет роли пользователя на указанные по его ID
    server.Post("/users/roles", [this](const httplib::Request& req, httplib::Response& res) {
        Token token = get_request_token(req);
        // Проверка токена и разрешений
        if (!token.is_valid()) {
            write_response(res, "Токен недействителен.", 401);
            return;
        }
        if (!token.has_permission("user:roles:write")) { // Право только у админа
            write_response(res, "Недостаточно прав.", 403);
            return;
        }
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("user_id") || !body_json.contains("roles")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }
        
        // Получаем айди пользователя
        std::string user_id = body_json["user_id"].get<std::string>();
        // Вектор для ролей
        std::vector<std::string> roles = body_json["roles"].get<std::vector<std::string>>();
        
        // Проверка существования ролей
        for (std::string& role : roles) if (role != "Admin" && role != "Student" && role != "Teacher") {
            write_response(res, "Одна или более из не существует.", 400);
            return;
        }

        // Отправляем изменение модулю авторизации
        nlohmann::json send_json;
        send_json["user_id"] = user_id;
        send_json["roles"] = roles;
        std::optional<HttpResponse> response = DoPostRequestToService(
            config.get("AUTHORIZATION_MODULE_HOST"), std::stoi(config.get("AUTHORIZATION_MODULE_PORT")), "users/data/change",
                "Bearer", token.get_as_string(), send_json);
        // Обработка неудачного запроса
        if (response == std::nullopt) {
            write_response(res, "Ошибка отправки запроса.", 500);
            return;
        }
        if (response->status != 200) {
            write_response(res, response->body["message"], response->status);
            return;
        }
        
        // Делаем строку изменений
        DBRow settings;
        settings.set("roles", dbConnector.convert_to_string_format(roles));
        // Обновляем
        int status = dbConnector.update_rows("users", settings, "id='"+user_id+"'");
        if (status == 200) write_response(res, "Успешное обновление ролей.");
        else write_response(res, "Ошибка при обновлении базы данных.", status);
    });
    server.Post("/users/nickname", [this](const httplib::Request& req, httplib::Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен недействителен.", 401);
            return;
        }
        // Получение тела проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("user_id") || !body_json.contains("nickname")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }
        // Проверка разрешений
        if (!token.has_permission("user:fullname:write")) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }

        // Получаем пользователя
        std::string user_id = body_json["user_id"].get<std::string>();
        DBRow user_row = dbConnector.get_row("users", "id", "id='"+user_id+"'");
        if (user_row.is_empty()) {
            write_response(res, "Пользователь не найден.", 404);
            return;
        }

        // Нельзя, если не себе
        if (user_id != token.get_user()) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }

        // Получаем данные
        std::string nickname = body_json["nickname"].get<std::string>();
        // Отправляем изменение модулю авторизации
        nlohmann::json send_json;
        send_json["user_id"] = user_id;
        send_json["nickname"] = nickname;
        std::optional<HttpResponse> response = DoPostRequestToService(
            config.get("AUTHORIZATION_MODULE_HOST"), std::stoi(config.get("AUTHORIZATION_MODULE_PORT")), "users/data/change",
                "Bearer", token.get_as_string(), send_json);
        // Обработка неудачного запроса
        if (response == std::nullopt) {
            write_response(res, "Ошибка отправки запроса.", 500);
            return;
        }
        if (response->status != 200) {
            write_response(res, response->body["message"], response->status);
            return;
        }
        
        // Делаем строку изменений
        DBRow settings;
        settings.set("nickname", nickname);
        // Обновляем
        int status = dbConnector.update_rows("users", settings, "id='"+user_id+"'");
        if (status == 200) write_response(res, "Успешное обновление никнейма.");
        else write_response(res, "Ошибка при обновлении базы данных.", status);
    });
    server.Post("/users/status", [this](const httplib::Request& req, httplib::Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен недействителен.", 401);
            return;
        }
        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("user_id") || !body_json.contains("status")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }
        // Проверка разрешений
        if (!token.has_permission("user:status:write")) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }

        // Проверка существования статуса
        std::string target_status = body_json["status"].get<std::string>();
        if (target_status != "Online" && target_status != "Offline" && target_status != "Blocked") {
            write_response(res, "Несуществующий статус.", 400);
            return;
        }

        // Получаем пользователя
        std::string user_id = body_json["user_id"].get<std::string>();
        DBRow user_row = dbConnector.get_row("users", "status", "id='"+user_id+"'");
        if (user_row.is_empty()) {
            write_response(res, "Пользователь не найден.", 404);
            return;
        }

        // Получаем текущий стаутс
        std::string current_status = user_row.get<std::string>("status");

         // Не может ставить блокировку, но пытается
        if (!token.has_permission("user:block") && target_status == "Blocked") {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }

        // Нельзя, если не админ и не себе или себе, но заблокирован
        if (!token.has_role("Admin") && (user_id != token.get_user() || current_status == "Blocked")) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }

        // Если блокируем пользователя
        if (target_status == "Blocked") {
            // Отправляем запрос на выход модулю авторизации
            nlohmann::json send_json;
            send_json["user_id"] = user_id;
            std::optional<HttpResponse> response = DoPostRequestToService(
            config.get("AUTHORIZATION_MODULE_HOST"), std::stoi(config.get("AUTHORIZATION_MODULE_PORT")), "logout/user",
                "Bearer", token.get_as_string(), send_json);
            // Обработка неудачного запроса не нужна, но желательно бы ему быть удачным
        }

        // Делаем строку изменений
        DBRow settings;
        settings.set("status", target_status);
        // Обновляем
        int status = dbConnector.update_rows("users", settings, "id='"+user_id+"'");
        if (status == 200) write_response(res, "Успешное обновление статуса.");
        else write_response(res, "Ошибка при обновлении базы данных.", status);
    });
    // Дисциплины
    // Возвращает массив содержащий Название, Описание и ID каждой дисциплины зарегистрированной в системе
    server.Get("/disciplines/list", [this](const httplib::Request& req, httplib::Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен недействителен.", 401);
            return;
        }

        // Получает список
        std::vector<DBRow> list = dbConnector.get_rows("disciplines", "id, title, status", "", "BY id ASC");
        // Формирует json для списка
        nlohmann::json json;
        json["disciplines"] = nlohmann::json::array();
        for (DBRow& row : list) {
            // Проверяет статус и пропускает, если удалена (но админу можно)
            std::string status = row.get<std::string>("status");
            if (status == "Deleted" && !token.has_role("Admin")) continue;
            // Формирует json для отдельной строки
            nlohmann::json row_json;
            row_json["discipline_id"] = row.get<int>("id");
            row_json["title"] = row.get<std::string>("title");
            row_json["status"] = status;
            json["disciplines"].push_back(row_json);
        }
        // Отправляет
        write_response(res, json, "Успешное получение списка дисциплин.");
    });
    // Для фильтра * возвращает Название, Описание, ID преподавателя для дисциплины по её ID
    server.Get("/disciplines/data", [this](const httplib::Request& req, httplib::Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен недействителен.", 401);
            return;
        }
        // Получение тела, проверка
        nlohmann::json params_json = get_params_json(req.params);
        if (!params_json.contains("discipline_id") || !params_json.contains("filter")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }
        std::string filter = params_json["filter"].get<std::string>();
        if (filter == "*" || filter == "all") filter = "text status teacherslist testslist studentslist";

        // Получение данных о дисциплине
        int discipline_id = std::stoi(params_json["discipline_id"].get<std::string>());
        DBRow discipline_row = dbConnector.get_row("disciplines", "*", "id="+std::to_string(discipline_id));

        // Получаем статус дисциплины: если удалена, не возвращаем, если не админ
        std::string status = discipline_row.get<std::string>("status");
        if (status == "Deleted" && !token.has_role("Admin")) {
            write_response(res, "Дисциплина недоступна.", 403);
            return;
        }

        std::vector<std::string> students_ids = discipline_row.get<std::vector<std::string>>("students");
        std::vector<std::string> teachers_ids = discipline_row.get<std::vector<std::string>>("teachers");
        // Проверяет наличие разрешений для всех выбранных фильтров, требующих это
        // Списки // Админу автоматически можно
        if (filter.find("list") != std::string::npos && !token.has_role("Admin")) {
            // Если нужны списки преподавателей или тестов, но нет разрешений - кик нхй
            if ((filter.find("teacherslist") != std::string::npos && !token.has_permission("discipline:teacherslist")) ||
                (filter.find("testslist") != std::string::npos && !token.has_permission("discipline:testslist")))
                    { write_response(res, "Недостаточно прав.", 403); return; }
            // Узнаём, среди студентов ли пользователь
            bool among_students = contains(students_ids, token.get_user());
            // Узнаём, среди преподавателей ли пользователь
            bool among_teachers = contains(teachers_ids, token.get_user());
            // Если нужен список студентов, 
            if (filter.find("studentslist") != std::string::npos) {
                // но у юзера нет разрешений, или он не преподаватель - кик нхй
                if (!token.has_permission("discipline:studentslist") || !among_teachers)
                    { write_response(res, "Недостаточно прав.", 403); return; }
            }
            // Если нужны списки преподавателей или тестов, но юзер ваще не в дисциплине
            else if (!among_teachers && !among_students)
                { write_response(res, "Недостаточно прав.", 403); return; }
        }

        // Формируем json
        nlohmann::json json;
        if (filter.find("text") != std::string::npos) {
            json["title"] = discipline_row.get<std::string>("title");
            json["describtion"] = discipline_row.get<std::string>("describtion");
        }
        if (filter.find("status") != std::string::npos) {
            json["status"] = status;
        }
        if (filter.find("teacherslist") != std::string::npos) {
            json["teachers"] = nlohmann::json::array();
            if (teachers_ids.size() > 0) {
                std::vector<DBRow> teachers_rows =
                    dbConnector.get_rows("users", "id, first_name, last_name, patronimyc", "id IN ("+build_range(teachers_ids)+")");
                for (DBRow& teacher_row : teachers_rows) {
                    nlohmann::json row_json;
                    // Формируем ФИО
                    std::string fullname;
                    if (teacher_row.is_empty())
                        fullname = "Неизвестно";
                    else {
                        std::string first = teacher_row.get<std::string>("first_name");
                        std::string last = teacher_row.get<std::string>("last_name");
                        std::string patronimyc = teacher_row.get<std::string>("patronimyc");
                        fullname = build_fullname(first, last, patronimyc);
                    }
                    // Добавляет в json
                    row_json["user_id"] = teacher_row.get<std::string>("id");
                    row_json["fullname"] = fullname;
                    json["teachers"].push_back(row_json);
                }
            }
        }
        if (filter.find("testslist") != std::string::npos) {
            json["tests"] = nlohmann::json::array();
            std::vector<int> tests_ids = discipline_row.get<std::vector<int>>("tests");
            if (tests_ids.size() > 0) {
                std::vector<DBRow> tests_rows = dbConnector.get_rows("tests", "id, title, status", "id IN ("+build_range(tests_ids)+")");
                for (DBRow& test_row : tests_rows) {
                    nlohmann::json row_json;
                    // Получает статус и скипает, если удалён (Админу можно)
                    std::string status = test_row.get<std::string>("status");
                    if (status == "Deleted" && !token.has_role("Admin")) continue;
                    // Формирует строку дальше
                    row_json["test_id"] = test_row.get<std::string>("id");
                    row_json["title"] = test_row.get<std::string>("title");
                    row_json["status"] = status;
                    json["tests"].push_back(row_json);
                }
            }
        }
        if (filter.find("studentslist") != std::string::npos) {
            json["students"] = nlohmann::json::array();
            if (students_ids.size() > 0) {
                std::vector<DBRow> students_rows =
                    dbConnector.get_rows("users", "id, first_name, last_name, patronimyc", "id IN ("+build_range(students_ids)+")");
                for (DBRow& student_row : students_rows) {
                    nlohmann::json row_json;
                    // Формируем ФИО
                    std::string fullname;
                    if (student_row.is_empty())
                        fullname = "Неизвестно";
                    else {
                        std::string first = student_row.get<std::string>("first_name");
                        std::string last = student_row.get<std::string>("last_name");
                        std::string patronimyc = student_row.get<std::string>("patronimyc");
                        fullname = build_fullname(first, last, patronimyc);
                    }
                    // Добавляет в json
                    row_json["user_id"] = student_row.get<std::string>("id");
                    row_json["fullname"] = fullname;
                    json["students"].push_back(row_json);
                }
            }
        }
        // Отправляем
        write_response(res, json, "Успешное получение данных о дисциплине.");
    });
    // Создаёт дисциплину с указанным названием, описанием и преподавателем. Как результат возвращает её ID
    server.Post("/disciplines/create", [this](const httplib::Request& req, httplib::Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен недействителен.", 401);
            return;
        }
        if (!token.has_permission("discipline:create")) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }
        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("title") || !body_json.contains("teacher_id")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }

        // Проверяет на наличие роли преподавателя у того, кто ставится в его качестве
        std::string teacher_id = body_json["teacher_id"].get<std::string>();
        DBRow teacher_row = dbConnector.get_row("users", "roles", "id='"+teacher_id+"'");
        std::vector<std::string> roles = teacher_row.get<std::vector<std::string>>("roles");
        bool is_teacher = contains(roles, "Teacher");
        if (!is_teacher) {
            write_response(res, "Недостаточно прав, так как не преподаватель.", 403);
            return;
        }

        // Формирует строку для вставки
        DBRow settings;
        settings.set("title", body_json["title"].get<std::string>());
        settings.set("describtion", body_json.contains("describtion") ? body_json["describtion"].get<std::string>() : "Ниже можно посмотреть данные дисциплины.");
        settings.set("teachers", "{"+teacher_id+"}");
        DBRow result = dbConnector.insert_row("disciplines", settings);
        if (result.is_empty()) {
            write_response(res, "Ошибка создания дисциплины.", 406);
            return;
        }
        // Возвращает айди
        nlohmann::json json;
        json["discipline_id"] = result.get<int>("id");
        write_response(res, json, "Успешное создание дисциплины.");
    });
    // Отмечает дисциплину как удалённую (реально ничего не удаляется). Все тесты и оценки перестают отображаться, но тоже не удаляются.
    server.Post("/disciplines/status", [this](const httplib::Request& req, httplib::Response& res) {
       Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен недействителен.", 401);
            return;
        }
        if (!token.has_permission("discipline:delete")) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }
        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("discipline_id") || !body_json.contains("status")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }

        // Получаем ин-фу удаляемой дисциплины
        int discipline_id = std::stoi(body_json["discipline_id"].get<std::string>());
        DBRow discipline_row = dbConnector.get_row("disciplines", "status, teachers", "id="+std::to_string(discipline_id));

        // Админу автоматически можно
        if (!token.has_role("Admin")) {
            // Нельзя, если не первый преподаватель дисциплины
            std::vector<std::string> teachers_ids = discipline_row.get<std::vector<std::string>>("teachers");
            if (teachers_ids[0] != token.get_user()) {
                write_response(res, "Некорректный запрос.", 400);
                return;
            }
        }

        // Получаем статус
        std::string target_status = body_json["status"].get<std::string>();
        // Проверка существования статуса
        if (target_status != "Exists" && target_status != "Deleted") {
            write_response(res, "Несуществующий статус дисциплины.", 400);
            return;
        }

        // Создаём строку настроек
        DBRow settings;
        settings.set("status", target_status);
        // Обновляем данные
        int status = dbConnector.update_rows("disciplines", settings, "id="+std::to_string(discipline_id));
        if (status != 200) {
            write_response(res, "Ошибка при обновлении базы данных.", status);
        }
        // Формирует json
        nlohmann::json json;
        json["title"] = discipline_row.get<std::string>("title");
        write_response(res, json, "Успешное изменение статуса дисциплины.");
    });
    // Заменяет Название и(или) Описание дисциплины на указанные по её ID
    server.Post("/disciplines/text", [this](const httplib::Request& req, httplib::Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен недействителен.", 401);
            return;
        }
        if (!token.has_permission("discipline:text:write")) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }
        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("discipline_id") || (!body_json.contains("title") && !body_json.contains("describtion"))) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }

        // Получаем айди дисциплины
        int discipline_id = std::stoi(body_json["discipline_id"].get<std::string>());
        DBRow discipline_row = dbConnector.get_row("disciplines", "title, teachers", "id="+std::to_string(discipline_id));
        // Нельзя, если не преподаватель дисциплины
        std::vector<std::string> teachers_ids = discipline_row.get<std::vector<std::string>>("teachers");
        bool among_teachers = contains(teachers_ids, token.get_user());
        if (!among_teachers) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }

        // Формируем изменения
        DBRow settings;
        if (body_json.contains("title")) settings.set("title", body_json["title"].get<std::string>());
        if (body_json.contains("describtion")) settings.set("describtion", body_json["describtion"].get<std::string>());
        // Обновляем данные
        int status = dbConnector.update_rows("disciplines", settings, "id="+std::to_string(discipline_id));
        if (status != 200) {
            write_response(res, "Ошибка при обновлении базы данных.", status);
        }
        // Формирует json
        nlohmann::json json;
        json["title"] = discipline_row.get<std::string>("title");
        write_response(res, json, "Успешное изменение информации о дисциплине.");
    });
    // Добавляет в дисциплину с ID новый тест с указанным названием, пустым списком вопросов и автором и возвращает ID теста. По умолчанию тест не активен.
    server.Post("/disciplines/tests/add", [this](const httplib::Request& req, httplib::Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен недействителен.", 401);
            return;
        }
        if (!token.has_permission("test:create") && !token.has_permission("discipline:test:add")) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }
        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("discipline_id") || (!body_json.contains("title") && !body_json.contains("test_id"))) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }

        // Получаем айди дисциплины
        int discipline_id = dbConnector.get_as_int(body_json["discipline_id"]);
        DBRow discipline_row = dbConnector.get_row("disciplines", "teachers, tests", "id="+std::to_string(discipline_id));
        // Нельзя, если не преподаватель дисциплины
        std::vector<std::string> teachers_ids = discipline_row.get<std::vector<std::string>>("teachers");
        bool among_teachers = contains(teachers_ids, token.get_user());
        if (!among_teachers) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }

        // Получаем айди теста. Если добавляем новый, то создаём
        int test_id;
        DBRow test_row;
        if (!body_json.contains("test_id")) {
            // Создаём новый тест
            DBRow test_settings;
            test_settings.set("title", body_json["title"].get<std::string>());
            test_settings.set("describtion", body_json.contains("describtion") ?
                body_json["describtion"].get<std::string>() : "Тест поможет закрепить знания о дисциплине.");
            test_settings.set("author", token.get_user());
            test_settings.set("status", "Deactivated");
            test_row = dbConnector.insert_row("tests", test_settings);
            if (test_row.is_empty()) {
                write_response(res, "Ошибка при создании теста.", 500);
                return;
            }
            test_id = test_row.get<int>("id");
        }
        else {
            test_id = std::stoi(body_json["test_id"].get<std::string>());
            test_row = dbConnector.get_row("tests", "status", "id="+std::to_string(test_id));
            if (test_row.is_empty() || test_row.get<std::string>("status") == "Deleted") {
                write_response(res, "Тест удалён или не существует.", 406);
                return;
            }
        }

        // Формируем изменения для дисциплины
        std::vector<int> tests_ids = discipline_row.get<std::vector<int>>("tests");
        tests_ids.push_back(test_id);
        DBRow discipline_settings;
        discipline_settings.set("tests", dbConnector.convert_to_string_format(tests_ids));
        // Обновляем данные
        int status = dbConnector.update_rows("disciplines", discipline_settings, "id="+std::to_string(discipline_id));
        if (status != 200) {
            write_response(res, "Ошибка при обновлении базы данных.", status);
            return;
        } 
        
        // Формируем json-ответ
        nlohmann::json json;
        json["test_id"] = test_id;
        json["title"] = test_row.get<int>("title");
        json["status"] = test_row.get<std::string>("status");
        write_response(res, json, "Успешное добавление нового теста в дисциплину.");
    });
    // Добавляет пользователя с указанным ID на дисциплину с указанным ID
    server.Post("/disciplines/users/add", [this](const httplib::Request& req, httplib::Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен недействителен.", 401);
            return;
        }
        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("discipline_id") || !body_json.contains("user_type")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }

        // Если не передан пользователь, добавляем того, кто отправил запрос
        std::string user_id = body_json.contains("user_id")  ? body_json["user_id"].get<std::string>() : token.get_user();
        // Получаем пользователя
        DBRow user_row = dbConnector.get_row("users", "roles", "id='"+user_id+"'");
        if (user_row.is_empty()) {
            write_response(res, "Пользователь не найден.", 404);
            return;
        }

        // Дисциплина
        int discipline_id = dbConnector.get_as_int(body_json["discipline_id"]);
        DBRow discipline_row = dbConnector.get_row("disciplines", "students, teachers", "id="+std::to_string(discipline_id));

        // Создаём настройки для изменения БД
        DBRow discipline_settings;
        // Получаем тип пользователя для добавления
        std::string user_type = body_json["user_type"].get<std::string>();
        // Проверка разрешений и формировка настроек для соответствующего типа вставки
        if (user_type == "student") {
            // Если нет прав (1), или добавляет не себя (1), или уже есть (2), или не студент (3), то низя
            // 1
            if (!token.has_permission("discipline:student:add") || user_id != token.get_user()) {
                write_response(res, "Недостаточно прав.", 403);
                return;
            }
            // 2
            std::vector<std::string> students_ids = discipline_row.get<std::vector<std::string>>("students");
            for (std::string& student_id : students_ids) if (student_id == user_id) {
                write_response(res, "Пользователь уже зачислен на дисциплину.", 406);
                return;
            }
            // 3
            std::vector<std::string> user_roles = user_row.get<std::vector<std::string>>("roles");
            bool is_teacher = contains(user_roles, "Student");
            if (!is_teacher) {
                write_response(res, "Пользователь не является студентом.", 406);
                return;
            }

            // Добавление в студенты
            students_ids.push_back(user_id);
            discipline_settings.set("students", dbConnector.convert_to_string_format(students_ids));
        }
        else if (user_type == "teacher") {
            // Если нет прав на добавление преподавателя (1), или владелец токена не препод дисциплины (или админ) (2),
            // или уже есть (3), или это не препод (4)
            // 1
            if (!token.has_permission("discipline:teacher:add")) {
                write_response(res, "Недостаточно прав.", 403);
                return;
            }
            // 2
            std::vector<std::string> teachers_ids = discipline_row.get<std::vector<std::string>>("teachers");
            if (!token.has_role("Admin") && !contains(teachers_ids, token.get_user())) {
                write_response(res, "Преподавателей может добавлять только преподаватель.", 403);
                return;
            }
            // 3
            for (std::string& teacher_id : teachers_ids) if (teacher_id == user_id) {
                write_response(res, "Преподаватель уже находится в дисциплине.", 406);
                return;
            }
            // 4
            std::vector<std::string> user_roles = user_row.get<std::vector<std::string>>("roles");
            bool is_teacher = contains(user_roles, "Teacher");
            if (!is_teacher) {
                write_response(res, "Пользователь не является преподавателем.", 406);
                return;
            }

            // Добавление в преподаватели
            teachers_ids.push_back(user_id);
            discipline_settings.set("teachers", dbConnector.convert_to_string_format(teachers_ids));
        }
        else { write_response(res, "Некорректный запрос.", 400); return; } // если user_type не соответствует 
        
        int status = dbConnector.update_rows("disciplines", discipline_settings, "id="+std::to_string(discipline_id));
        if (status == 200) write_response(res, "Успешное зачисление пользователя на дисциплину.");
        else write_response(res, "Ошибка при обновлении базы данных.", status);
    });
    // Отчисляет пользователя с указанным ID с дисциплины с указанным ID
    server.Post("/disciplines/users/remove", [this](const httplib::Request& req, httplib::Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен недействителен.", 401);
            return;
        }
        if (!token.has_permission("discipline:user:remove")) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }
        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("discipline_id") || !body_json.contains("user_id")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }

        // Получаем ин-фу дисциплины
        int discipline_id = dbConnector.get_as_int(body_json["discipline_id"]);
        DBRow discipline_row = dbConnector.get_row("disciplines", "teachers, students", "id="+std::to_string(discipline_id));
        // Получаем айди пользователя
        std::string user_id = body_json["user_id"].get<std::string>();

        // Получаем преподавателей
        std::vector<std::string> teachers_ids = discipline_row.get<std::vector<std::string>>("teachers");
        // Нельзя, если удаляем преподавателя, а их 1
        bool among_teachers = contains(teachers_ids, user_id);
        if (among_teachers && teachers_ids.size() == 1) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }

        // Получаем студентов
        std::vector<std::string> students_ids = discipline_row.get<std::vector<std::string>>("students");
        // Нельзя, если  пользователя вовсе нет на дисциплине
        bool among_students = contains(students_ids, user_id);
        if (!among_students) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }

        // Можно если админ, или сам себя удаляет, или если удаляет первый преподаватель
        if (token.has_role("Admin") || token.get_user() == user_id || teachers_ids[0] == token.get_user()) {
            // Создаём настройки для изменения БД
            DBRow discipline_settings;

            // Настраиваем дисциплину
            if (among_students) {
                auto student_it = std::find(students_ids.begin(), students_ids.end(), user_id);
                if (student_it != students_ids.end()) students_ids.erase(student_it);
                discipline_settings.set("students", dbConnector.convert_to_string_format(students_ids));
            }
            else  {
                auto teacher_it = std::find(teachers_ids.begin(), teachers_ids.end(), user_id);
                if (teacher_it != teachers_ids.end()) teachers_ids.erase(teacher_it);
                discipline_settings.set("teachers", dbConnector.convert_to_string_format(teachers_ids));
            } 

            // Обновляем записи
            int status = dbConnector.update_rows("disciplines", discipline_settings, "id="+std::to_string(discipline_id));
            if (status == 200) write_response(res, "Пользователь отчислен с дисциплины.");
            else write_response(res, "Ошибка при обновлении базы данных.", status);
        }
        else write_response(res, "Недостаточно прав.", 403);
    });
    // Тесты
    // Возвращает запрощенную информацию о тесте
    server.Get("/tests/data", [this](const httplib::Request& req, httplib::Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен недействителен.", 401);
            return;
        }
        // Получение тела, проверка
        nlohmann::json params_json = get_params_json(req.params);
        if (!params_json.contains("test_id") || !params_json.contains("filter")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }
        std::string filter = params_json["filter"].get<std::string>();
        if (filter == "*" || filter == "all") filter = "text status max_tries discipline questions trieslist has_tries";
        
        // Айди теста
        int test_id = dbConnector.get_as_int(params_json["test_id"]);
        // Получает тест из БД
        DBRow test_row = dbConnector.get_row("tests", "*", "id="+std::to_string(test_id));
        if (test_row.is_empty()) {
            write_response(res, "Тест не найден.", 404);
            return;
        }
        
        // Удалённый тест может получить только Админ
        std::string status = test_row.get<std::string>("status");
        if (status == "Deleted" && !token.has_role("Admin")) {
            write_response(res, "Тест недоступен.", 403);
            return;
        }

        // Получаем дисциплину
        DBRow discipline_row = dbConnector.get_row("disciplines", "title, teachers", std::to_string(test_id)+"=ANY(tests)");

        // Проверяет наличие разрешений для всех выбранных фильтров, требующих это
        // Попытки прохождения
        if (filter.find("trieslist") != std::string::npos) {
            // Само разрешение
            if (!token.has_permission("test:tries:read")) { write_response(res, "Недостаточно прав.", 403); return; }
            // Можно смотреть попытки только преподавателю дисциплины и админу
            if (!token.has_role("Admin")) {
                // Проверяем наличие в преподавателях автора запроса
                std::vector<std::string> teachers_ids = discipline_row.get<std::vector<std::string>>("teachers");
                if (!contains(teachers_ids, token.get_user()))
                    { write_response(res, "Недостаточно прав.", 403); return; }
            }
        }

        // Собирает json
        nlohmann::json json;
        if (filter.find("text") != std::string::npos) {
            json["title"] = test_row.get<std::string>("title");
            json["describtion"] = test_row.get<std::string>("describtion");
        }
        if (filter.find("status") != std::string::npos) {
            json["status"] = status;
        }
        if (filter.find("disciplines") != std::string::npos) {
            // Впечатываем заголовок и айди
            json["discipline_title"] = discipline_row.get<std::string>("title");
            json["discipline_id"] = discipline_row.get<std::string>("id");
            json["discipline_status"] = discipline_row.get<std::string>("status");
        }
        if (filter.find("questions") != std::string::npos) {
            json["questions"] = nlohmann::json::array();
            // Берём вопросы теста
            std::vector<std::vector<int>> questions_signatures = test_row.get<std::vector<std::vector<int>>>("questions_signatures");
            for (std::vector<int>& question_signature : questions_signatures) {
                DBRow question_row = dbConnector.get_row("questions", "*",
                    "id="+std::to_string(question_signature[0])+" AND version="+std::to_string(question_signature[1]));
                nlohmann::json row_json;
                // Формирует информацию (Любой вопрос можно получить)
                row_json["question_id"] = question_row.get<int>("id");
                row_json["question_version"] = question_row.get<int>("version");
                row_json["title"] = question_row.get<std::string>("title");
                row_json["options"] = question_row.get<std::vector<std::string>>("options");
                row_json["points"] = question_row.get<std::vector<int>>("points");
                row_json["max_options"] = question_row.get<int>("max_options_in_answer");
                row_json["author_id"] = question_row.get<int>("author");
                // Добавляет
                json["questions"].push_back(row_json);
            }
        }
        if (filter.find("tries") != std::string::npos) {
            // Если нужно максимальное кол-во для теста
            if (filter.find("max_tries") != std::string::npos) {
                json["max_tries"] = test_row.get<int>("max_tries_for_student");
            }

            std::vector<int> tries_ids = test_row.get<std::vector<int>>("tries");
            bool has_tries = tries_ids.size() > 0;

            // Если нужно их общее кол-во
            if (filter.find("has_tries") != std::string::npos) {
                json["has_tries"] = has_tries;
            }

            // Если нужен их список
            if (filter.find("trieslist") != std::string::npos) {
                // Получаем строки данных о попытках
                json["tries"] = nlohmann::json::array();
                if (has_tries) {
                    std::vector<DBRow> tries_rows = dbConnector.get_rows("tries", "*", "id IN("+build_range(tries_ids)+")");
                    for (DBRow& try_row : tries_rows) {
                        nlohmann::json row_json;
                        // Формирует информацию (любую попытку можно смотреть, если есть разрешение)
                        row_json["try_id"] = try_row.get<int>("id");
                        row_json["status"] = try_row.get<std::string>("status");
                        row_json["points"] = try_row.get<int>("points");
                        row_json["max_points"] = try_row.get<int>("max_points");
                        row_json["score_percent"] = try_row.get<int>("score_percent");
                        // Находит и вставляет автора
                        std::string author_id = try_row.get<std::string>("author");
                        DBRow author_row = dbConnector.get_row("users", "id, first_name, last_name, patronimyc", "id='"+author_id+"'");
                        std::string fullname;
                        if (author_row.is_empty())
                            fullname = "Неизвестно";
                        else {
                            std::string first = author_row.get<std::string>("first_name");
                            std::string last = author_row.get<std::string>("last_name");
                            std::string patronimyc = author_row.get<std::string>("patronimyc");
                            fullname = build_fullname(first, last, patronimyc);
                        }
                        row_json["author_id"] = author_id;
                        row_json["author_fullname"] = fullname;
                        // Добавляет
                        json["tries"].push_back(row_json);
                    }
                }
            }
        }
        // Отправляет ответ
        write_response(res, json, "Информация о тесте успешно получена.");
    });
    // Заменяет Название и(или) Описание теста на указанные по его ID
    server.Post("/tests/text", [this](const httplib::Request& req, httplib::Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен недействителен.", 401);
            return;
        }
        if (!token.has_permission("test:text:write")) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }
        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("test_id") || (!body_json.contains("title") && !body_json.contains("describtion"))) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }

        // Получаем айди
        int test_id = dbConnector.get_as_int(body_json["test_id"]);

        // Получаем тест
        DBRow test_row = dbConnector.get_row("disciplines", "title", "id="+std::to_string(test_id));
        if (test_row.is_empty()) {
            write_response(res, "Тест не найден.", 404);
            return;
        }

        // Получаем строку дисциплины
        DBRow discipline_row = dbConnector.get_row("disciplines", "teachers", std::to_string(test_id)+"=ANY (tests)");
        // Нельзя, если не преподаватель дисциплины
        std::vector<std::string> teachers_ids = discipline_row.get<std::vector<std::string>>("teachers");
        bool among_teachers = contains(teachers_ids, token.get_user());
        if (!among_teachers) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }

        // Формируем изменения
        DBRow settings;
        if (body_json.contains("title")) settings.set("title", body_json["title"].get<std::string>());
        if (body_json.contains("describtion")) settings.set("describtion", body_json["describtion"].get<std::string>());
        // Обновляем данные
        int status = dbConnector.update_rows("tests", settings, "id="+std::to_string(test_id));
        if (status != 200) {
            write_response(res, "Ошибка при обновлении базы данных.", status);
            return;
        }

        // Формируем json
        nlohmann::json json;
        json["title"] = test_row.get<std::string>("title");
        write_response(res, json, "Успешное обновление информации о тесте.");
    });
    // Для теста с указанным ID устанавливает значение активности. 
    server.Post("/tests/status", [this](const httplib::Request& req, httplib::Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен недействителен.", 401);
            return;
        }
        if (!token.has_permission("test:status:write")) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }
        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("test_id") || !body_json.contains("status")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }

        // Получаем айди теста и нужный статус
        int test_id = dbConnector.get_as_int(body_json["test_id"]);
        std::string target_status = body_json["status"].get<std::string>();
        // Проверка корректного статуса
        if (target_status != "Activated" && target_status != "Deactivated" && target_status != "Deleted") {
            write_response(res, "Некорректный запрос.", 400);
            return;
            // Статус Deactivated
            // Все начатые попытки автоматически отмечаются завершёнными.
            // Статус Deleted 
            // Отмечает тест как удалённый (реально ничего не удаляется). Все оценки перестают отображаться, но тоже не удаляются.
        }

        // Получаем строку теста
        DBRow test_row = dbConnector.get_row("tests", "author, status, tries", "id="+std::to_string(test_id));
        if (test_row.is_empty()) {
            write_response(res, "Тест не найден.", 404);
            return;
        }
        std::string author = test_row.get<std::string>("author");
        std::string current_status = test_row.get<std::string>("status");

        // Нельзя сменить на такой же статус
        if (current_status == target_status) {
            write_response(res, "Статус уже соответствует устанавливаемому.", 406);
            return;
        }
        // Нельзя тому, кто не препод дисциплины (2), или не админ (1)
        if (!token.has_role("Admin")) { // 1
            // Получаем строку дисциплины
            DBRow discipline_row = dbConnector.get_row("disciplines", "teachers", std::to_string(test_id)+"=ANY(tests)");
            // Получаем преподавателей
            std::vector<std::string> teachers_ids = discipline_row.get<std::vector<std::string>>("teachers");
            // 2
            bool among_teachers = contains(teachers_ids, token.get_user());
            if (!among_teachers) {
                write_response(res, "Недостаточно прав.", 403);
                return;
            }
        }
        
        // Обновляем статус и проверяем успех
        DBRow settings;
        settings.set("status", target_status);
        int status = dbConnector.update_rows("tests", settings, "id="+std::to_string(test_id));

        // Если статус обновился и теперь явно не активный, заканчивает попытки
        if (status == 200 && target_status != "Activated") {
            // Получаем попытки, которые не закончены и для этого теста
            std::vector<int> tries_ids = test_row.get<std::vector<int>>("tries");
            if (tries_ids.size() > 0) {
                std::vector<DBRow> tries_rows = dbConnector.get_rows("tries", "id", "status='Solving' AND id IN("+build_range(tries_ids)+")");
                // Заполняем решённые
                std::vector<int> solving_tries_ids;
                for (DBRow& try_row : tries_rows)
                    solving_tries_ids.push_back(try_row.get<int>("id"));
                if (solving_tries_ids.size() > 0) {
                    // Создаём настройку статуса
                    DBRow settings;
                    settings.set("status", "Solved");
                    // Ставим для всех
                    status = dbConnector.update_rows("tries", settings, "id IN ("+build_range(solving_tries_ids)+")");
                }
            }
        }
        
        if (status != 200) {
            write_response(res, "Ошибка при обновлении базы данных.", status);
            return;
        }
        
        // Формирует json
        nlohmann::json json;
        json["title"] = test_row.get<std::string>("title");
        write_response(res, json, "Успешное обновление статуса теста.");
    });
    // Если у теста ещё не было попыток прохождения, то добавляет в теста с указанным ID вопрос с указанным ID в последнюю позицию
    server.Post("/tests/questions/add", [this](const httplib::Request& req, httplib::Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен недействителен.", 401);
            return;
        }
        if (!token.has_permission("test:question:add")) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }
        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("test_id") || !body_json.contains("question_id")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }

        // Получаем айди теста и нужный вопрос, если он есть
        int test_id = dbConnector.get_as_int(body_json["test_id"]);
        int question_id =  dbConnector.get_as_int(body_json["question_id"]);

        // Получаем строку теста
        DBRow test_row = dbConnector.get_row("tests", "status, tries, questions_signatures", "id="+std::to_string(test_id));
        // Проверяем существование теста
        if (test_row.is_empty()) {
            write_response(res, "Ресурс не найден.", 404);
            return;
        }
        // Статус
        std::string test_status = test_row.get<std::string>("status");
        if (test_status == "Deleted") {
            write_response(res, "Тест удалён.", 406);
            return;
        }
        // Проверяем на то, есть ли попытки прохождения
        std::vector<int> tries_ids = test_row.get<std::vector<int>>("tries");
        if (tries_ids.size() > 0) {
            write_response(res, "Нельзя изменить список вопросов теста, который уже имеет попытки прохождения.", 406);
            return;
        }

        // Получаем строку дисциплины теста
        DBRow discipline_row = dbConnector.get_row("disciplines", "teachers", std::to_string(test_id)+"=ANY(tests)");
        // Получаем преподавателей
        std::vector<std::string> teachers_ids = discipline_row.get<std::vector<std::string>>("teachers");
        // Проверяем, что автор запроса - преподаватель дисциплины, к которой привязан тест
        bool among_teachers = contains(teachers_ids, token.get_user());
        if (!among_teachers) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }

        // Получаем вопрос
        DBRow question_row = dbConnector.get_row("questions", "id, version", body_json.contains("question_version") ?
            // Если нужная версия указана, получаем её
            ("id="+std::to_string(question_id)+" AND version="+body_json["question_version"].get<std::string>()) :
            // Получаем последнюю версию версию, если она не указана в запросе.
            ("id="+std::to_string(question_id)+" AND version=(SELECT MAX(version) FROM questions WHERE id="+std::to_string(question_id)+")"));
        // Проверяем наличие добавляемого вопроса впринципе
        if (question_row.is_empty()) {
            write_response(res, "Ресурс не найден.", 404);
            return;
        }

        // Добавляем
        DBRow settings;
        std::vector<std::vector<int>> questions_signatures = test_row.get<std::vector<std::vector<int>>>("questions_signatures");
        std::vector<int> new_signature;
        new_signature.push_back(question_row.get<int>("id"));
        new_signature.push_back(question_row.get<int>("version"));
        questions_signatures.push_back(new_signature);
        if (test_status == "Activated") settings.set("status", "Deactivated"); // Если тест активен, деактивируем
        settings.set("questions_signatures", dbConnector.convert_to_string_format(questions_signatures));
        int status = dbConnector.update_rows("tests", settings, "id="+std::to_string(test_id));
        if (status == 200) write_response(res, "Успешное добавление вопроса в тест.");
        else write_response(res, "Ошибка при обновлении базы данных.", status);
    });
    // Если у теста ещё не было попыток прохождения, то удаляет у теста с указанным ID вопрос с указанным номером
    server.Post("/tests/questions/remove", [this](const httplib::Request& req, httplib::Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен недействителен.", 401);
            return;
        }
        if (!token.has_permission("test:question:remove")) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }
        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("test_id") || !body_json.contains("question_number")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }

        // Получаем айди теста и нужный вопрос, если он есть
        int test_id = dbConnector.get_as_int(body_json["test_id"]);
        int question_number =  dbConnector.get_as_int(body_json["question_number"]);

        // Получаем строку теста
        DBRow test_row = dbConnector.get_row("tests", "tries, questions_signatures", "id="+std::to_string(test_id));
        // Проверяем существование теста
        if (test_row.is_empty()) {
            write_response(res, "Ресурс не найден.", 404);
            return;
        }
        // Проверяем на то, есть ли попытки прохождения
        std::vector<int> tries_ids = test_row.get<std::vector<int>>("tries");
        if (tries_ids.size() > 0) {
            write_response(res, "Нельзя изменить список вопросов теста, который уже имеет попытки прохождения.", 406);
            return;
        }

        // Получаем строку дисциплины теста
        DBRow discipline_row = dbConnector.get_row("disciplines", "teachers", std::to_string(test_id)+"=ANY(tests)");
        // Получаем преподавателей
        std::vector<std::string> teachers_ids = discipline_row.get<std::vector<std::string>>("teachers");
        // Проверяем, что автор запроса - преподаватель дисциплины, к которой привязан тест
        bool among_teachers = contains(teachers_ids, token.get_user());
        if (!among_teachers) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }

        // Получаем сигнатуры вопросов
        std::vector<std::vector<int>> questions_signatures = test_row.get<std::vector<std::vector<int>>>("questions_signatures");
        // Проверяем на выход за пределы
        if (question_number - 1 > questions_signatures.size()) {
            write_response(res, "Нельзя удалить вопрос с несуществующим номером.", 406);
            return;
        }
        // Удаляем сигнатуру
        questions_signatures.erase(questions_signatures.begin() + question_number - 1);
        // Обновляем запись
        DBRow settings;
        settings.set("questions_signatures", dbConnector.convert_to_string_format(questions_signatures));
        int status = dbConnector.update_rows("tests", settings, "id="+std::to_string(test_id));
        if (status == 200) write_response(res, "Успешное удаление вопроса из теста.");
        else write_response(res, "Ошибка при обновлении базы данных.", status);
    });
    // Если у теста ещё не было попыток прохождения, то для теста с указанным ID устанавливает указанную последовательность вопросов
    server.Post("/tests/questions/update", [this](const httplib::Request& req, httplib::Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен недействителен.", 401);
            return;
        }
        if (!token.has_permission("test:question:update")) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }
        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("test_id") || !body_json.contains("questions_signatures")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }

        // Получаем айди теста
        int test_id = dbConnector.get_as_int(body_json["test_id"]);

        // Получаем строку теста
        DBRow test_row = dbConnector.get_row("tests", "tries", "id="+std::to_string(test_id));
        // Проверяем существование теста
        if (test_row.is_empty()) {
            write_response(res, "Ресурс не найден.", 404);
            return;
        }
        // Проверяем на то, есть ли попытки прохождения
        std::vector<int> tries_ids = test_row.get<std::vector<int>>("tries");
        if (tries_ids.size() > 0) {
            write_response(res, "Нельзя изменить список вопросов теста, который уже имеет попытки прохождения.", 406);
            return;
        }

        // Получаем строку дисциплины теста
        DBRow discipline_row = dbConnector.get_row("disciplines", "teachers", std::to_string(test_id)+"=ANY(tests)");
        // Получаем преподавателей
        std::vector<std::string> teachers_ids = discipline_row.get<std::vector<std::string>>("teachers");
        // Проверяем, что автор запроса - преподаватель дисциплины, к которой привязан тест
        bool among_teachers = contains(teachers_ids, token.get_user());
        if (!among_teachers) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }

        // Получаем сигнатуры
        std::vector<std::vector<int>> questions_signatures = dbConnector.get_as_signatures(body_json["questions_signatures"]);
        // Проверяем наличие каждого вопроса. Если не надйен, скип
        for (int i = 0; i < questions_signatures.size(); i++) {
            // Получает вопрос из БД
            DBRow question_row = dbConnector.get_row("questions", "id, version", questions_signatures[i].size() > 1 ?
                // Если нужная версия указана, получаем её
                ("id="+std::to_string(questions_signatures[i][0])+" AND version="+std::to_string(questions_signatures[i][1])) :
                // Получаем последнюю версию версию, если она не указана в запросе.
                ("id="+std::to_string(questions_signatures[i][0])+" AND version=(SELECT MAX(version) FROM questions WHERE id="+std::to_string(questions_signatures[i][0])+")"));

            // Проверяем существование
            if (question_row.is_empty()) {
                // Удаляем, если нету
                questions_signatures.erase(questions_signatures.begin() + i);
                i++;
            }
            // Иначе устанавливаем версию, если не было
            else if (questions_signatures[i].size() == 1) questions_signatures[i].push_back(question_row.get<int>("id"));
        }
        // Обновляем запись
        DBRow settings;
        settings.set("questions_signatures", dbConnector.convert_to_string_format(questions_signatures));
        int status = dbConnector.update_rows("tests", settings, "id="+std::to_string(test_id));
        if (status == 200) write_response(res, "Успешное обновление вопросов для теста.");
        else write_response(res, "Ошибка при обновлении базы данных.", status);
    });
    // Для теста с указанным ID выбирает все попытки пользователя с определённым ID
    server.Get("/tests/tries/user", [this](const httplib::Request& req, httplib::Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен недействителен.", 401);
            return;
        }
        // Нельзя без разрешений
        if (!token.has_permission("test:tries:read")) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }
        // Получение тела, проверка
        nlohmann::json params_json = get_params_json(req.params);
        if (!params_json.contains("test_id") || !params_json.contains("user_id")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }
        
        // Получаем айди теста и пользователя
        int test_id = dbConnector.get_as_int(params_json["test_id"]);
        std::string user_id = params_json["user_id"].get<std::string>();

        // Получаем строку теста
        DBRow test_row = dbConnector.get_row("tests", "*", "id="+std::to_string(test_id));
        // Проверяем существование теста
        if (test_row.is_empty()) {
            write_response(res, "Тест не найден.", 404);
            return;
        }

        // Можно, если админ, автор попыток, которые хочет получить
        if (!token.has_role("Admin") && user_id != token.get_user()) {
            // Получаем строку дисциплины теста
            DBRow discipline_row = dbConnector.get_row("disciplines", "teachers", std::to_string(test_id)+"=ANY(tests)");
            // Получаем преподавателей
            std::vector<std::string> teachers_ids = discipline_row.get<std::vector<std::string>>("teachers");
            // Проверяем, что автор запроса - преподаватель дисциплины, к которой привязан тест
            bool among_teachers = contains(teachers_ids, token.get_user());
            if (!among_teachers) {
                write_response(res, "Недостаточно прав.", 403);
                return;
            }
        }

        // Формируем json для ответа
        nlohmann::json json;
        // Получаем строки данных о попытках
        json["tries"] = nlohmann::json::array();
        std::vector<DBRow> tries_rows =
            dbConnector.get_rows("tries", "*", "author='"+user_id+"' AND test="+std::to_string(test_id));
        for (DBRow& try_row : tries_rows) {
            // Строка с информацией // Можно даже если тест удалён
            nlohmann::json row_json;
            // О попытке
            int try_id = try_row.get<int>("id");
            row_json["try_id"] = try_id;
            row_json["status"] = try_row.get<std::string>("status");
            row_json["points"] = try_row.get<int>("points");
            row_json["max_points"] = try_row.get<int>("max_points");
            row_json["score_percent"] = try_row.get<int>("score_percent");
            // О тесте попытки
            row_json["test_id"] = test_row.get<int>("id");
            row_json["test_status"] = test_row.get<std::string>("status");
            row_json["test_title"] = test_row.get<std::string>("title");
            // Добавляет
            json["tries"].push_back(row_json);
        }
        write_response(res, json, "Успешное получение данных о попытке пользователя.");
    });
    // Вопросы
    // Возвращает массив содержащий Название вопроса, его версию и ID автора для каждого теста в системе. Если у вопроса есть несколько версий, показывается только последняя
    server.Get("/questions/list", [this](const httplib::Request& req, httplib::Response& res) {
        // Получение токена
        Token token = get_request_token(req);
        // Проверка токена и разрешений
        if (!token.is_valid()) {
            write_response(res, "Токен недействителен.", 401);
            return;
        }
        if (!token.has_permission("questions:list")) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }
        
        // Получает список
        std::vector<DBRow> list = dbConnector.get_rows("questions", "id, version, title, status, options, max_options_in_answer", "", "BY id ASC, version ASC");
        // Формирует json для списка
        nlohmann::json json;
        json["questions"] = nlohmann::json::array();
        for (auto& row : list) {
            std::string status = row.get<std::string>("status");
            // Удалённый вопрос может видеть только админ
            if (status == "Deleted" && !token.has_role("Admin")) continue;
            // Формирует json для отдельной строки
            nlohmann::json row_json;
            row_json["question_id"] = row.get<int>("id");
            row_json["question_version"] = row.get<int>("version");
            row_json["title"] = row.get<std::string>("title");
            row_json["status"] = status;
            row_json["options"] = row.get<std::vector<std::string>>("options");
            row_json["max_options"] = row.get<int>("max_options_in_answer");
            json["questions"].push_back(row_json);
        }
        // Отправляет
        write_response(res, json, "Успешное получение списка вопросов.");
    });
    // Для указанного ID вопроса и версии возвращает Название (Текст вопроса), Варианты ответов, Номер правильного ответа
    server.Get("/questions/data", [this](const httplib::Request& req, httplib::Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен недействителен.", 401);
            return;
        }
        // Само разрешение
        if (!token.has_permission("question:read")) {
            write_response(res, "Недостаточно прав.", 403);
            return;
            }
        // Получение тела, проверка
        nlohmann::json params_json = get_params_json(req.params);
        if (!params_json.contains("question_id") || !params_json.contains("filter")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }
        std::string filter = params_json["filter"].get<std::string>();
        if (filter == "*" || filter == "all") filter = "title status options points author max_options";
        
        // Айди вопроса
        int question_id = dbConnector.get_as_int(params_json["question_id"]);
        
        // Получает вопрос из БД
        DBRow question_row = dbConnector.get_row("questions", "*", params_json.contains("question_version") ?
            // Если нужная версия указана, получаем её
            ("id="+std::to_string(question_id)+" AND version="+params_json["question_version"].get<std::string>()) :
            // Получаем последнюю версию версию, если она не указана в запросе.
            ("id="+std::to_string(question_id)+" AND version=(SELECT MAX(version) FROM questions WHERE id="+std::to_string(question_id)+")"));

        // Проверяем существование
        if (question_row.is_empty()) {
            write_response(res, "Вопрос не найден.", 404);
            return;
        }

        // Обработка статуса
        std::string status = question_row.get<std::string>("status");
        if (status == "Deleted" && !token.has_role("Admin")) {
            write_response(res, "Нельзя получить данные о вопросе.", 403);
            return;
        }

        // Собирает json
        nlohmann::json json;
        json["version"] = question_row.get<int>("version");
        if (filter.find("title") != std::string::npos) {
            json["title"] = question_row.get<std::string>("title");
        }
        if (filter.find("status") != std::string::npos) {
            json["status"] = status;
        }
        if (filter.find("author") != std::string::npos) {
            json["author"] = question_row.get<std::string>("author");
        }
        if (filter.find("options") != std::string::npos) {
            json["options"] = question_row.get<std::vector<std::string>>("options");
        }
        if (filter.find("points") != std::string::npos) {
            json["points"] = question_row.get<std::vector<int>>("points");
        }
        if (filter.find("max_options") != std::string::npos) {
            json["max_options"] = question_row.get<int>("max_options_in_answer");
        }
        // Отправляет ответ
        write_response(res, json, "Успешное получение данных о вопросе.");
    });
    // Создаёт новый вопрос с заданным Названием, Тексом вопроса, Вариантами ответов, Номером правильного ответа. Версия вопроса 1. В качестве ответа возвращается ID вопроса
    server.Post("/questions/create", [this](const httplib::Request& req, httplib::Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен недействителен.", 401);
            return;
        }
        if (!token.has_permission("question:create")) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }

        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("title")
            || !body_json.contains("options") || !body_json.contains("points") || !body_json.contains("max_options")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }
        
        // Добавляет
        DBRow settings;
        settings.set("title", body_json["title"].get<std::string>());
        settings.set("options", dbConnector.convert_to_string_format(body_json["options"].get<std::vector<std::string>>()));
        settings.set("points", dbConnector.convert_to_string_format(dbConnector.get_as_intarr(body_json["points"])));
        settings.set("max_options_in_answer", body_json["max_options"].get<std::string>()); 
        settings.set("author", token.get_user());
        std::cout << 8 << std::endl;
        DBRow question_row = dbConnector.insert_row("questions", settings);
        if (question_row.is_empty()) {
            write_response(res, "Ошибка при создании вопроса.", 500);
            return;
        }

        // Формирует json ответ
        nlohmann::json json;
        json["question_id"] = question_row.get<int>("id");
        json["question_version"] = question_row.get<int>("version");
        write_response(res, json, "Успешное создание вопроса.");
    });   
    // Для указанного ID вопроса создаёт новую версию с заданным Названием, Тексом вопроса, Вариантами ответов, Номером правильного ответа
    server.Post("/questions/update", [this](const httplib::Request& req, httplib::Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен недействителен.", 401);
            return;
        }
        if (!token.has_permission("test:question:update")) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }

        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("question_id") ||
            !body_json.contains("title") || !body_json.contains("options") || !body_json.contains("points") || !body_json.contains("max_options")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }
        
        // Получаем айди
        int question_id = dbConnector.get_as_int(body_json["question_id"]);
        // Строку, проверяем на существование
        DBRow question_row = dbConnector.get_row("questions", "id, version, author", "id="+std::to_string(question_id));
        if (question_row.is_empty()) {
            write_response(res, "Вопрос не найден.", 404);
            return;
        }
        // Проверяем на автора
        if (question_row.get<std::string>("author") != token.get_user()) {
            write_response(res, "Вопрос может обновить только автор.", 403);
            return;
        }

        // Добавляет
        DBRow settings;
        settings.set("id", body_json["question_id"].get<std::string>());
        settings.set("title", body_json["title"].get<std::string>());
        settings.set("options", dbConnector.convert_to_string_format(body_json["options"].get<std::vector<std::string>>()));
        settings.set("points", dbConnector.convert_to_string_format(dbConnector.get_as_intarr(body_json["points"])));
        settings.set("max_options_in_answer", body_json["max_options"].get<std::string>()); 
        settings.set("author", token.get_user());
        DBRow new_question_row = dbConnector.insert_row("questions", settings);
        if (new_question_row.is_empty()) {
            write_response(res, "Ошибка при создании новой версии вопроса.", 500);
            return;
        }

        // Формирует json ответ
        nlohmann::json json;
        json["question_id"] = new_question_row.get<int>("id");
        json["question_version"] = new_question_row.get<int>("version");
        write_response(res, json, "Успешное создание новой версии вопроса.");
    });
    // Если вопрос не используется в тестах (даже удалённых), то вопрос отмечается как удалённый (но реально не удаляется)
    server.Post("/questions/status", [this](const httplib::Request& req, httplib::Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен недействителен.", 401);
            return;
        }

        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("question_id") && !body_json.contains("status")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }

        // Статус нужный
        std::string target_status = body_json["status"].get<std::string>();
        if (target_status != "Deleted" && target_status != "Exists") {
            write_response(res, "Статус не существует.", 400);
            return;
        }
        // Если нет прав, а хочет удалить
        if (target_status == "Deleted" && !token.has_permission("question:delete")) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }

        // Получаем строку вопроса
        int question_id = dbConnector.get_as_int(body_json["question_id"]);
        DBRow question_row = dbConnector.get_row("questions", "status, author", "id="+std::to_string(question_id));
        // Проверяем существование
        if (question_row.is_empty()) {
            write_response(res, "Вопрос не найден.", 404);
            return;
        }
        // Проверяем статус
        std::string current_status = question_row.get<std::string>("status");
        if (current_status == target_status) {
            write_response(res, "Вопрос уже имеет нужный статус.", 406);
            return;
        }
        // Можно только админу или автору вопроса
        std::string author = question_row.get<std::string>("author");
        if (!token.has_role("Admin") && author != token.get_user()) {
            write_response(res, "Вопрос может быть удалён только автором.", 403);
            return;
        }

        // Ставим статус
        DBRow settings;
        settings.set("status", target_status);
        int status = body_json.contains("question_version") ?
            dbConnector.update_rows("questions", settings, "id="+std::to_string(question_id)+" AND version="+body_json["question_version"].get<std::string>()) :
            dbConnector.update_rows("questions", settings, "id="+std::to_string(question_id));
        if (status == 200) write_response(res, (body_json.contains("question_version") ?
            "Успешное изменение статуса вопроса." : "Успешное изменение статуса вопросов."));
        else write_response(res, "Ошибка при изменении статуса вопроса.", status);
    });
    // Попытки
    // Если пользователь с ID ещё не отвечал на тест с ID и тест находится в активном состоянии,
    // то создаётся новая попытка и возвращается её ID, А ТАКЖЕ ВСЕ ВОПРОСЫ И БАЛЛЫ ЗА ОТВЕТЫ
    server.Post("/tries/start", [this](const httplib::Request& req, httplib::Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен недействителен.", 401);
            return;
        }
        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("test_id")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }

        // Айди теста
        int test_id = dbConnector.get_as_int(body_json["test_id"]);
        // Получает тест из БД
        DBRow test_row = dbConnector.get_row("tests", "*", "id="+std::to_string(test_id));
        // Его статус (ДАЖЕ АДМИНУ нельзя проходить удалённый тест)
        std::string test_status = test_row.get<std::string>("status");
        if (test_status == "Deleted") {
            write_response(res, "Тест удалён.", 403);
            return;
        }

        // Можно начать, если админу
        if (!token.has_role("Admin")) {
            // Получаем строку дисциплины, для теста которой создаём попытку
            DBRow discipline_row = dbConnector.get_row("disciplines", "teachers, students", std::to_string(test_id)+"=ANY(tests)");
            // Проверяем наличие в преподавателях автора запроса // Можно преподавателю дисциплины
            std::vector<std::string> teachers_ids = discipline_row.get<std::vector<std::string>>("teachers");
            bool among_teachers = contains(teachers_ids, token.get_user());
            if (!among_teachers) {
                // Проверяем наличие в студентах автора запроса // Можно студенту дисциплины (1), если тест активен (2) и попытки не исчерпаны (3)
                // 1
                std::vector<std::string> students_ids = discipline_row.get<std::vector<std::string>>("students");
                bool among_students = contains(students_ids, token.get_user());
                if (!among_students) { write_response(res, "Недостаточно прав.", 403); return; }
                // 2
                if (test_status != "Activated") {
                    write_response(res, "Тест неfдоступен.", 403);
                    return;
                }
                // 3
                int max_tries_count = test_row.get<int>("max_tries_for_student");
                std::vector<int> tries_ids = test_row.get<std::vector<int>>("tries"); // айди попыток, принадлежащих тесту
                size_t current_tries_count =
                    dbConnector.get_rows("tries", "id", "author='"+token.get_user()+ // Получаем строки попыток с нужным автором,
                        "' AND id IN("+build_range(tries_ids)+")") // и принадлежащими тесту
                            .size(); // кол-во подошедших строк
                if (current_tries_count >= max_tries_count) { write_response(res, "Попытки исчерпаны.", 403); return; }
            }

        }
        
        // Поучаем вопросы
        std::vector<std::vector<int>> questions_signatures = test_row.get<std::vector<std::vector<int>>>("questions_signatures");
        // Если их 0, ошибку
        if (questions_signatures.size() == 0) {
            write_response(res, "Тест пустой.", 406);
            return;
        }

        // Создаём попытку
        // Сама настройка
        DBRow try_settings;
        // Статус и автор, тест
        try_settings.set("status", "Solving");
        try_settings.set("author", token.get_user());
        try_settings.set("test", dbConnector.convert_to_string_format(test_id));
        // Ответы
        std::vector<int> answers_ids;
        // Создаёт строку ответа для каждого вопроса теста
        for (std::vector<int>& question_signature : questions_signatures) {
            DBRow answer_settings;
            answer_settings.set("question_signature", dbConnector.convert_to_string_format(question_signature));
            answer_settings.set("author", token.get_user());
            // Вставляет её в БД
            DBRow answer_row = dbConnector.insert_row("answers", answer_settings);
            if (answer_row.is_empty()) {
                write_response(res, "Ошибка создания попытки для прохождения теста.", 406);
                return;
            }
            // Получает её айди
            answers_ids.push_back(answer_row.get<int>("id"));
        }
        try_settings.set("answers", dbConnector.convert_to_string_format(answers_ids));
        // Добавляем запись попытки в таблицу попыток
        DBRow try_row = dbConnector.insert_row("tries", try_settings);
        if (try_row.is_empty()) {
            write_response(res, "Ошибка создания попытки для прохождения теста.", 406);
            return;
        }
        int try_id = try_row.get<int>("id");

        // Добавляем попытку в тест
        DBRow test_settings;
        std::vector<int> tries_ids = test_row.get<std::vector<int>>("tries");
        tries_ids.push_back(try_id);
        test_settings.set("tries", dbConnector.convert_to_string_format(tries_ids));
        int add_status = dbConnector.update_rows("tests", test_settings, "id="+std::to_string(test_id));
        if (add_status != 200) {
            write_response(res, "Ошибка при записи попытки.", add_status);
            return;
        }

        // Формурем json ответ, получая данные для каждого вопроса (даже удалённого).
        nlohmann::json json;
        json["try_id"] = try_id;
        json["questions"] = nlohmann::json::array();
        for (int i = 0; i < questions_signatures.size(); i ++) {
            DBRow question_row =
                dbConnector.get_row("questions", "*", "id="+std::to_string(questions_signatures[i][0])+
                    " AND version="+std::to_string(questions_signatures[i][1]));
            nlohmann::json row_json;
            row_json["answer_id"] = answers_ids[i];
            row_json["question_id"] = question_row.get<int>("id");
            row_json["question_version"] = question_row.get<int>("version");
            row_json["title"] = question_row.get<std::string>("title");
            row_json["options"] = question_row.get<std::vector<std::string>>("options");
            row_json["points"] = question_row.get<std::vector<int>>("points");
            row_json["max_options"] = question_row.get<int>("max_options_in_answer");
            json["questions"].push_back(row_json);
        }
        write_response(res, json, "Попытка начата.");
    });
    // Если тест находится в активном состоянии и пользователь ещё не закончил попытку, то для попытки с ID изменяет значение ответа с ID
    server.Post("/tries/answer/change", [this](const httplib::Request& req, httplib::Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен недействителен.", 401);
            return;
        }
        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("answer_id") || !body_json.contains("options")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }

        // Получить ответ
        int answer_id = dbConnector.get_as_int(body_json["answer_id"]);
        // Попытку
        DBRow try_row = dbConnector.get_row("tries", "*", std::to_string(answer_id)+"=ANY(answers)");
        // Проверяем на её существование
        if (try_row.is_empty()) {
            write_response(res, "Ресурс не найден.", 404);
            return;
        }

        // Можно, если сам
        std::string author_id = try_row.get<std::string>("author");
        if (author_id != token.get_user()) {
            write_response(res, "Нельзя менять ответ не своей попытки.", 403);
            return;
        }
        
        // Нельзя, если попытка не существует, или окончена
        if (try_row.is_empty()) {
            write_response(res, "Попытка не найдена.", 404);
            return;
        }
        if (try_row.get<std::string>("status") != "Solving") {
            write_response(res, "Попытка уже окончена.", 406);
            return;
        }

        // Меняем значение ответа
        std::vector<int> options = dbConnector.get_as_intarr(body_json["options"]);
        DBRow settings;
        settings.set("options", dbConnector.convert_to_string_format(options));
        int status = dbConnector.update_rows("answers", settings, "id="+std::to_string(answer_id));
        if (status == 200) write_response(res, "Успешное изменение ответа.");
        else write_response(res, "Ошибка при обновлении базы данных.", status);
    });
    // Если тест находится в активном состоянии и пользователь ещё не закончил попытку, то устанавливает попытку в состояние: завершено. Если тест переключили в состояние не активный, то все попытки для него автоматически устанавливаются в состояние: завершено
    server.Post("/tries/stop", [this](const httplib::Request& req, httplib::Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен недействителен.", 401);
            return;
        }
        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("try_id")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }

        // Получить попытку
        int try_id = dbConnector.get_as_int(body_json["try_id"]);
        DBRow try_row = dbConnector.get_row("tries", "*", "id="+std::to_string(try_id));
        // Проверяем на её существование
        if (try_row.is_empty()) {
            write_response(res, "Ресурс не найден.", 404);
            return;
        }

        // Можно, если сам
        std::string author_id = try_row.get<std::string>("author");
        if (author_id != token.get_user()) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }

        // Нельзя, если попытка не существует, или окончена
        if (try_row.is_empty()) {
            write_response(res, "Ресурс не найден.", 404);
            return;
        }
        if (try_row.get<std::string>("status") != "Solving") {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }

        // Меняем статус попытки
        DBRow settings;
        settings.set("status", "Solved");
        int status = dbConnector.update_rows("tries", settings, "id="+std::to_string(try_id));
        if (status == 200) write_response(res, "Успешная остановка попытки.");
        else write_response(res, "Ошибка при обновлении базы данных.", status);
    });
    // Для пользователя с ID и теста с ID возвращается массив ответов и статус состояние попытки
    server.Get("/tries/view", [this](const httplib::Request& req, httplib::Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен недействителен.", 401);
            return;
        }
        // Нельзя без разрешений
        if (!token.has_permission("test:tries:read")) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }
        // Получение тела, проверка
        nlohmann::json params_json = get_params_json(req.params);
        if (!params_json.contains("try_id")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }
        
        // Получаем айди попытки
        int try_id = dbConnector.get_as_int(params_json["try_id"]);

        // Получаем строку попытки
        DBRow try_row = dbConnector.get_row("tries", "*", "id="+std::to_string(try_id));
        // Проверяем существование 
        if (try_row.is_empty()) {
            write_response(res, "Попытка не найдена.", 404);
            return;
        }

        // Получаем айди автора
        std::string author_id = try_row.get<std::string>("author");
        
        // Получаем айди теста
        int test_id = try_row.get<int>("test");

        // Можно, если админ или автор попытки, которую хочет получить
        if (!token.has_role("Admin") && author_id != token.get_user()) {
            // Получаем строку дисциплины теста
            DBRow discipline_row = dbConnector.get_row("disciplines", "teachers", std::to_string(test_id)+"=ANY(tests)");
            // Получаем преподавателей
            std::vector<std::string> teachers_ids = discipline_row.get<std::vector<std::string>>("teachers");
            // Проверяем, что автор запроса - преподаватель дисциплины, к которой привязан тест
            bool among_teachers = contains(teachers_ids, token.get_user());
            if (!among_teachers) {
                write_response(res, "Посмотреть попытку могут только преподаватель соответствующей дисциплины и её автор.", 403);
                return;
            }
        }

        // Формируем json для ответа
        nlohmann::json json;
        // Вставляем основные даныне
        std::cout << try_row << std::endl;
        json["status"] = try_row.get<std::string>("status");
        json["points"] = try_row.get<int>("points");
        json["max_points"] = try_row.get<int>("max_points");
        json["score_percent"] = try_row.get<int>("score_percent");
        json["author_id"] = author_id; // Можно добавить имя автора
        // Приводим ответы
        json["answers"] = nlohmann::json::array();
        std::vector<int> answers_ids = try_row.get<std::vector<int>>("answers");
        if (answers_ids.size() > 0) {
            std::vector<DBRow> answers_rows = dbConnector.get_rows("answers", "*", "id IN ("+build_range(answers_ids)+")");
            for (DBRow& answer_row : answers_rows) {
                std::vector<int> question_signature = answer_row.get<std::vector<int>>("question_signature");
                DBRow question_row =
                    dbConnector.get_row("questions", "*", "id="+std::to_string(question_signature[0])
                        +" AND version="+std::to_string(question_signature[1]));
                nlohmann::json row_json;
                row_json["question_id"] = question_row.get<int>("id");
                row_json["question_version"] = question_row.get<int>("version");
                row_json["question_title"] = question_row.get<std::string>("title");
                row_json["question_options"] = question_row.get<std::vector<std::string>>("options");
                row_json["selected_options"] = answer_row.get<std::vector<int>>("options");
                row_json["points"] = answer_row.get<int>("points");
                row_json["max_points"] = answer_row.get<int>("max_points");
                json["answers"].push_back(row_json);
            }
        }
        // Отправляем
        write_response(res, json, "Успешное получение данных о попытке.");
    });
    // Отправляет сообщение с text пользователю с указанным айди
    server.Post("/news/send", [this](const httplib::Request& req, httplib::Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен недействителен.", 401);
            return;
        }
        // Нельзя без разрешений
        if (!token.has_permission("news:write")) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }
        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("recipient") || !body_json.contains("text")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }
        
        // Получаем айди получателя
        std::string user_id = body_json["recipient"].get<std::string>();
        // Нельзя себе
        if (user_id == token.get_user() && user_id != "1") { // 1 - Айди Админа
            write_response(res, "Нельзя отправлять самому себе.", 406);
            return;
        }

        // Получаем строку попытки
        DBRow user_row = dbConnector.get_row("users", "id", "id='"+user_id+"'");
        // Проверяем существование 
        if (user_row.is_empty()) {
            write_response(res, "Получатель не найден.", 404);
            return;
        }

        DBRow settings;
        settings.set("sender", token.get_user());
        settings.set("recipient", user_id);
        settings.set("text", body_json["text"].get<std::string>());
        DBRow news_row = dbConnector.insert_row("news", settings);
        if (news_row.is_empty()) {
            write_response(res, "Ошибка при отправке сообщения.", 406);
            return;
        }
        write_response(res, "Сообщение успешно отправлено.");
    });
    // Возвращает список сообщений с получателем с ID
    server.Get("/news/list/recipient", [this](const httplib::Request& req, httplib::Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен недействителен.", 401);
            return;
        }
        // Получение тела, проверка
        nlohmann::json params_json = get_params_json(req.params);
        if (!params_json.contains("user_id")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }
        
        // Получаем айди получателя
        std::string user_id = params_json["user_id"].get<std::string>();

        // Можно только админу и себе
        if (token.get_user() != user_id && !token.has_role("Admin")) {
            write_response(res, "Нельзя смотреть адресованное другим.", 403);
            return;
        }

        // Получаем строки сообщений
        std::vector<DBRow> news_rows = dbConnector.get_rows("news", "*", "status <> 'Deleted' AND recipient='"+user_id+"'");
        // Формируем json
        nlohmann::json json;
        json["news"] = nlohmann::json::array();
        for (DBRow& news_row : news_rows) {
            // Скип удалённого
            std::string status = news_row.get<std::string>("status");
            if (status == "Deleted") continue;
            // Формирует строку новости
            nlohmann::json row_json;
            row_json["news_id"] = news_row.get<int>("id");
            row_json["status"] = status;
            row_json["text"] = news_row.get<std::string>("text");
            // Начало добавления отправителя
            std::string sender_id = news_row.get<std::string>("sender");
            DBRow sender_row = dbConnector.get_row("users", "id, first_name, last_name, patronimyc", "id='"+sender_id+"'");
            std::string fullname;
            if (sender_row.is_empty())
                fullname = "Неизвестно";
            else {
                std::string first = sender_row.get<std::string>("first_name");
                std::string last = sender_row.get<std::string>("last_name");
                std::string patronimyc = sender_row.get<std::string>("patronimyc");
                fullname = build_fullname(first, last, patronimyc);
            }
            row_json["sender_id"] = sender_id;
            // Конец добавления отправителя
            row_json["sender_fullname"] = fullname;
            // Добавляет в json
            json["news"].push_back(row_json);
        }
        
        write_response(res, json, "Сообщения успешно получены.");
    });
    // Меняет статус новости
    server.Post("/news/status", [this](const httplib::Request& req, httplib::Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен недействителен.", 401);
            return;
        }
        // Нельзя без разрешений
        if (!token.has_permission("news:write")) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }
        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("news_id") || !body_json.contains("status")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }
        
        // Проверка существования статуса
        std::string target_status = body_json["status"].get<std::string>();
        if (target_status != "Sended" && target_status != "Viewed" && target_status != "Deleted") {
            write_response(res, "Несуществующий статус.", 400);
            return;
        }

        // Получаем айди новости
        int news_id = dbConnector.get_as_int(body_json["news_id"]);

        // Получаем строку новости
        DBRow news_row = dbConnector.get_row("news", "status, sender, recipient", "id="+std::to_string(news_id));
        // Проверяем существование 
        if (news_row.is_empty()) {
            write_response(res, "Не найдена новость.", 404);
            return;
        }

        // Получает отправителя и получателя
        std::string current_status = news_row.get<std::string>("status");
        std::string sender_id = news_row.get<std::string>("sender");
        std::string recipient_id = news_row.get<std::string>("recipient");

        // Нельзя, если статус уже такой
        if (current_status == target_status) {
            write_response(res, "Статус сообщения уже установлен.", 406);
            return;
        }
        // Нельзя, если не имеет к сообщению отношения
        if (token.get_user() != sender_id && token.get_user() != recipient_id && token.has_role("Admin")) {
            write_response(res, "Нельзя изменять статус чужого сообщения.", 403);
            return;
        }

        DBRow settings;
        settings.set("status", target_status);
        int status = dbConnector.update_rows("news", settings, "id="+std::to_string(news_id));
        if (status != 200) {
            write_response(res, "Ошибка при изменении статуса сообщения.", status);
            return;
        }
        write_response(res, "Статус сообщения изменён.");
    });
}

// Отправка запросов
std::optional<HttpResponse> APIServer::DoGetRequestToService(const std::string& host, const int& port, const std::string& source,
    const std::string &token_prefix, const std::string &token) {
    // Создаём клиент
    httplib::Client client(host, port);

    // Устанавливаем таймаут (10 секунд)
    client.set_connection_timeout(10, 0);

    // Формируем заголовки
    httplib::Headers headers = {
        {"Authorization", token_prefix + " " + token},
        {"Content-Type", "application/json; charset=utf-8"},
        {"Accept", "application/json"}
    };

    // Отправляем GET‑запрос
    std::cout << "Обращаемся к юрл: " << host << ":" << port << "/" << source << std::endl;
    auto res = client.Get("/"+source, headers);
    // Возвращет
    if (!res) return std::nullopt; // "Ошибка выполнения запроса."
    if (res->status != 200) return HttpResponse{nlohmann::json::parse(res->body), res->status};
    return HttpResponse{nlohmann::json::parse(res->body), 200};
}
std::optional<HttpResponse> APIServer::DoPostRequestToService(const std::string& host, const int& port, const std::string& source,
    const std::string& token_prefix, const std::string& token, const nlohmann::json& body) {
    // Создаём клиент
    httplib::Client client(host, port);

    // Устанавливаем таймаут (10 секунд)
    client.set_connection_timeout(10, 0);

    // Конвертируем JSON в строку
    std::string body_str = body.dump();

    // Формируем заголовки
    httplib::Headers headers = {
        {"Authorization", token_prefix + " " + token},
        {"Content-Type", "application/json; charset=utf-8"}
    };

    // Отправляем POST‑запрос
    std::cout << "Обращаемся к юрл: " << host << ":" << port << "/" << source << ", отправляя тело " << body_str << std::endl;
    auto res = client.Post("/"+source, headers, body_str, "application/json");
    // Возвращает
    if (!res) return std::nullopt; // "Ошибка выполнения запроса."
    if (res->status != 200) return HttpResponse{nlohmann::json::parse(res->body), res->status};
    return HttpResponse{nlohmann::json::parse(res->body), 200};
}

// Вспомогательные методы (не БД)
// Собирание имени
std::string APIServer::build_fullname(const std::string& first_name, const std::string& last_name, const std::string& patronymic) {
    // Если имени нет (пустая строка)
    if (first_name == "") {
        return "Неизвестно";
    }
    std::string fullname = first_name;
    // Добавляем фамилию, если она не пуста
    if (last_name != "") {
        fullname += " " + last_name;
    }
    // Добавляем отчество, если оно не пусто
    if (patronymic != "") {
        fullname += " " + patronymic;
    }
    // Возвращаем
    return fullname;
}
// Построение списка айдишек через запятую для IN
std::string APIServer::build_range(const std::vector<int> &ids) {
    std::string range = "";
    for (int id : ids) {
        if (range != "") range += ", ";
        range += std::to_string(id);
    }
    return range;
}
std::string APIServer::build_range(const std::vector<std::string> &ids) {
    std::string range = "";
    for (std::string id : ids) {
        if (range != "") range += ", ";
        range += "'"+id+"'";
    }
    return range;
}
// Проверка содержания
bool APIServer::contains(const std::vector<std::string>& array, const std::string& value) const {
    for (std::string element : array)
        if (element == value) 
            return true;
    return false;
}


#endif