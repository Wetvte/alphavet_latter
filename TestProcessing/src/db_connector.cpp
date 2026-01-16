#ifndef DB_CONNECTOR
#define DB_CONNECTOR

#include "../include/db_connector.h"

DBConnector::DBConnector() : connection(nullptr) {}

DBConnector::~DBConnector()
{
    disconnect();
}

bool DBConnector::connect(const std::string &host, const std::string &port,
                          const std::string &dbname, const std::string &user,
                          const std::string &pass)
{
    std::string conninfo = "host=" + host + " port=" + port +
                           " dbname=" + dbname + " user=" + user +
                           " password=" + pass;

    connection = PQconnectdb(conninfo.c_str());
    return PQstatus(connection) == CONNECTION_OK;
}
void DBConnector::disconnect()
{
    if (connection)
        PQfinish(connection);
}

// Строка БД
// Конструктор
// Доступ по ключу (с проверкой)
std::string DBRow::operator[](const std::string &key) const
{
    auto it = data.find(key);
    if (it != data.end())
        return it->second;
    return "";
}
// Проверка наличия поля
bool DBRow::has(const std::string &key) const
{
    return data.find(key) != data.end();
}
// Установка значения. Строка должна соответствовать тому, как это же значение получается из БД
void DBRow::set(const std::string &key, const std::string &value)
{
    data[key] = value;
}
// Получение сырого map (для интеграции с другим кодом)
const std::map<std::string, std::string> DBRow::get_map() const
{
    return data;
}
// Размер (число полей)
size_t DBRow::size() const
{
    return data.size();
}
// Размер (число полей)
bool DBRow::is_empty() const
{
    return data.size() == 0;
}
// Итераторы для range-based for
auto DBRow::begin() const { return data.begin(); }
auto DBRow::end() const { return data.end(); }
// Вывод в поток, ну писать шоб
std::ostream &operator<<(std::ostream &os, const DBRow &row)
{
    os << "{";
    for (const auto &[key, value] : row.data)
    {
        os << "\n"
           << key << ": " << value << "; ";
    }
    os << "}";
    return os;
}

// Работа с получением int значений из json
std::vector<std::vector<int>> DBConnector::get_as_signatures(const nlohmann::json &value)
{
    std::vector<std::vector<int>> result;
    std::cout << ">Started parsing signatures" << std::endl;
    for (const nlohmann::json &row : value) {
        std::vector<int> signature;
        std::cout << "-Started parsing signature" << std::endl;
        for (const nlohmann::json &cell : row) { 
            std::cout << " Parsing int-string " << cell.dump() << std::endl;
            int number = cell.is_number() ? cell.get<int>() : get_as_int(cell.get<std::string>());
            signature.push_back(number);
        }
        std::cout << "-Ended parsing signature" << std::endl;
        result.push_back(signature);
    }
    std::cout << ">Ended parsing signatures" << std::endl;
    return result;
}
std::vector<int> DBConnector::get_as_intarr(const nlohmann::json &value)
{
    std::vector<int> result;
    std::cout << ">Started parsing arr" << std::endl;
    for (const nlohmann::json &cell : value) { 
        std::cout << " Parsing int-string " << cell.dump() << std::endl;
        int number = cell.is_number() ? cell.get<int>() : get_as_int(cell.get<std::string>());
        result.push_back(number);
    }
    std::cout << ">Ended parsing arr" << std::endl;
    return result;
}
int DBConnector::get_as_int(const nlohmann::json &value)
{
    return value.is_number() ? value.get<int>() : get_as_int(value.get<std::string>());
}
int DBConnector::get_as_int(const std::string &str)
{
    try
    {
        return std::stoi(str);
    }
    catch (const std::exception &e)
    {
        std::cout << e.what() << std::endl;
        return -1;
    }
}

