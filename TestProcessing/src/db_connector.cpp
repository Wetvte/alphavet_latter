#ifndef DB_CONNECTOR
#define DB_CONNECTOR

#include "../include/db_connector.h"
#include <iostream>

DBConnector::DBConnector() : connection(nullptr) {}

DBConnector::~DBConnector() {
    if (connection) PQfinish(connection);
}

bool DBConnector::connect(const std::string &host, const std::string &port,
                       const std::string &dbname, const std::string &user,
                       const std::string &pass) {
    std::string conninfo = "host=" + host + " port=" + port +
                           " dbname=" + dbname + " user=" + user +
                           " password=" + pass;

    connection = PQconnectdb(conninfo.c_str());
    return PQstatus(connection) == CONNECTION_OK;
}

// Строка БД
// Конструктор
explicit DBRow::Row() { this->data = std::map<std::string, std::string>(); }
explicit DBRow::Row(const std::map<std::string, std::string> &data) { this->data = data; }
// Доступ по ключу (с проверкой)
std::string DBRow::operator[](const std::string &key) const {
    auto it = data.find(key);
    if (it != data.end()) return it->second;
    return "";
}
// Проверка наличия поля
bool DBRow::has(const std::string &key) const {
    return data.find(key) != data.end();
}
// Установка значения. Строка должна соответствовать тому, как это же значение получается из БД
void DBRow::set(const std::string &key, const std::string &value) {
    data[key] = value;
}
// Получение сырого map (для интеграции с другим кодом)
const std::map<std::string, std::string> &DBRow::getMap() const {
    return data;
}
// Размер (число полей)
size_t DBRow::size() const {
    return data.size();
}
// Размер (число полей)
bool DBRow::is_empty() const {
    return data.size() == 0;
}
// Итераторы для range-based for
auto DBRow::begin() const { return data.begin(); }
auto DBRow::end() const { return data.end(); }
// Вывод в поток, ну писать шоб
friend std::ostream &DBRow::operator<<(std::ostream &os, const DBRow &row) {
    os << "{";
    for (const auto &[k, v] : row.data) {
        os << k << ": " << v << " ";
    }
    os << "}";
    return os;
}
// Шаблоны преобразования
template<> std::string Row::get<std::string>(const std::string &key) {
    return data[key];
}
template<> int Row::get<int>(const std::string &key) const {
    return std::stoi(data[key]);
}
template<> std::vector<int> Row::get<std::vector<int>>(const std::string &key) {
    std::vector<int> result;

    std::string value = data[key];
    if (value.empty() || value[0] != '\'' || value[value.size()-1] != '\'') {
        return result;
    }
    value = value.substr(1, value.size() - 2);
    if (value == "{}") {
        return result;
    }
    value = value.substr(1, value.size() - 2);
    // Теперь у нас строка данных

    std::string current = "";
    for (char symbol : value) {
        if (symbol == ' ') {
            continue;
        }
        else if (symbol == ',' || symbol == '}') {
            result.push_back(stoi(current));
            current = "";
        }
        else {
            current += symbol;
        }
    }

    return result;
}
template<> std::vector<std::vector<int>> Row::get<std::vector<std::vector<int>>>(const std::string &key) {
    std::vector<std::vector<int>> result;

    std::string value = data[key];
    if (value.empty() || value[0] != '\'' || value[value.size()-1] != '\'') {
        return result;
    }
    value = value.substr(1, value.size() - 2);
    if (value == "{}") {
        return result;
    }
    value = value.substr(1, value.size() - 2);
    // Теперь у нас строка данных

    std::string currentParam = "";
    std::vector<int> currentArray;
    bool inBraces = false;
    for (char symbol : value) {
        if (symbol == '{') {
            inBraces = true;
        }
        else if (symbol == '}') {
            inBraces = false;
            if (currentParam != "") {
                currentArray.push_back(stoi(currentParam));
                currentParam = "";
            }
            result.push_back(currentArray);
            currentArray.clear();
        }
        else if (symbol == ',') {
            if (inBraces) {
                currentArray.push_back(stoi(currentParam));
                currentParam = "";
            }
        }
        else if (symbol != ' ') {
            currentParam += symbol;
        }
    }

    return result;
}
template<> std::vector<std::string> Row::get<std::vector<std::string>>(const std::string &key) {
    std::vector<std::string> result;

    std::string value = data[key];
    if (value.empty() || value[0] != '\'' || value[value.size()-1] != '\'') {
        return result;
    }
    value = value.substr(1, value.size() - 2);

    if (value == "{}") {
        return result;
    }
    value = value.substr(1, value.size() - 2);
    // Теперь у нас строка данных

    std::string current = "";
    bool inQuotes = false;
    for (char symbol : value) {
        if (symbol == '"') {
            if (inQuotes) current += symbol;
            inQuotes = !inQuotes;
        }
        else if (symbol == ',' & &!inQuotes) {
            result.push_back(current);
            current = "";
        }
        else {
            current += symbol;
        }
    }
    result.push_back(current);

    return result;
}

