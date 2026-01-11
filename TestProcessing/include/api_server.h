#ifndef API_SERVER_H
#define API_SERVER_H

#include "httplib.h"
#include "db_connector.h"
#include "config_loader.h"
#include "token_decoder.h"
#include <tuple>
#include <atomic>
#include <optional>

struct HttpResponse {
    nlohmann::json body;
    int status;
};

class APIServer {
public:
    APIServer();

    void connect_to_db();
    void start();
    void shutdown();

    static APIServer* instance;

private:
    Config config;
    DBConnector dbConnector;
    httplib::Server server;
    std::atomic<bool> shutdowned{false};

    void setup_routes();
    Token get_request_token(const httplib::Request &req);
    Token get_header_token(const std::string &header);
    nlohmann::json get_body_json(const std::string &body);
    nlohmann::json get_query_json(const std::string &query);
    nlohmann::json get_params_json(const httplib::Params& params);
    void write_response(httplib::Response &res, const std::string &message, const int &status = 200);
    void write_response(httplib::Response &res, nlohmann::json json, const std::string &message, const int &status = 200);
    
    // Собственные запросы
    std::optional<HttpResponse> DoGetRequestToService(const std::string& host, const int& port, const std::string& source,
        const std::string &token_prefix, const std::string &token);
    std::optional<HttpResponse> DoPostRequestToService(const std::string& host, const int& port, const std::string& source,
        const std::string &token_prefix, const std::string &token, const nlohmann::json& body);

    // Вспомогательные методы
    std::string build_fullname(const std::string& first_name="", const std::string& last_name="", const std::string& patronymic="");
    std::string build_range(const std::vector<std::string> &ids);
    std::string build_range(const std::vector<int> &ids);
    bool contains(const std::vector<std::string>& array, const std::string& value) const;