// Работа с БД
// Сам метод запроса, возвращающий результат
PGresult *DBConnector::execute(const std::string &sql)
{
    return PQexec(connection, sql.c_str());
}
// Запрос с целью проверки работоспособности, хз, но пусть будет
bool DBConnector::query(const std::string &sql)
{
    PGresult *res = execute(sql);
    bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return success;
}
// Вставка строки
DBRow DBConnector::insert_row(const std::string &table, const DBRow &setting_row)
{
    DBRow result;
    // Если нечего вставлять - на...
    if (setting_row.is_empty())
        return result;
    // Для каждой пары в мапе
    std::string columns, values;
    for (const auto &[col, val] : setting_row.get_map())
    {
        // Вставляет колонку и значение в общую строку для запроса
        columns += (columns.empty() ? "" : ", ") + col;
        values += (values.empty() ? "" : ", ") + shield(val); ///
    }
    // Формирует запрос на вставку
    std::string sql = "INSERT INTO " + table + " (" + columns + ") VALUES (" + values + ") RETURNING *;";
    // Выполняет этот запросек
    std::cout << "Попытка вставить в БД строку: " << setting_row << std::endl;
    std::cout << "Insert request: " << sql << std::endl;
    PGresult *res = execute(sql);
    std::cout << "Insert status: " << PQresStatus(PQresultStatus(res)) << std::endl;
    // Выясняет об успехе
    bool success = (PQresultStatus(res) == PGRES_TUPLES_OK);
    if (success)
    {
        // Вычисляем кол-во полей (столбцов)
        int columnsCount = PQnfields(res);
        // Для каждого
        std::cout << "Обратно получены " << columnsCount << " столбцов." << std::endl;
        for (int col = 0; col < columnsCount; col++)
        {
            // Находим название и значение его
            const char *key = PQfname(res, col);
            const char *value = PQgetvalue(res, 0, col);
            std::cout << "Одна из колонок - " << std::string(key) << " со значением " << std::string(value) << std::endl;
            result.set(std::string(key), value ? std::string(value) : "");
        }
    }
    // Очищает результат и возвращает результат
    PQclear(res);
    return result;
}
// Получение строк
std::vector<DBRow> DBConnector::get_rows(const std::string &table, const std::string &columns, const std::string &where, const std::string &order)
{
    std::cout << "DB Connection: " << PQstatus(connection) << std::endl;
    // Для результата
    std::vector<DBRow> result;
    // Формируем запрос для нахождения
    std::string sql = "SELECT " + columns + " FROM " + table;
    if (!where.empty())
    {
        // Ставим фильтр если нужно
        sql += " WHERE " + where;
    }
    if (!order.empty()) {
        // Добавляем сортировку
        sql += " ORDER " + order;
    }
    sql += ";";
    std::cout << "Getting rows request " << sql << std::endl;
    // Оформляем вот запросик
    PGresult *res = execute(sql);
    // Проверяем вот запросик
    std::cout << "Getting rows result " << PQresultStatus(res) << std::endl;
    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        PQclear(res);
        return result;
    }
    // Нужные данные для цикла
    int columnsCount = PQnfields(res);
    int rowsCount = PQntuples(res);

    std::cout << "Getting rows count " << rowsCount << std::endl;
    for (int row = 0; row < rowsCount; row++)
    {
        DBRow db_row;
        for (int col = 0; col < columnsCount; col++)
        {
            const char *key = PQfname(res, col);
            const char *value = PQgetvalue(res, row, col);
            db_row.set(key, value ? std::string(value) : "");
        }
        result.push_back(db_row);
    }

    PQclear(res);
    return result;
}
DBRow DBConnector::get_row(const std::string &table, const std::string &columns, const std::string &where)
{
    std::vector<DBRow> list = get_rows(table, columns, where + " LIMIT 1");
    if (list.empty())
    {
        return DBRow{};
    }
    return list[0];
}
// Изменить строку
int DBConnector::update_rows(const std::string &table, const DBRow &values, const std::string &where)
{
    if (values.size() == 0 || where.size() == 0)
    {
        return 400; // Нет полей для SET или WHERE — ошибка
    }

    std::string sql = "UPDATE " + table + " SET ";

    // Формируем часть SET
    std::string set_part = "";
    for (const auto &[col, val] : values.get_map())
    {
        set_part += (set_part == "" ? "" : ", ") + col + "=" + shield(val);
    }

    sql += set_part + " WHERE " + where + ";";

    // Выполняем запрос
    std::cout << "Попытка обновить в БД: " << values << std::endl;
    std::cout << "Update request: " << sql << std::endl;
    PGresult *res = execute(sql);
    bool success = PQresultStatus(res) == PGRES_COMMAND_OK;
    std::cout << "Update result: " << PQresStatus(PQresultStatus(res)) << std::endl;

    PQclear(res);
    return success ? 200 : 406;
}

// Экранирует значения, чтоб не сломать запрос
std::string DBConnector::shield(const std::string &value)
{
    // Обрабатываем NULL-значения
    if (value.empty())
        return "NULL";
    std::string result = "";
    for (int i = 0; i < value.size(); i++)
    {
        switch (value[i])
        {
        case '\'': // Одиночная кавычка
            result += "\\'";
            break;
        case '\\': // Обратный слеш
        // Так как экранируется и массив текстов и нормальные значения, нужно как-то обеспечить отсутствие экранирования
        // обратного слеша, экранирующего двойные кавычки внутри элемента для массивов текста, но, при этом, он должен экранироваться у других.
        // Я не придумал лучше, чем проверить, заключён ли элемент в фигурные скобки, и есть ли после \ двойная кавычка. Если нет - экранируем
            if (value.front() != '{' && value.back() != '}' && value[i+1] != '"') result += "\\\\";
            break;
        case '\n': // Новая строка
            result += "\\n";
            break;
        case '\t': // Табуляция
            result += "\\t";
            break;
        default:
            result += value[i]; // Остальные символы просто добавляем
            break;
        }
    }

    // Экранирует
    return "'" + result + "'";
}

// Приведение значения к строке, которая будет вставляться в БД (без доп. экранирования, которое необходимо любому значению (',\...))
std::string DBConnector::convert_to_string_format(const std::string &value)
{
    return value;
}
std::string DBConnector::convert_to_string_format(const int &value)
{
    return std::to_string(value);
}
std::string DBConnector::convert_to_string_format(const std::vector<int> &value)
{
    std::string result = "{";
    for (int number : value)
    {
        if (result != "{")
            result += ",";
        result += std::to_string(number);
    }
    result += "}";
    return result;
}
std::string DBConnector::convert_to_string_format(const std::vector<std::vector<int>> &value)
{
    std::string result = "{";
    for (std::vector<int> numbers : value)
    {
        if (result != "{")
            result += ",";
        result += convert_to_string_format(numbers);
    }
    result += "}";
    return result;
}
std::string DBConnector::convert_to_string_format(const std::vector<std::string> &value)
{
    std::string result = "{";
    for (std::string line : value)
    {
        if (result != "{")
            result += ",";
        // Проверка на наличие "
        size_t pos = 0;
        while ((pos = line.find("\"", pos)) != std::string::npos)
        {
            line.replace(pos, 1, "\\\"");
            pos += 2; // сдвигаем позицию после замены
        }
        // Вставка
        result += "\"" + line + "\"";
    }
    result += "}";
    return result;
}
#endif

/*

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
*/