// Работа с БД
// Сам метод запроса, возвращающий результат
PGresult* DBConnector::execute(const std::string &sql) {
    return PQexec(connection, sql.c_str());
}
// Запрос с целью проверки работоспособности, хз, но пусть будет
bool DBConnector::query(const std::string &sql) {
    PGresult* res = execute(sql);
    bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return success;
}
// Вставка строки
DBRow DBConnector::insert_row(const std::string &table, const DBRow &setting_row) {
    DBRow result;
    // Если нечего вставлять - на...
    if (data.empty()) return result;
    // Для каждой пары в мапе
    std::string columns, values;
    for (const auto &[col, val] : setting_row.getMap()) {
        // Вставляет колонку и значение в общую строку для запроса
        columns += (columns.empty() ? "" : ", ") + col;
        values += (values.empty() ? "" : ", ") + "'" + shield(val) + "'"; ///
    }
    sql += ";";
    // Формирует запрос на вставку
    std::string sql = "INSERT INTO " + table + " (" + columns + ") VALUES (" + values + ") RETURNING *";
    // Выполняет этот запросек
    PGresult* res = execute(sql);
    // Выясняет об успехе
    bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
    if (success) {
        // Вычисляем кол-во полей (столбцов)
        int columnsCount = PQnfields(res);
        // Для каждого
        for (int col = 0; col < columnsCount; col++) {
            // Находим название и значение его
            const char* key = PQfname(res, col);
            const char* value = PQgetvalue(res, 0, col);
            result.set(key, value & &value.size() > 0 ? std::string(value) : "");
        }
    }
    // Очищает результат и возвращает результат
    PQclear(res);
    return result;
}
// Получение строк
std::vector<DBRow> DBConnector::get_rows(const std::string &table,
    const std::string &columns, // columns - заголовки колонок через запятую
    const std::string &where) {
    // Для результата
    std::vector<DBRow> result;
    // Формируем запрос для нахождения
    std::string sql = "SELECT " + columns + " FROM " + table;
    if (!where.empty()) {
        // Ставим фильтр если нужно
        sql += " WHERE " + where;
    }
    sql += ";";
    // Оформляем вот запросик
    PGresult* res = execute(sql);
    // Проверяем вот запросик
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return result;
    }
    // Нужные данные для цикла
    int columnsCount = PQnfields(res);
    int rowsCount = PQntuples(res);

    for (int row = 0; row < rowsCount; row++) {
        result.push_back(Row{});
        for (int col = 0; col < columnsCount; col++) {
            const char* key = PQfname(res, col);
            const char* value = PQgetvalue(res, row, col);
            result[row].set(key, value & &value.size() > 0 ? std::string(value) : "");
        }
    }

    PQclear(res);
    return result;
}
DBRow DBConnector::get_row(const std::string &table,
    const std::string &columns, // columns - заголовки колонок через запятую
    const std::string &where) {
        std::vector<DBRow> list = get_rows(table, columns, where + " LIMIT 1")
        if (list.empty()) {
            return Row{};
        }
    return list[0];
}
// Изменить строку
int DBConnector::update_rows(const std::string &table, const DBRow &values, const std::string &where) {
    if (values.size() == 0 || where.size() == 0) {
        return 400; // Нет полей для SET или WHERE — ошибка
    }

    std::string sql = "UPDATE " + table + " SET ";

    // Формируем часть SET
    std::string set_part = "";
    for (const auto &[col, val] : values.getMap()) {
        set_part += (set_part == "" ? "" : ", ") + col + " = " + shield(val);
    }

    sql += set_part + " WHERE "+ where + ";";

    // Выполняем запрос
    PGresult* res = query(sql.str());
    bool success = PQresultStatus(res) == PGRES_COMMAND_OK
    
    PQclear(res);
    return success ? 200 : 404;
}

