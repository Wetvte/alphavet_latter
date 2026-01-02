#ifndef API_SERVER
#define API_SERVER

#include "../include/api_server.h"
#include <algorithm>

using namespace httplib;

APIServer::APIServer() {
    config = ConfigLoader::load("C:\\Users\\Admin\\Desktop\\WetvteGitClone\\alphavet_latter\\TestProcessing\\resources\\test_config.env");
    // ../resources/db_config.env не работает

    using namespace std;
    std::cout << config.get("DB_HOST") << ", " <<
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
void APIServer::start() {
    setupRoutes();
    server.listen("localhost", std::stoi(config.get("API_PORT")));
}
// Токены
Token APIServer::get_request_token(const Request & req) {
    auto header_it = req.headers.find("Authorization");
    if (header_it == req.headers.end()) {
        return Token{}; // заголовок не найден
    }
    std::string auth_header = header_it->second;
    return get_header_token(auth_header);
}
Token APIServer::get_header_token(const std:: string &auth_header) {
    // Проверяем префикс "Bearer "
    const std::string prefix = "Bearer ";
    if (auth_value.size() < prefix.size() ||
        auth_value.compare(0, prefix.size(), prefix) != 0) {
        return Token{}; // неверный формат
    }

    // Извлекаем токен (после префикса)
    std::string token_string = auth_value.substr(prefix.size());
    return TokenDecoder::decode(token_string);
}
// Тело запроса
nlohmann::json APIServer::get_body_json(const std::string &body) {
    nlohmann::json json;
    try {
        json = nlohmann::json::parse(body);
    } catch (const nlohmann::json::exception& e) {}
    return json;
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
            case 503: des = " (База данных не доступна)."; break;
        }
*/
// Методы
void APIServer::write_response(const Response &res, const std::string &message, const int &status = 200) const {
    nlohmann::json json;
    write_response(res, json, message, status);
}
void APIServer::write_response(const Response &res, nlohmann::json json, const std::string &message, const int &status = 200) {
    res.set_header("Content-Type", "application/json");
    res.status = status;
    json["message"] = message;
    res.set_content(json.dump(), "text/plain");
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
        У тестов есть данные о вопросах, которые они содержат, о попытках их прохождения, об авторе)
        У попыток есть данные о содержащихся вопросах, авторе с тестом и ответах
        У ответов только данные о вопросе, на который они ссылаются
        У вопросов нет посторонних ссылок
    */

    /* Методы и права в них:
        Get - /users/list - users:list
        Get - /users/data - user:roles:read, user:disciplines:read, user:tries:read
        Post - /users/fullname - user:fullname:write
        Post - /users/roles - user:roles:write
        Post - /users/status - user:status:write, user:block

        Get - /disciplines/list - discipline:teacherslist, discipline:testslist, discipline:studentslist
        Post - /disciplines/create - discipline:create
        Post - /disciplines/delete - discipline:delete
        Post - /disciplines/text - discipline:text:write
        Post - /disciplines/tests/addnew - test:create, discipline:test:add
        Post - /disciplines/users/add - discipline:student:add, discipline:teacher:add
        Post - /disciplines/users/remove, discipline:user:remove

        Get - /tests/data - test:tries:read
        Post - /tests/status - test:status:write
        Post - /tests/questions/add - test:question:add
        Post - /tests/questions/remove - test:question:remove
        Post - /tests/questions/update - test:question:update
        Get - /tests/tries/user - test:tries:read

        Get - /questions - questions:list
        Get - /questions/data - question:read
        Post - /questions/create - question:create
        Post - /questions/update - question:update
        Post - /questions/delete, question:delete

        Post - /tries/start - tries/answer/change
        Post - /tries/answer/delete
        Post - /tries/stop
        Get - /tries/view - test:tries:read
        
        Post - /news/send - news:write
        Get - /news/list/recipient
        Post - /news/status - news:write
    */

    // Пользователи
    // Возвращает массив содержащий ФИО и ID каждого пользователя зарегистрированного в системе
    server.Get("/users/list", [this](const Request& req, Response& res) {
        // Получение токена
        Token token = get_request_token(req);
        // Проверка токена и разрешений
        if (!token.is_valid()) {
            write_response(res, "Токен не действителен.", 401);
            return;
        }
        if (!token.has_permission("users:list")) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }
        
        // Получает список
        std::vector<DBRow> list = dbConnector.get_rows("users", "id, first_name, last_name, patronimyc", "");
        // Формирует json для списка
        nlohmann::json list_json = nlohmann::json::array();
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
            list_json.push_back(row_json);
        }
        // Отправляет
        write_response(res, list_json, "Список пользователей успешно получен.");
    });
    // Возвращает информацию пользователя по его ID. Возвращается только та информация которую запросили из следующей: список дисциплин, список прохождений тестов (попыток))
    server.Get("/users/data", [this](const Request& req, Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен не действителен.", 401);
            return;
        }
        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("user_id") || !body_json.contains("filter")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }
        std::string filter = body_json.get<std::string>("filter");
        if (filter == "*" || filter == "all") filter = "fullname status roles disciplines tries";
        
        // Айди пользователя
        std::string user_id = body_json["user_id"].get<std::string>();

        // Проверяет наличие разрешений для всех выбранных фильтров, требующих это
        // Роли
        if (filter.find("roles") != std::string::npos) {
            // Само разрешение
            if (!token.has_permission("user:roles:read") ||
            /* Изначально Свои и Чужие нельзя */
            // но Я сделал так, что свои можно, и, если админ, то ну любые то же же да
            (user_id != token.get_user() && !token.has_role("Admin"))) { write_response(res, "Недостаточно прав.", 403); return; }
        }
        // Дисциплины
        if (filter.find("disciplines") != std::string::npos) {
            // Само разрешение
            if (!token.has_permission("user:disciplines:read") ||
            // Можно смотреть дисциалины только себе и админу
            (user_id != token.get_user() && !token.has_role("Admin"))) { write_response(res, "Недостаточно прав.", 403); return; }
        }
        // Попытки прохождения тестов
        if (filter.find("tries") != std::string::npos) {
            // Само разрешение
            if (!token.has_permission("user:tries:read") ||
            // Можно смотреть дисциалины только себе и админу
            (user_id != token.get_user() && !token.has_role("Admin"))) { write_response(res, "Недостаточно прав.", 403); return; }
        }

        // Получает пользователя из БД
        DBRow user_row = dbConnector.get_row("users", "*", "id="+user_id);
        // Собирает json
        nlohmann::json json;
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
            json["roles"] = user_row.get<std::vector<std::string>>("roles");
        }
        if (filter.find("disciplines") != std::string::npos) {
            json["disciplines"] = nlohmann::json::array();
            // Получаем строки данных о дисциплинах с пользователем
            std::vector<DBRow> disciplines_rows =
                dbConnector.get_rows("disciplines", "title, status, teachers", user_id+"=ANY(teachers) OR "+user_id+"=ANY(students)");
            for (DBRow& discipline_row : disciplines_rows) {
                nlohmann::json row_json;
                // Сохраняем в ответ заголовок и статус дисциплины, а также преподавателей.
                row_json["title"] = discipline_row.get<std::string>("title");
                row_json["status"] = discipline_row.get<std::string>("status");
                row_json["teachers"] = discipline_row.get<std::vector<std::string>>("teachers");
                json["disciplines"].push_back(row_json);
            }
        }
        if (filter.find("tries") != std::string::npos) {
            json["tries"] = nlohmann::json::array();
            // Получаем строки данных о попытках
            std::vector<DBRow> tries_rows = dbConnector.get_rows("tries", "*", "author="+user_id);
            for (DBRow& try_row : tries_rows) {
                int try_id = try_row.get<int>("id");
                DBRow test_row = dbConnector.get_row("tests", "id, title, status", try_id+"=ANY(tries)");
                int test_id = test_row.get<int>("id");
                DBRow discipline_row = dbConnector.get_row("disciplines", "id, title, status", test_id+"=ANY(tests)");
                int discipline_id = discipline_row.get<int>("id");
                nlohmann::json row_json;
                // Записываем в ответ данные о тесте и его дисциплине
                row_json["test_id"] = test_id;
                row_json["test_title"] = test_row.get<std::string>("title");
                row_json["test_status"] = test_row.get<std::string>("status");
                row_json["test_discipline_id"] = discipline_id;
                row_json["test_discipline_title"] = discipline_row.get<std::string>("title");
                row_json["test_discipline_status"] = discipline_row.get<std::string>("status");
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
    server.Post("/users/fullname", [this](const Request& req, Response& res) {
        Token token = get_request_token(req);
        // Проверка токена и разрешений
        if (!token.is_valid()) {
            write_response(res, "Токен не действителен.", 401);
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
        
        DBRow settings;
        settings.set("first_name", dbConnector.convert_to_string_format(body_json["first_name"].get<std::string>()));
        settings.set("last_name", dbConnector.convert_to_string_format(body_json["last_name"].get<std::string>())));
        settings.set("patronimyc", dbConnector.convert_to_string_format(body_json["patronimyc"].get<std::string>())); 
        int status = dbConnector.update_rows("users", settings, "id="+user_id);
        if (status == 200) write_response(res, "Успешное обновление ФИО.");
        else write_response(res, "Ошибка при обновлении базы данных.", status);
    });
    // Заменяет роли пользователя на указанные по его ID
    server.Post("/users/roles", [this](const Request& req, Response& res) {
        // Проверка токена и разрешений
        if (!token.is_valid()) {
            write_response(res, "Токен не действителен.", 401);
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
        std::vector<std::string> roles = body_json["roles"].get<std::vector<std::stirng>>();
        // Делаем строку изменений
        DBRow settings;
        settings.set("roles", dbConnector.convert_to_string_format(roles));
        // Обновляем
        int status = dbConnector.update_rows("users", settings, "id="+user_id);
        if (status == 200) write_response(res, "Успешное обновление ролей.");
        /// Успешное обновление ролей синхронизировать с модулем авторизации, в котором, при успехе, удалять access token, чтобы нужно было его обновить для новых прав
        else write_response(res, "Ошибка при обновлении базы данных.", status);
    });
    server.Post("/users/status", [this](const Request& req, Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен не действителен.", 401);
            return;
        }
        // Получение тела проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("user_id") || !body_json.contains("status")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }
        // Проверка разрешений
        std::string taget_status = body_json["status"].get<std::string>();
        if (!token.has_permission("user:status:write") || // не может изменять статус ИЛИ
            (!token.has_permission("user:block") && // не может ставить/снимать блокировку, но пытается
                (taget_status == "Blocked") || taget_status == "Unblocked"))) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }

        // Получаем пользователя
        std::string user_id = body_json["user_id"].get<std::string>();
        DBRow user_row = dbConnector.get_row("users", "status", "id="+user_id);
        if (user_row.is_empty()) {
            write_response(res, "Пользователь не найден.", 404);
            return;
        }

        // Получаем текущий стаутс
        std::string current_status = user_row.get<std::stirng>("status");

        // Нельзя, если не админ и не себе или заблокирован
        if (!token.has_role("Admin") && (user_id != token.get_user() || current_status == "Blocked")) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }

        // Делаем строку изменений
        DBRow settings;
        settings.set("status", taget_status);
        // Обновляем
        int status = dbConnector.update_rows("users", settings, "id="+user_id);
        if (status == 200) write_response(res, "Успешное обновление статуса.");
        else write_response(res, "Ошибка при обновлении базы данных.", status);
    });
    // Дисциплины
    // Возвращает массив содержащий Название, Описание и ID каждой дисциплины зарегистрированной в системе
    server.Get("/disciplines/list", [this](const Request& req, Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен не действителен.", 401);
            return;
        }

        // Получает список
        std::vector<DBRow> list = dbConnector.get_rows("disciplines", "id, title, status", "");
        // Формирует json для списка
        nlohmann::json list_json = nlohmann::json::array();
        for (DBRow& row : list) {
            // Формирует json для отдельной строки
            nlohmann::json row_json;
            row_json["discipline_id"] = row.get<int>("id");
            row_json["title"] = row.get<std::string>("title");
            row_json["status"] = row.get<std::string>("status")
            list_json.push_back(row_json);
        }
        // Отправляет
        write_response(res, list_json, "Успешное получение списка дисциплин.");
    });
    // Для фильтра * возвращает Название, Описание, ID преподавателя для дисциплины по её ID
    server.Get("/disciplines/data", [this](const Request& req, Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен не действителен.", 401);
            return;
        }
        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("discipline_id") || !body_json.contains("filter")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }
        std::string filter = body_json.get<std::string>("filter");
        if (filter == "*" || filter == "all") filter = "text, teacherslist, testslist, studentslist";
        
        // Получение данных о дисциплине
        int discipline_id = body_json["discipline_id"].get<int>();
        DBRow discipline_row = dbConnector.get_row("disciplines", "*", "id="+discipline_id);
        std::vector<std::string> students_ids = discipline_row.get<std::vector<std::string>>("students");
        std::vector<std::string> teachers_ids = discipline_row.get<std::vector<std::string>>("teachers");
        // Проверяет наличие разрешений для всех выбранных фильтров, требующих это
        // Списки // Админу автоматически можно
        if (filter.find("list") != std::string::npos && !token.has_role("Admin"))) {
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
                if (!token.has_permission("discipline:studentslist" || !among_teachers))
                    { write_response(res, "Недостаточно прав.", 403); return; }
            }
            // Если нужны списки преподавателей или тестов, но юзер ваще не в дисциплине
            else if (!among_teachers && !among_students)
                { write_response(res, "Недостаточно прав.", 403); return; }
        }

        // Формируем json
        nlohmann::json json;
        if (filter.find("text") != std::string::npos) {
            json["title"] = discipline.get<std::string>("title");
            json["describtion"] = discipline.get<std::string>("describtion");
        }
        if (filter.find("teacherslist") != std::string::npos) {
            json["teachers"] = nlohmann::json::array();
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
                row_json["id"] = teacher_row.get<std::string>("id");
                row_json["fullname"] = fullname;
                json["teachers"].push_back(row_json);
            }
        }
        if (filter.find("testslist") != std::string::npos) {
            json["tests"] = nlohmann::json::array();
            std::vector<int> tests_ids = discipline.get<std::vector<int>>("tests");
            std::vector<DBRow> tests_rows = dbConnector.get_rows("tests", "id, title", "id IN ("+build_range(tests_ids)+")");
            for (DBRow& test_row : tests_rows) {
                nlohmann::json row_json;
                row_json["id"] = test_row.get<std::string>("id");
                row_json["title"] = test_row.get<std::string>("title");
                json["tests"].push_back(row_json);
            }
        }
        if (filter.find("studentslist") != std::string::npos) {
            json["students"] = nlohmann::json::array();
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
                row_json["id"] = student_row.get<std::string>("id");
                row_json["fullname"] = fullname;
                json["students"].push_back(row_json);
            }
        }
        // Отправляем
        write_response(res, json, "Успешное получение данных о дисциплине.");
    });
    // Создаёт дисциплину с указанным названием, описанием и преподавателем. Как результат возвращает её ID
    server.Post("/disciplines/create", [this](const Request& req, Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен не действителен.", 401);
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
        DBRow teacher_row = dbConnector.get_row("users", "roles", "id="+teacher_id);
        std::vector<std::string> roles = teacherRow.get<std::vector<std::string>>("roles");
        bool is_teacher = contains(roles, "Teacher");
        if (!is_teacher) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }

        // Формирует строку для вставки
        DBRow settings;
        settings.set("title", body_json["title"].get<std::string>());
        settings.set("describtion", body_json["describtion"].get<std::string>());
        settings.set("teachers", "{"+teacher_id+"}");
        DBRow result = dbConnector.insert_row("users", settings);
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
    server.Post("/disciplines/delete", [this](const Request& req, Response& res) {
       Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен не действителен.", 401);
            return;
        }
        if (!token.has_permission("discipline:delete")) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }
        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("discipline_id")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }

        // Получаем айди удаляемой дисциплины
        int discipline_id = body_json["discipline_id"].get<int>();
        // Получаем строку удаляемой дисциплины
        DBRow discipline_row = dbConnector.get_row("disciplines", "teachers", "id="+discipline_id);

        // Админу автоматически можно
        if (!token.has_role("Admin")) {
            // Нельзя, если не первый преподаватель дисциплины
            std::vector<std::string> teachers_ids = discipline_row.get<std::vector<std::string>>("teachers");
            if (teachers_ids[0] != token.get_user()) {
                write_response(res, "Некорректный запрос.", 400);
                return;
            }
        }

        // Создаём строку настроек
        DBRow settings;
        settings.set("status", "Deleted");
        // Обновляем данные
        int status = dbConnector.update_rows("disciplines", settings, "id="+discipline_id);
        /// Удалить все тесты дисциплины
        /// Остановить все попытки всех тестов дисциплины
        if (status == 200) write_response(res, "Успешное удаление дисциплины.");
        else write_response(res, "Ошибка при обновлении базы данных.", status);
    });
    // Заменяет Название и(или) Описание дисциплины на указанные по её ID
    server.Post("/disciplines/text", [this](const Request& req, Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен не действителен.", 401);
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
        int discipline_id = body_json["discipline_id"].get<int>();
        // Получаем строку дисциплины
        DBRow discipline_row = dbConnector.get_row("disciplines", "teachers", "id="+discipline_id);
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
        int status = dbConnector.update_rows("disciplines", settings, "id="+discipline_id);
        if (status == 200) write_response(res, "Успешное обновление информации о дисциплине.");
        else write_response(res, "Ошибка при обновлении базы данных.", status);
    });
    // Добавляет в дисциплину с ID новый тест с указанным названием, пустым списком вопросов и автором и возвращает ID теста. По умолчанию тест не активен.
    server.Post("/disciplines/tests/addnew", [this](const Request& req, Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен не действителен.", 401);
            return;
        }
        if (!token.has_permission("test:create") && !token.has_permission("discipline:test:add")) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }
        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("discipline_id") || !body_json.contains("title")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }

        // Получаем айди дисциплины
        int discipline_id = body_json["discipline_id"].get<int>();
        // Получаем строку дисциплины
        DBRow discipline_row = dbConnector.get_row("disciplines", "teachers, tests", "id="+discipline_id);
        // Нельзя, если не преподаватель дисциплины
        std::vector<std::string> teachers_ids = discipline_row.get<std::vector<std::string>>("teachers");
        bool among_teachers = contains(teachers_ids, token.get_user());
        if (!among_teachers) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }

        // Создаём новый тест
        DBRow test_settings;
        test_settings.set("title", body_json["title"].get<std::string>());
        if (body_json.contains("describtion")) test_settings.set("describtion", body_json["describtion"].get<std::string>());
        DBRow test_row = dbConnector.insert_row("tests", test_settings);
        if (test.is_empty()) {
            write_response(res, "Ошибка при создании теста.", 409);
            return;
        }
        // Формируем изменения для дисциплины
        std::vector<int> tests_ids = discipline.get<std::vector<int>>("tests");
        tests_ids.push_back(test_row.get<int>("id"));
        DBRow discipline_settings;
        discipline_settings.set("tests", db_connector.convert_to_string_format(tests_ids));
        // Обновляем данные
        int status = dbConnector.update_rows("disciplines", discipline_settings, "id="+discipline_id);
        if (status == 200) write_response(res, "Успешное добавление нового теста в дисциплину.");
        else write_response(res, "Ошибка при обновлении базы данных.", status);
    });
    // Добавляет пользователя с указанным ID на дисциплину с указанным ID
    server.Post("/disciplines/users/add", [this](const Request& req, Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен не действителен.", 401);
            return;
        }
        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("user_id") || !body_json.contains("discipline_id") || !body_json.contains("user_type")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }

        // Айди
        int discipline_id = body_json["discipline_id"].get<int>();
        std::string user_id = body_json["user_id"].get<std::string>();

        // Получаем дисциплину
        DBRow discipline_row = dbConnector.get_row("disciplines", "students, teachers", "id="+discipline_id)

        // Создаём настройки для изменения БД
        DBRow discipline_settings;
        // Получаем тип пользователя для добавления
        int user_type = body_json["user_type"].get<std::string>();
        // Проверка разрешений и формировка настроек для соответствующего типа вставки
        if (user_type == "student") {
            // Если нет прав (1), или добавляет не себя (1), или уже есть (2), то низя
            // 1
            if (!token.has_permission("discipline:student:add") || user_id != token.get_user()) {
                write_response(res, "Недостаточно прав.", 403);
                return;
            }
            // 2
            std::vector<std::string> students_ids = discipline_row.get<std::vector<std::string>>("students");
            for (std::string& student_id : students_ids) if (student_id == user_id) {
                write_response(res, "Недостаточно прав.", 403);
                return;
            }

            // Добавление в студенты
            students.push_back(user_id);
            discipline_settings.set("students", dbConnector.convert_to_string_format(students));
        }
        else if (user_type == "teacher") {
            // Если нет прав на добавление преподавателя (1), или владелец токена не первый препод дисциплины (или админ) (2),
            // или уже есть (3), или это не препод (4)
            // 1
            if (!token.has_permission("discipline:teacher:add")) {
                write_response(res, "Недостаточно прав.", 403);
                return;
            }
            // 2
            std::vector<std::string> teachers_ids = discipline_row.get<std::vector<std::string>>("teachers");
            if (!token.has_role("Admin") && token.get_user() != teachers[0]) {
                write_response(res, "Недостаточно прав.", 403);
                return;
            }
            // 3
            for (std::string& teacher_id : teachers_ids) if (teacher_id == user_id) {
                write_response(res, "Недостаточно прав.", 403);
                return;
            }
            // 4
            DBROW user_row = dbConnector.get_row("users", "roles, disciplines", "id="+user_id);
            std::vector<std::string> user_roles = user.get<std::vector<std::string>>("roles");
            bool is_teacher = contains(user_roles, "Teacher");
            if (!is_teacher) {
                write_response(res, "Недостаточно прав.", 403);
                return;
            }

            // Добавление в преподаватели
            teachers.push_back(user_id);
            discipline_settings.set("teachers", dbConnector.convert_to_string_format(teachers_ids));
        }
        else { write_response(res, "Некорректный запрос.", 400); return } // если user_type не соответствует 
        
        int status = dbConnector.insert_row("disciplines", discipline_settings, "id="+discipline_id);
        if (status == 200) write_response(res, "Успешное зачисление пользователя на дисциплину.");
        else write_response(res, "Ошибка при обновлении базы данных.", status);
    });
    // Отчисляет пользователя с указанным ID с дисциплины с указанным ID
    server.Post("/disciplines/users/remove", [this](const Request& req, Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен не действителен.", 401);
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

        // Получаем айди дисциплины
        int discipline_id = body_json["discipline_id"].get<int>();
        // Получаем айди пользователя
        std::string user_id = body_json["user_id"].get<std::string>();

        // Получаем строку дисциплины
        DBRow discipline_row = dbConnector.get_row("disciplines", "teachers, students", "id="+discipline_id);

        // Получаем преподавателей
        std::vector<std::string> teachers_ids = discipline_row.get<std::vector<std::string>>("teachers");
        // Нельзя, если удаляем преподавателя, а их 1
        bool among_teachers = contains(teachers_ids, user_id);
        if (among_teachers && teachers.size() == 1) {
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
        if (token.has_role("Admin") || token.get_user() == user_id || teachers[0] == token.get_user()) {
            // Создаём настройки для изменения БД
            DBRow discipline_settings;

            // Настраиваем дисциплину
            if (among_students) {
                std::erase(students_ids, user_id);
                discipline_settings.set("students", dbConnector.convert_to_string_format(students_ids));
            }
            else  {
                std::erase(teachers_ids, user_id);
                discipline_settings.set("teachers", dbConnector.convert_to_string_format(teachers_ids));
            } 

            // Обновляем записи
            int status = dbConnector.insert_row("disciplines", discipline_settings, "id="+discipline_id);
            if (status == 200) write_response(res, "Пользователь отчислен с дисциплины.");
            else write_response(res, "Ошибка при обновлении базы данных.", status);
        }
        else write_response(res, "Недостаточно прав.", 403);
    });
    // Тесты
    // Возвращает запрощенную информацию о тесте
    server.Get("/tests/data", [this](const Request& req, Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен не действителен.", 401);
            return;
        }
        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("test_id") || !body_json.contains("filter")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }
        std::string filter = body_json.get<std::string>("filter");
        if (filter == "*" || filter == "all") filter = "title describtion status discipline questions tries";
        
        // Айди теста
        int test_id = body_json["test_id"].get<int>();
        // Получает тест из БД
        DBRow test_row = dbConnector.get_row("tests", "*", "id="+test_id);
        
        // Получаем дисциплину
        DBRow discipline_row = dbConnector.get_row("disciplines", "teachers", test_id"=ANY(tests)");

        // Проверяет наличие разрешений для всех выбранных фильтров, требующих это
        // Попытки прохождения
        if (filter.find("tries") != std::string::npos) {
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

        // Берём дисциплину теста
        DBRow discipline_row = dbConnector.get_row("disciplines", "title", test_id+"=ANY(tests)");

        // Собирает json
        nlohmann::json json;
        if (filter.find("title") != std::string::npos) {
            json["title"] = test_row.get<std::string>("title");
        }
        if (filter.find("describtion") != std::string::npos) {
            json["describtion"] = test_row.get<std::string>("describtion");
        }
        if (filter.find("status") != std::string::npos) {
            json["status"] = test_row.get<std::string>("status");
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
                DBRow question_row = dbConnector.get_row("questions", "id, version, title, options, points, max_options_in_answer",
                    "id="+question_signature[0]+" AND vesrion="+question_signature[1]);
                nlohmann::json row_json;
                row_json["id"] = question_row.get<int>("id");
                row_json["version"] = question_row.get<int>("version");
                row_json["title"] = question_row.get<std::string>("title");
                row_json["options"] = question_row.get<std::vector<std::string>>("options");
                row_json["points"] = question_row.get<std::vector<int>>("points");
                row_json["max_options_in_answer "] = question_row.get<int>("max_options_in_answer");
                json["questions"].push_back(row_json);
            }
        }
        if (filter.find("tries") != std::string::npos) {
            json["tries"] = nlohmann::json::array();
            // Получаем строки данных о попытках
            std::vector<int> tries_ids = test_row.get<std::vector<int>>("tries");
            std::vector<DBRow> tries_rows = dbConnector.get_rows("tries", "*", "id IN("+build_range(tries_ids)+")");
            for (DBRow& try_row : tries_rows) {
                nlohmann::json row_json;
                row_json["status"] = try_row.get<std::string>("status");
                row_json["points"] = try_row.get<int>("points");
                row_json["max_points"] = try_row.get<int>("max_points");
                row_json["score_percent"] = try_row.get<int>("score_percent");
                json["tries"].push_back(row_json);
            }
        }
        // Отправляет ответ
        write_response(res, json, "Информация о тесте успешно получена.");
    });
    // Для теста с указанным ID устанавливает значение активности. 
    server.Post("/tests/status", [this](const Request& req, Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен не действителен.", 401);
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
        int test_id = body_json["test_id"].get<int>();
        std::string target_status = body_json["status"].get<std::string>();
        // Проверка корректного статуса
        if (taget_status != "Activated" && target_status != "Deactivated" && target_status != "Deleted") {
            write_response(res, "Некорректный запрос.", 400);
            return;
            // Статус Deactivated
            // Все начатые попытки автоматически отмечаются завершёнными.
            // Статус Removed 
            // Отмечает тест как удалённый (реально ничего не удаляется). Все оценки перестают отображаться, но тоже не удаляются.
        }

        // Получаем строку теста
        DBRow test_row = dbConnector.get_row("tests", "author, status, tries", "id="+test_id);
        std::string author = test_row.get<std::string>("author");
        std::stirng current_status = test_row.get<std::string>("status");

        // Нельзя сменить на такой же статус
        if (current_status == target_status)) {
            write_response(res, 406);
            return;
        }
        // Нельзя тому, кто не препод дисциплины (2), или не админ (1)
        if (!token.has_role("Admin")) { // 1
            // Получаем строку дисциплины
            DBRow discipline_row = dbConnector.get_row("disciplines", "teachers", test_id+"=ANY(tests)");
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
        int status = dbConnector.update_rows("tests", settings, "id="+test_id);

        // Если статус обновился и теперь явно не активный, заканчивает попытки
        if (status == 200 && target_status != "Activated") {
            // Получаем попытки, которые не закончены и для этого теста
            std::vector<int> tries_ids = test_row.get<std::vector<int>>("tries");
            std::vector<DBRow> tries_rows = dbConnector.get_rows("tries", "id", "status='Solving' AND id IN("+build_range(tries_ids)+")");
            // Заполняем решённые
            std::vector<int> solving_tries;
            for (DBRow& try_row : tries_rows)
                solving_tries.push_back(try_row.get<int>("id"));
            // Создаём настройку статуса
            DBRow settings;
            settings.set("status", "Solved");
            // Ставим для всех
            status = dbConnector.update_rows("tries", setting, "id IN ("+build_range(solving_tries)+")")
        }
        
        if (status == 200) write_response(res, "Успешное обновление статуса теста.");
        else write_response(res, "Ошибка при обновлении базы данных.", status);
    });
    // Если у теста ещё не было попыток прохождения, то добавляет в теста с указанным ID вопрос с указанным ID в последнюю позицию
    server.Post("/tests/questions/add", [this](const Request& req, Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен не действителен.", 401);
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
        int test_id = body_json["test_id"].get<int>();
        int question_id =  body_json["question_id"].get<int>();

        // Получаем строку теста
        DBRow test_row = dbConnector.get_row("tests", "status, tries, questions_signatures", "id="+test_id);
        // Проверяем существование теста
        if (test_row.is_empty()) {
            write_response(res, "Ресурс не найден.", 404);
            return;
        }
        // Проверяем на то, есть ли попытки прохождения
        std::vector<int> tries = test_row.get<std::vector<int>>("tries");
        if (tries.size() > 0) {
            write_response(res, 406);
            return;
        }

        // Получаем строку дисциплины теста
        DBRow discipline_row = dbConnector.get_row("disciplines", "teachers", test_id+"=ANY(tests)");
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
            ("id="+question_id+" AND version="+body_json.contains("question_version")) :
            // Получаем последнюю версию версию, если она не указана в запросе.
            ("version=(SELECT MAX(Version) FROM questions WHERE id="+question_id+")"));
        // Проверяем наличие добавляемого вопроса впринципе
        if (question_row.is_empty()) {
            write_response(res, "Ресурс не найден.", 404);
            return;
        }

        std::string test_status = test_row.get<std::string>("status");
        // Добавляем
        DBRow settings;
        std::vector<std::vector<int>> questions_signatures = test_row.get<std::vector<std::vector<int>>>("questions_signatures");
        std::vector<int> new_signature;
        new_signature.push_back(question_row.get<int>("id"));
        new_signature.push_back(question_row.get<int>("version"));
        questions_signatures.push_back(new_signature);
        if (test_status == "Activated") settings.set("status", "Deactivated"); // Если тест активен, деактивируем
        settings.set("questions_signatures", dbConnector.convert_to_string_format(questions_signatures));
        int status = dbConnector.update_rows("tests", settings, "id="+test_id);
        if (status == 200) write_response(res, "Успешное добавление вопроса в тест.");
        else write_response(res, "Ошибка при обновлении базы данных.", status);
    });
    // Если у теста ещё не было попыток прохождения, то удаляет у теста с указанным ID вопрос с указанным номером
    server.Post("/tests/questions/remove", [this](const Request& req, Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен не действителен.", 401);
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
        int test_id = body_json["test_id"].get<int>();
        int question_number =  body_json["question_number"].get<int>();

        // Получаем строку теста
        DBRow test_row = dbConnector.get_row("tests", "tries, questions_signatures", "id="+test_id);
        // Проверяем существование теста
        if (test_row.is_empty()) {
            write_response(res, "Ресурс не найден.", 404);
            return;
        }
        // Проверяем на то, есть ли попытки прохождения
        std::vector<int> tries_ids = test_row.get<std::vector<int>>("tries");
        if (tries_ids.size() > 0) {
            write_response(res, 406);
            return;
        }

        // Получаем строку дисциплины теста
        DBRow discipline_row = dbConnector.get_row("disciplines", "teachers", test_id+"=ANY(tests)");
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
            write_response(res, 406);
            return;
        }
        // Удаляем сигнатуру
        questions_signatures.erase(questions_signatures.begin() + question_number - 1);
        // Обновляем запись
        DBRow settings;
        settings.set("questions_signatures", dbConnector.convert_to_string_format(questions_signatures));
        int status = dbConnector.update_rows("tests", settings, "id="+test_id);
        if (status == 200) write_response(res, "Успешное удаление вопроса из теста.");
        else write_response(res, "Ошибка при обновлении базы данных.", status);
    });
    // Если у теста ещё не было попыток прохождения, то для теста с указанным ID устанавливает указанную последовательность вопросов
    server.Post("/tests/questions/update", [this](const Request& req, Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен не действителен.", 401);
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
        int test_id = body_json["test_id"].get<int>();

        // Получаем строку теста
        DBRow test_row = dbConnector.get_row("tests", "tries", "id="+test_id);
        // Проверяем существование теста
        if (test_row.is_empty()) {
            write_response(res, "Ресурс не найден.", 404);
            return;
        }
        // Проверяем на то, есть ли попытки прохождения
        std::vector<int> tries = test_row.get<std::vector<int>>("tries");
        if (tries.size() > 0) {
            write_response(res, 406);
            return;
        }

        // Получаем строку дисциплины теста
        DBRow discipline_row = dbConnector.get_row("disciplines", "teachers", test_id+"=ANY(tests)");
        // Получаем преподавателей
        std::vector<std::string> teachers = discipline_row.get<std::vector<std::string>>("teachers");
        // Проверяем, что автор запроса - преподаватель дисциплины, к которой привязан тест
        bool among_teachers = contains(teachers_ids, token.get_user());
        if (!among_teachers) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }

        // Получаем сигнатуры
        std::vector<std::vector<int>> questions_signatures =  body_json["questions_signatures"].get<std::vector<std::vector<int>>>();
        // Обновляем запись
        DBRow settings;
        settings.set("questions_signatures", dbConnector.convert_to_string_format(questions_signatures));
        int status = dbConnector.update_rows("tests", settings, "id="+test_id);
        if (status == 200) write_response(res, "Успешное обновление вопросов для теста.");
        else write_response(res, "Ошибка при обновлении базы данных.", status);
    });
    // Для теста с указанным ID выбирает все попытки пользователя с определённым ID
    server.Get("/tests/tries/user", [this](const Request& req, Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен не действителен.", 401);
            return;
        }
        // Нельзя без разрешений
        if (!token.has_permission("test:tries:read")) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }
        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("test_id") || !body_json.contains("user_id")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }
        
        // Получаем айди теста и пользователя
        int test_id = body_json["test_id"].get<int>();
        int user_id = body_json["user_id"].get<int>();

        // Получаем строку теста
        DBRow test_row = dbConnector.get_row("tests", "tries", "id="+test_id);
        // Проверяем существование теста
        if (test_row.is_empty()) {
            write_response(res, "Тест не найден.", 404);
            return;
        }

        // Можно, если админ, автор попыток, которые хочет получить
        if (!token.has_role("Admin") && user_id != token.get_user()) {
            // Получаем строку дисциплины теста
            DBRow discipline_row = dbConnector.get_row("disciplines", "teachers", test_id+"=ANY(tests)");
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
        nlohmann::json json = nlohmann::json::array();
        // Получаем строки данных о попытках
        std::vector<DBRow> tries_rows = dbConnector.get_rows("tries", "*", "author="+user_id+" AND test"+test_id);
        for (DBRow& try_row : tries_rows) {
            nlohmann::json row_json;
            row_json["status"] = try_row.get<std::string>("status");
            row_json["points"] = try_row.get<int>("points");
            row_json["max_points"] = try_row.get<int>("max_points");
            row_json["score_percent"] = try_row.get<int>("score_percent");
            json.push_back(row_json);
        }
        write_response(res, json, "Успешное получение данных о попытке пользователя.");
    });
    // Вопросы
    // Возвращает массив содержащий Название вопроса, его версию и ID автора для каждого теста в системе. Если у вопроса есть несколько версий, показывается только последняя
    server.Get("/questions", [this](const Request& req, Response& res) {
        // Получение токена
        Token token = get_request_token(req);
        // Проверка токена и разрешений
        if (!token.is_valid()) {
            write_response(res, "Токен не действителен.", 401);
            return;
        }
        if (!token.has_permission("questions:list")) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }
        
        // Получает список
        std::vector<DBRow> list = dbConnector.get_rows("questions", "id, version, title", "");
        // Формирует json для списка
        nlohmann::json list_json = nlohmann::json::array();
        for (auto& row : list) {
            // Формирует json для отдельной строки
            nlohmann::json row_json;
            std::string first = ;
            std::string last = row.get<std::string>("last_name");
            std::string patronimyc = row.get<std::string>("patronimyc");
            std::string fullname = build_fullname(first, last, patronimyc);
            row_json["id"] = row.get<int>("id");
            row_json["version"] = row.get<int>("version");
            row_json["title"] = row.get<std::string>("title");
            list_json.push_back(row_json);
        }
        // Отправляет
        write_response(res, list_json, "Успешное получение списка вопросов.");
    });
    // Для указанного ID вопроса и версии возвращает Название (Текст вопроса), Варианты ответов, Номер правильного ответа
    server.Get("/questions/data", [this](const Request& req, Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен не действителен.", 401);
            return;
        }
        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("question_id") || !body_json.contains("filter")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }
        std::string filter = body_json.get<std::string>("filter");
        if (filter == "*" || filter == "all") filter = "title status options points author";
        
        // Айди теста
        int question_id = body_json["question_id"].get<int>();
        
        // Получает вопрос из БД
        DBRow question_row = dbConnector.get_row("questions", "*", body_json.contains("question_version") ?
            // Если нужная версия указана, получаем её
            ("id="+question_id+" AND version="+body_json.contains("question_version")) :
            // Получаем последнюю версию версию, если она не указана в запросе.
            ("version=(SELECT MAX(Version) FROM questions WHERE id="+question_id+")"));

        // Проверяем существование
        if (question_row.is_empty()) {
            write_response(res, "Вопрос не найден.", 404);
            return;
        }

        // Проверяет наличие разрешений для всех выбранных фильтров, требующих это
        // Данные о вопросе
        if (filter.find("options") != std::string::npos || filter.find("points") != std::string::npos) {
            // Само разрешение
            if (!token.has_permission("question:read")) { write_response(res, "Недостаточно прав.", 403); return; }
            // Можно смотреть, если админу
            if (!token.has_role("Admin")) {
                // Проверяем наличие в преподавателях автора запроса // Можно преподавателю дисциплины
                std::vector<std::string> teachers = discipline_row.get<std::vector<std::string>>("teachers");
                bool among_teachers = contains(teachers_ids, token.get_user());
                if (!among_teachers) { write_response(res, "Недостаточно прав.", 403); return; }
            }
        }

        // Собирает json
        nlohmann::json json;
        if (filter.find("title") != std::string::npos) {
            json["title"] = question_row.get<std::string>("title");
        }
        if (filter.find("status") != std::string::npos) {
            json["status"] = question_row.get<std::string>("status");
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
        // Отправляет ответ
        write_response(res, json, "Успешное получение данных о вопросе.");
    });
    // Создаёт новый вопрос с заданным Названием, Тексом вопроса, Вариантами ответов, Номером правильного ответа. Версия вопроса 1. В качестве ответа возвращается ID вопроса
    server.Post("/questions/create", [this](const Request& req, Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен не действителен.", 401);
            return;
        }
        if (!token.has_permission("question:create")) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }

        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("title") || !body_json.contains("options") || !body_json.contains("points")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }
        
        // Добавляет
        DBRow settings;
        settings.set("title", body_json["title"].get<std::string>());
        settings.set("describtion", body_json["describtion"].get<std::string>());
        settings.set("options", dbConnector.convert_to_string_format(body_json["options"].get<std::vector<std::string>>()));
        settings.set("points", dbConnector.convert_to_string_format(body_json["points]"].get<std::vector<int>>()));
        settings.set("author", token.get_user());
        DBRow question_row = dbConnector.insert_row("questions", settings);
        if (question_row.is_empty()) {
            write_response(res, "Ошибка при создании вопроса.", 406);
            return;
        }
        write_response(res, "Успешное создание вопроса.");
    });   
    // Для указанного ID вопроса создаёт новую версию с заданным Названием, Тексом вопроса, Вариантами ответов, Номером правильного ответа
    server.Post("/questions/update", [this](const Request& req, Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен не действителен.", 401);
            return;
        }
        if (!token.has_permission("question:update")) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }

        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("question_id") ||
            !body_json.contains("title") || !body_json.contains("options") || !body_json.contains("points")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }
        
        // Добавляет
        DBRow settings;
        settings.set("id", dbConnector.convert_to_string_format(body_json["question_id"].get<int>()));
        settings.set("title", body_json["title"].get<std::string>());
        settings.set("describtion", body_json["describtion"].get<std::string>());
        settings.set("options", dbConnector.convert_to_string_format(body_json["options"].get<std::vector<std::string>>()));
        settings.set("points", dbConnector.convert_to_string_format(body_json["points]"].get<std::vector<int>>()));
        settings.set("author", token.get_user());
        DBRow question_row = dbConnector.insert_row("questions", settings);
        if (question_row.is_empty()) {
            write_response(res, "Ошибка при создании новой версии вопроса.", 406);
            return;
        }
        write_response(res, "Успешное создание новой версии вопроса.");
    });
    // Если вопрос не используется в тестах (даже удалённых), то вопрос отмечается как удалённый (но реально не удаляется)
    server.Post("/questions/delete", [this](const Request& req, Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен не действителен.", 401);
            return;
        }
        if (!token.has_permission("question:delete")) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }

        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("question_id")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }

        // Получаем строку вопроса
        int question_id = body_json["question_id"].get<int>();
        DBRow question_row = dbConnector.get_row("questions", "status, author", "id="+question_id);
        // Проверяем существование
        if (question_row.is_empty()) {
            write_response(res, "Вопрос не найден.", 404);
            return;
        }
        // Проверяем статус
        std::string current_status = question_row.get<std::string>("status");
        if (current_status == "Deleted") {
            write_response(res, "Вопрос уже был удалён.", 403);
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
        settings.set("status", "Deleted");
        int status = dbConnector.update_rows("questions", settings, "id="+question_id);
        if (status == 200) write_response(res, "Успешное удаление вопроса.");
        else write_response(res, "Ошибка при удалении вопроса.", status);
    });
    // Попытки
    // Если пользователь с ID ещё не отвечал на тест с ID и тест находится в активном состоянии,
    // то создаётся новая попытка и возвращается её ID, А ТАКЖЕ ВСЕ ВОПРОСЫ И БАЛЛЫ ЗА ОТВЕТЫ
    server.Post("/tries/start", [this](const Request& req, Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен не действителен.", 401);
            return;
        }
        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("test_id")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }

        // Айди теста
        int test_id = body_json["test_id"].get<int>();
        // Получает тест из БД
        DBRow test_row = dbConnector.get_row("tests", "*", "id="+question_id);

        // Можно начать, если админу
        if (!token.has_role("Admin")) {
            // Проверяем наличие в преподавателях автора запроса // Можно преподавателю дисциплины
            std::vector<std::string> teachers_ids = discipline_row.get<std::vector<std::string>>("teachers");
            bool among_teachers = contains(teachers_ids, token.get_user());
            if (!among_teachers) {
                // Проверяем наличие в студентах автора запроса // Можно студенту дисциплины (1), если попытки не исчерпаны (2)
                // 1
                std::vector<std::string> students_ids = discipline_row.get<std::vector<std::string>>("Student");
                bool among_students = contains(students_ids, token.get_user());
                if (!among_students) { write_response(res, "Недостаточно прав.", 403); return; }
                // 2
                int max_tries_count = test_row.get<int>("MaxTriesForStudent");
                std::vector<int> tries_ids = test_row.get<std::vector<int>>("tries"); // айди попыток, принадлежащих тесту
                int current_tries_count =
                    dbConnector.get_rows("tries", "id", "author="+token.get_user()+ // Получаем строки попыток с нужным автором,
                        " AND status='Solved' AND id IN("+build_range(tries_ids)+")") // статусом решённой и принадлежащими тесту
                            .size(); // кол-во подошедших строк
                if (current_tries_count >= max_tries_count) { write_response(res, "Недостаточно прав.", 403); return; }
            }

        }
        
        // Создаём попытку
        // Сама настройка
        DBRow try_settings;
        // Статус и автор
        try_settings.set("status", "Solving");
        try_settings.set("author", token.get_user());
        // Ответы
        std::vector<int> answers_ids;
        std::vector<std::vector<int>> questions_signatures = test_row.get<std::vector<std::vector<int>>("questions_signatures");
        for (std::vector<int>& question_signature : questions_signatures) {
            // Создаёт строку ответа для каждого вопроса теста
            DBRow answer_settings;
            answer_settings.set("question_signature", dbConnector.convert_to_string_format(question_signature));
            answer_row.set("author", token.get_user());
            DBRow answer_row = dbConnector.insert_row("answers", answer_settings);
            if (answer_row.is_empty()) {
                write_response(res, "Ошибка создания попытки для прохождения теста.", 406);
                return;
            }
            answers_ids.push_back(answer_row.get<int>("id"));
        }
        try_settings.set("answers", dbConnector.convert_to_string_format(answers_ids));
        // Добавляем запись
        DBRow try_row = dbConnector.insert_row("tries", answer_settings);
        if (try_row.is_empty()) {
            write_response(res, "Ошибка создания попытки для прохождения теста.", 406);
            return;
        }

        // Формурем json ответ, получая данные для каждого вопроса.
        nlohmann::json json;
        json["try_id"] = try_row.get<int>("id");
        json["questions"] = nlohmann::json::array();
        for (std::vector<int>& question_signature : questions_signatures) {
            DBRow question_row = dbConnector.get_row("questions", "*", "id="+question_signature[0]+" AND version="+question_signature[1]);
            nlohmann::json row_json;
            row_json["id"] = question_row.get<int>("id");
            row_json["version"] = question_row.get<int>("version");
            row_json["title"] = question_row.get<std::string>("title");
            row_json["options"] = question_row.get<std::vector<std::string>>("options");
            row_json["points"] = question_row.get<std::vector<int>>("points");
        }
        write_response(res, json, "Попытка начата.");
    });
    // Если тест находится в активном состоянии и пользователь ещё не закончил попытку, то для попытки с ID изменяет значение ответа с ID
    server.Post("/tries/answer/change", [this](const Request& req, Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен не действителен.", 401);
            return;
        }
        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("try_id") || !body_json.contains("question_number") || !body_json.contains("answer_options")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }

        // Получить попытку
        int try_id = body_json["try_id"];
        DBRow try_row = dbConnector.get_row("tries", "*", "id="+try_id);
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

        // Получаем строку попытки
        DBRow try_row = dbConnector.get_row("tries", "status, answers", "id="+try_id)
        // Нельзя, если попытка не существует, или окончена
        if (try_row.is_empty()) {
            write_response(res, "Ресурс не найден.", 404);
            return;
        }
        if (try_row.get<std::string>("status") != "Solving") {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }

        // Получаем ответы
        std::vector<int> answers_ids = try_row.get<std::vector<int>>("answers");
        // Проверяем существование номера
        int question_number = body_json["question_number"].get<int>();
        if (answers_ids.size() > question_number) {
            write_response(res, "Ресурс не найден.", 404);
            return;
        }
        int answer_id = answers_ids[question_number-1];
        // Меняем значение ответа
        std::vector<int> answer_options = body_json["answer_options"];
        DBRow settings;
        settings.set("options", dbConnector.convert_to_string_format(answer_options));
        int status = dbConnector.update_rows("answers", settings, "id="+answer_id);
        if (status == 200) write_response(res, "Успешное изменение ответа.");
        else write_response(res, "Ошибка при обновлении базы данных.", status);
    });
    // Если попытка которой принадлежит ответ не завершена, то изменяет индекс варианта ответа на -1.
    server.Post("/tries/answer/delete", [this](const Request& req, Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен не действителен.", 401);
            return;
        }
        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("try_id") || !body_json.contains("question_number")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }

        // Получить попытку
        int try_id = body_json["try_id"];
        DBRow try_row = dbConnector.get_row("tries", "*", "id="+try_id);
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

        // Получаем строку попытки
        DBRow try_row = dbConnector.get_row("tries", "status, answers", "id="+try_id)
        // Нельзя, если попытка не существует, или окончена
        if (try_row.is_empty()) {
            write_response(res, "Ресурс не найден.", 404);
            return;
        }
        if (try_row.get<std::string>("status") != "Solving") {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }

        // Получаем ответы
        std::vector<int> answers_ids = try_row.get<std::vector<int>>("answers");
        // Проверяем существование номера
        int question_number = body_json["question_number"].get<int>();
        if (answers_ids.size() > question_number) {
            write_response(res, "Ресурс не найден.", 404);
            return;
        }
        int answer_id = answers_ids[question_number-1];
        // Меняем значение ответа
        std::vector<int> answer_options = {-1};
        DBRow settings;
        settings.set("options", dbConnector.convert_to_string_format(answer_options));
        int status = dbConnector.update_rows("answers", settings, "id="+answer_id);
        if (status == 200) write_response(res, "Успешное удаление ответа.");
        else write_response(res, "Ошибка при обновлении базы данных.", status);
    });
    // Если тест находится в активном состоянии и пользователь ещё не закончил попытку, то устанавливает попытку в состояние: завершено. Если тест переключили в состояние не активный, то все попытки для него автоматически устанавливаются в состояние: завершено
    server.Post("/tries/stop", [this](const Request& req, Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен не действителен.", 401);
            return;
        }
        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("try_id") || !body_json.contains("question_number")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }

        // Получить попытку
        int try_id = body_json["try_id"];
        DBRow try_row = dbConnector.get_row("tries", "*", "id="+try_id);
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

        // Получаем строку попытки
        DBRow try_row = dbConnector.get_row("tries", "status, answers", "id="+try_id)
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
        int status = dbConnector.update_rows("tries", settings, "id="+try_id);
        if (status == 200) write_response(res, "Успешная остановка попытки.");
        else write_response(res, "Ошибка при обновлении базы данных.", status);
    });
    // Для пользователя с ID и теста с ID возвращается массив ответов и статус состояние попытки
    server.Get("/tries/view", [this](const Request& req, Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен не действителен.", 401);
            return;
        }
        // Нельзя без разрешений
        if (!token.has_permission("test:tries:read")) {
            write_response(res, "Недостаточно прав.", 403);
            return;
        }
        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("try_id")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }
        
        // Получаем айди попытки
        int try_id = body_json["try_id"].get<int>();

        // Получаем строку попытки
        DBRow try_row = dbConnector.get_row("tries", "*", "id="+try_id);
        // Проверяем существование 
        if (try_row.is_empty()) {
            write_response(res, "Ресурс не найден.", 404);
            return;
        }

        // Можно, если админ или автор попытки, которую хочет получить
        if (!token.has_role("Admin") && user_id != token.get_user()) {
            // Получаем строку дисциплины теста
            DBRow discipline_row = dbConnector.get_row("disciplines", "teachers", test_id+"=ANY(tests)");
            // Получаем преподавателей
            std::vector<std::string> teachers = discipline_row.get<std::vector<std::string>>("teachers");
            // Проверяем, что автор запроса - преподаватель дисциплины, к которой привязан тест
            bool among_teachers = contains(teachers_ids, token.get_user());
            if (!among_teachers) {
                write_response(res, "Недостаточно прав.", 403);
                return;
            }
        }

        // Формируем json для ответа
        nlohmann::json json = nlohmann::json::array();
        // Получаем строку данных о попытке
        DBRow try_row = dbConnector.get_row("tries", "*", "id="+try_id);
        // Вставляем основные даныне
        json["status"] = try_row.get<std::string>("status");
        json["points"] = try_row.get<int>("points");
        json["max_points"] = try_row.get<int>("max_points");
        json["score_percent"] = try_row.get<int>("score_percent");
        // Приводим ответы
        json["answers"] = nlohmann::json::array();
        std::vector<int> answers = try_row.get<std::vector<int>>("answers");
        std::vector<DBRow> answers_rows = dbConnector.get_rows("answers", "*", "id IN ("+build_range(answers)+")");
        for (DBRow& answer_row : answers_rows) {
            std::vector<int> question_signature = answer_row.get<std::vector<int>>("question_signature")
            DBRow question_row = dbConnector.get_row("questions", "*", "id="+question_signature[0]+" AND version="+question_signature[1]);
            nlohmann::json row_json;
            row_json["question_id"] = question_row.get<int>("id");
            row_json["question_version"] = question_row.get<int>("version");
            row_json["question_title"] = question_row.get<std::string>("title");
            row_json["question_options"] = question_row.get<std::vector<std::string>>("options");
            row_json["selected_options"] = answer_row.get<std::vector<int>>("options");
            row_json["points"] = answer_row.get<int>("points");
        }
        // Отправляем
        write_response(res, json, "Успешное получение данных о попытке.");
    });
    // Отправляет сообщение с text пользователю с указанным айди
    server.Post("/news/send", [this](const Request& req, Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен не действителен.", 401);
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
        if (user_id == token.get_user()) {
            write_response(res, "Нельзя отправлять самому себе.", 406);
            return;
        }

        // Получаем строку попытки
        DBRow user_row = dbConnector.get_row("users", "id", "id="+user_id);
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
    server.Get("/news/list/recipient", [this](const Request& req, Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен не действителен.", 401);
            return;
        }
        // Получение тела, проверка
        nlohmann::json body_json = get_body_json(req.body);
        if (!body_json.contains("user_id")) {
            write_response(res, "Некорректный запрос.", 400);
            return;
        }
        
        // Получаем айди получателя
        std::string user_id = body_json["user_id"].get<std::string>();
        // Нельзя себе
        if (user_id == token.get_user()) {
            write_response(res, "Пользователь не найден.", 404);
            return;
        }

        // Можно только админу и себе
        if (token.get_user() != user_id && !token.has_role("Admin")) {
            write_response(res, "Нельзя смотреть адресованное другим.", 403);
            return;
        }

        // Получаем строки сообщений
        std::vector<DBRow> news_rows = dbConnector.get_rows("news", "*", "status <> 'Deleted AND recipient="+user_id);
        // Формируем json
        nlohmann::json json = nlohmann::json::array();
        for (DBRow& news_row : news_rows) {
            nlohmann::json row_json;
            row_json["id"] = news_row.get<int>("id");
            row_json["status"] = news_row.get<std::string>("status");
            row_json["text"] = news_row.get<std::string>("text");
            std::string sender_id = news_row.get<std::string>("sender");
            DBRow sender_row = dbConnector.get_row("users", "id, first_name, last_name, patronimyc", "id="+sender_id);
            std::string fullname;
            if (sender_row.is_empty())
                fullname = "Неизвестно";
            else {
                std::string first = sender_row.get<std::string>("first_name");
                std::string last = sender_row.get<std::string>("last_name");
                std::string patronimyc = student_row.get<std::string>("patronimyc");
                fullname = build_fullname(first, last, patronimyc);
            }
            // Добавляет в json
            row_json["sender_id"] = sender_id;
            row_json["sender_fullname"] = fullname;
        }
        
        write_response(res, json, "Сообщения успешно получены.");
    });
    // Меняет статус новости
    server.Post("/news/status", [this](const Request& req, Response& res) {
        Token token = get_request_token(req);
        // Проверка токена
        if (!token.is_valid()) {
            write_response(res, "Токен не действителен.", 401);
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
        
        // Получаем айди новости
        int news_id = body_json["news_id"].get<int>();
        // Нельзя себе
        if (user_id == token.get_user()) {
            write_response(res, "Нельзя отправлять самому себе.", 406);
            return;
        }

        // Получаем строку новости
        DBRow news_row = dbConnector.get_row("news", "status, sender, recipient", "id="+user_id);
        // Проверяем существование 
        if (news_row.is_empty()) {
            write_response(res, "Получатель не найден.", 404);
            return;
        }

        // Получает статус, отправителя и получателя
        std::string target_status = body_json["status"].get<std::string>();
        std::string current_status = news_row.get<std::string>("status");
        std::string sender_id = news_row.get<std::string>("sender");
        std::string recipient_id = news_row.get<std::string>("recipient");

        // Нельзя, если статус уже такой
        if (current_status == taget_status) {
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
        int status = dbConnector.update_rows("news", settings, "id="+news_id);
        if (status == 200) write_response(res, "Статус сообщения изменён.");
        else write_response(res, "Ошибка при изменении статуса сообщения.", status);
    });
}

// Вспомогательные методы (не БД)
// Собирание имени
std::string APIServer::build_fullname(const std::string& first_name = "", const std::string& last_name = "", const std::string& patronymic = "") {
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
    std::string range = ""
    for (int& id : ids) {
        if (range != "") range += ", ";
        range += to_string(id);
    }
    return range;
}
std::string APIServer::build_range(const std::vector<std::string> &ids) {
    std::string range = ""
    for (std::string& id : ids) {
        if (range != "") range += ", ";
        range += to_string(id);
    }
    return range;
}
// Проверка содержания
bool APIServer::contains(const std::vector<std::string>& array, const std::string& value) const {
    for (std::string& element : array)
        if (element == value) 
            return true;
    return false;
}


#endif