    /*
    На помойку (ваще не пригодилось, всё описано в routes)

    std::vector<DBConnector::Row> get_users_list(); // Возвращает массив строк, содержащих ФИО и ID каждого пользователя зарегистрированного в системе
    std::string get_user_fullname(const std::string &id); // Возвращает ФИО пользователя по его ID
    std::vector<std::string> get_user_roles(const std::string &id); // Возвращает роли пользователя по его ID
    std::string get_user_status(const std::string &id); // Возвращает статус пользователя по его ID
    DBRow get_user_info(const std::string &id); // Возвращает строку информации пользователя по его ID. По умолчанию список дисциплин, список тестов, список оценок.
    int change_user_fullname(const std::string &id,
        const std::string &first_name, const std::string &last_name, const std::string &patronimyc); // Заменяет ФИО пользователя на указанное по его ID
    int change_user_roles(const std::string &id, const std::vector<std::string> &roles); // Заменяет все роли пользователя на указанные по его ID
    int change_user_status(const std::string &id, const std::string &status); // Заменяет статус пользователя на указанное по его ID
    int change_user_info(const std::string &id, const DBRow &changes) // Заменяет имеющиеся в строке параметры переданными для пользователя с ID

    int create_discipline(const std::string &title, const std::string &describtion, const std::vector<std::string> &teachersIds); // Создаёт дисциплину с указанным названием, описанием и преподавателем. Как результат возвращает её ID
    std::vector<DBConnector::Row> get_disciplines_list(); // Возвращает массив строк содержащий Название, Описание и ID каждой дисциплины зарегистрированной в системе
    std::vector<DBConnector::Row> get_discipline_tests(const int &id); // Возвращает массив содержащий Название и ID для каждого теста присутствующего в дисциплине по её ID
    std::vector<std::string> get_discipline_students(const int &id); // Возвращает массив содержащий ID каждого студента записанного на дисциплину по её ID
    DBRow get_discipline_info(const int &id, const std::string &columns = "*"); // Возвращает строку информации дисциплины по её ID. По умолчанию Название, Описание, ID преподавателя
    int change_discipline_info(const int &id, const DBRow &info); // Заменяет данные дисциплины на указанные по её ID
    int add_student_to_discipline(const int &disciplineId, const std::string studentId); // Добавляет пользователя с указанным ID на дисциплину с указанным ID
    int dismiss_student_from_discipline(const int &disciplineId, const std::string studentId); // Отчисляет пользователя с указанным ID с дисциплины с указанным ID
    int delete_discipline(const int &id); // Отмечает дисциплину как удалённую (реально ничего не удаляется). Все тесты и оценки перестают отображаться, но тоже не удаляются.

    int create_test(const int &disciplineId, const std::string &title, const std::string &authorId); // Добавляет новый тест в дисциплину с ID новый тест с указанным названием, пустым списком вопросов и автором и возвращает ID теста. По умолчанию тест не активен.
    std::string get_test_status(const int &id); // Для дисциплины с указанным ID и теста с указанным ID возвращает статус
    std::tuple<std::vector<DBConnector::Row, std::string> get_test_try_answers_of(const std::string &authorId, const int &testId); // Для пользователя с ID и теста с ID возвращается массив ответов и статус состояние попытки
    std::vector<std::string> get_test_tries_authors(const int &id); // Для теста с указанным ID выбирает все попытки и возвращает ID пользователей выполнивших эти попытки
    std::vector<DBConnector::Row> get_test_tries(const int &id); // Для теста с указанным ID выбирает все попытки и возвращает ID пользователей выполнивших эти попытки
    DBRow get_test_info(const int &id, const std::string &columns = "*"); // Возвращает строку информации теста по ID дисциплины и его ID.
    int change_test_questions(); // Если у теста ещё не было попыток прохождения, то для теста с указанным ID устанавливает указанную последовательность вопросов
    int change_test_status(const int &id); // Для дисциплины с указанным ID и теста с указанным ID изменяет статус
    int change_test_info(const int &id, const DBRow &info); // Заменяет данные теста на указанные по ID дисциплины и его ID
    int add_question_to_test(const int &testId, const int &questionId); // Если у теста ещё не было попыток прохождения, то добавляет в теста с указанным ID вопрос с указанным ID в последнюю позицию
    int remove_question_from_test(const int &testId, const int &questionOrderNumber); // Если у теста ещё не было попыток прохождения, то удаляет у теста с указанным ID вопрос с указанным номером
    int delete_test(const int &id); // Отмечает тест как удалённый (реально ничего не удаляется). Все оценки перестают отображаться, но тоже не удаляются.
   
    DBRow get_questions_list(); // Возвращает массив содержащий Название вопроса, его версию и ID автора для каждого теста в системе. Если у вопроса есть несколько версий, показывается только последняя
    DBRow get_question_info(const int &id, const std::vector<std::string> &columns); // Для указанного ID вопроса и версии возвращает Название, Текст вопроса, Варианты ответов, Номер правильного ответа
    int create_question(const std::string &title, const std::vector<std::string> &options, std::vector<int> points); // Создаёт новый вопрос с заданным Названием, Вариантами ответов, Номером правильного ответа. Версия вопроса 1. В качестве ответа возвращается ID вопроса
    int update_question(const int &id, const DBRow &info); // Для указанного ID вопроса создаёт новую версию с заданным Названием, Тексом вопроса, Вариантами ответов, Номером правильного ответа
    int change_question_status(const int &id); // Если вопрос не используется в тестах (даже удалённых), то вопрос отмечается как удалённый (но реально не удаляется)

    int create_try(const int &testId); // Если пользователь с ID ещё не отвечал на тест с ID и тест находится в активном состоянии, то создаётся новая попытка и возвращается её ID
    std::vector<DBConnector::Row> get_tries_for_test(const int &id); // Для теста с указанным ID выбирает все попытки и возвращает оценки и ID пользователей выполнивших эти попытки
    int change_try_answer(const int &tryId, const int &answerOrderNumber, const int &optionOrderNumber); // Если тест находится в активном состоянии и пользователь ещё не закончил попытку, то для попытки с ID изменяет значение ответа с ID
    std::tuple<std::vector<DBConnector::Row, std::string> get_try_answers_of(const std::string &authorId, const int &tryId); // Для пользователя с ID и теста с ID возвращается массив ответов и статус состояние попытки
    int change_try_answers(const int &id, const std::vector<int> &options); // Для массового сохранения пунктов ответов в попытке
    int change_try_status(const int &id); // При смене статуса на завершённый: Если тест находится в активном состоянии и пользователь ещё не закончил попытку, то устанавливает попытку в состояние: завершено.
    int change_try_info(const DBRow &info); // Заменяет данные попытки на указанные по её ID

    DBRow get_answer_info(cosnt int &id); // Возвращает троку информации ответа. По дефолту ID вопроса, и индекс выбранного варианта ответа от 0.
    int create_answer(const int &testId, const std::vector<std::string> &columns); // Создаётся куча ответов при начале попытки. Возвращае
    int change_answer_option(const int &id, const int &option); // Меняет выбранный пункт в ответе
    int change_answer_info(const int &id, const DBRow &info); // Заменяет данные ответа на указанные по его ID
    */
};

#endif
