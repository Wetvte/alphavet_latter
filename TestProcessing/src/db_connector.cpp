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

// Строка БД
// Конструктор
explicit DBConnector::Row::Row() { this->data = std::map<std::string, std::string>(); }
explicit DBConnector::Row::Row(const std::map<std::string, std::string>& data) { this->data = data; }
// Доступ по ключу (с проверкой)
std::string DBConnector::Row::operator[](const std::string& key) const {
    auto it = data.find(key);
    if (it != data.end()) return it->second;
    return "";
}
// Проверка наличия поля
bool DBConnector::Row::has(const std::string& key) const {
    return data.find(key) != data.end();
}
// Установка значения (для построения строки)
void DBConnector::Row::set(const std::string& key, const std::string& value) {
    data[key] = value;
}
// Получение сырого map (для интеграции с другим кодом)
const std::map<std::string, std::string>& DBConnector::Row::getMap() const {
    return data;
}
// Размер (число полей)
size_t DBConnector::Row::size() const {
    return data.size();
}
// Размер (число полей)
bool DBConnector::Row::is_empty() const {
    return data.size() == 0;
}
// Итераторы для range-based for
auto DBConnector::Row::begin() const { return data.begin(); }
auto DBConnector::Row::end() const { return data.end(); }
// Вывод в поток, ну писать шоб
friend std::ostream& DBConnector::Row::operator<<(std::ostream& os, const Row& row) {
    os << "{";
    for (const auto& [k, v] : row.data) {
        os << k << ": " << v << " ";
    }
    os << "}";
    return os;
}

// Работа с БД
// Сам метод запроса, возвращающий результат
PGresult* DBConnector::execute(const std::string& sql) {
    return PQexec(connection, sql.c_str());
}
// Запрос с целью проверки работоспособности, хз, но пусть будет
bool DBConnector::query(const std::string& sql) {
    PGresult* res = execute(sql);
    bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return success;
}
// Вставка строки
bool DBConnector::insert_row(const std::string& table, const Row& setting_row, Row& result_row) {
    // Если нечего вставлять - на...
    if (data.empty()) return false;
    // Для каждой пары в мапе
    std::string columns, values;
    for (const auto& [col, val] : setting_row.getMap()) {
        // Вставляет колонку и значение в общую строку для запроса
        columns += (columns.empty() ? "" : ", ") + col;
        values += (values.empty() ? "" : ", ") + "'" + escapeValue(val) + "'";
    }
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
            result_row[key] = value && value.size() > 0 ? value : "";
        }
    }
    // Очищает результат и возвращает результат
    PQclear(res);
    return success;
}
// Получение строк
bool DBConnector::get_rows(const std::string& table,
    const std::string& columns, // columns - заголовки колонок с "" через запятую
    const std::string& where, std::vector<Row>& result) {
    // Для результата
    result = std::vector<Row>();
    // Формируем запрос для нахождения
    std::string sql = "SELECT " + columns + " FROM " + table;
    if (!where.empty()) {
        // Ставим фильтр если нужно
        sql += " WHERE " + where;
    }
    // Оформляем вот запросик
    PGresult* res = execute(sql);
    // Проверяем вот запросик
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return false;
    }
    // Нужные данные для цикла
    int columnsCount = PQnfields(res);
    int rowsCount = PQntuples(res);

    for (int row = 0; row < rowsCount; row++) {
        result.push_back(Row{});
        for (int col = 0; col < columnsCount; col++) {
            const char* key = PQfname(res, col);
            const char* value = PQgetvalue(res, row, col);
            result[row][key] = value && value.size() > 0 ? value : "";
        }
    }

    PQclear(res);
    return true;
}


#endif