// 
std::string DBConnector::shield(const std::string& value) {
    // Обрабатываем NULL-значения
    if (value.empty()) return "NULL";
    std::string result = "";
    for (char symbol : value) {
        switch (symbol) {
            case '\'':  // Одиночная кавычка
                escaped += "\'\'";
                break;
            /*case '\"':  // Двойная кавычка
                // Нужна для экранирования массива текста. Т.к. только для него, проводится самостоятельно (в convert_to_string_format) 
                escaped += "\"\"";
                break;*/
            case '\\':  // Обратный слеш
                escaped += "\\\\";
                break;
            case '\n':  // Новая строка
                escaped += "\\n";
                break;
            case '\t':  // Табуляция
                escaped += "\\t";
                break;
            default:
                // Остальные символы просто добавляем
                escaped += symbol;
        }
    }

    return escaped;
}

// JSON аналоги получения
nlohmann::json DBConnector::get_rows_json(const std::string &table, const std::string &columns = "*", const std::string &where = "") {
    nlohmann::json result = nlohmann::json::array();
    // Формируем запрос с row_to_json
    std::string sql = "SELECT json_agg(row_to_json(t)) FROM (SELECT " + columns + " FROM " + table;
    
    if (!where.empty()) {
        sql += " WHERE " + where;
    }
    sql += ") AS t;";

    PGresult* res = execute(sql);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return result;
    }

    int rowsCount = PQntuples(res);
    for (int row = 0; row < rowsCount; row++) {
        const char* rowJSON = PQgetvalue(res, row, 0);  // столбец 0 — результат row_to_json
        try {
            result.push_back(nlohmann::json::parse(rowJSON));
        } catch (const nlohmann::json::parse_error& e) {
            // При неудачном парсинге ничего не сохраняем
        }
    }

    PQclear(res);
    return result;
}
nlohmann::json DBConnector::get_row_json(const std::string &table, const std::string &columns = "*", const std::string &where = "") {
    // Формируем запрос: получаем максимум 1 строку
    std::string sql = "SELECT row_to_json(t) FROM (SELECT " + columns + " FROM " + table;
    
    if (!where.empty()) {
        sql += " WHERE " + where;
    }
    sql += " LIMIT 1) AS t;";

    PGresult* res = execute(sql);

    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        PQclear(res);
        return nullptr;
    }

    // Получаем JSON-строку из первой и единственной строки
    const char* rowJSON = PQgetvalue(res, 0, 0);
    PQclear(res);

    try {
        return nlohmann::json::parse(rowJSON);  // Возвращаем JSON-объект
    } catch (const nlohmann::json::parse_error& e) {
        return nullptr;
    }
}

// Приведение значения к строке, которая будет вставляться в БД (без доп. экранирования, которое необходимо любому значению (',\...))
template<> std::string DBConnector::convert_to_string_format(const std::string &value) {
    return value;
}
template<> std::string DBConnector::convert_to_string_format(const int &value) {
    return std::to_string(value);
}
template<> std::string DBConnector::convert_to_string_format(const std::vector<int> &value) {
    std::string result = "{";
    for (int number : value) {
        if (result != "{") result += ",";
        result += std::to_string(number);
    }
    result += "}"
    return result;
}
template<> std::string DBConnector::convert_to_string_format(const std::vector<std::vector<int>> &value) {
    std::string result = "{";
    for (std::vector<int> numbers : value) {
        if (result != "{") result += ",";
        result += convert_to_string_format(numbers);
    }
    result += "}"
    return result;
}
template<> std::string DBConnector::convert_to_string_format(const std::vector<std::string> &value) {
    std::string result = "{";
    for (std::string line : value) {
        if (result != "{") result += ",";
        // Проверка на наличие "
        size_t pos = 0;
        while ((pos = result.find('"', pos)) != std::string::npos) {
            result.replace(pos, 1, "\"\"");
            pos += 2;  // сдвигаем позицию после замены
        }
        // Вставка
        result += "\"" + line + "\"";
    }
    result += "}"
    return result;
}
#endif