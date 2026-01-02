#ifndef DB_CONNECTOR_H
#define DB_CONNECTOR_H

#include "libpq-fe.h"
#include <string>
#include <vector>
#include <map>

struct DBRow {
        private:
            std::map<std::string, std::string> data;
        public:
        // Доступ по ключу (с проверкой)
        std::string operator[](const std::string &key) const;
        // Проверка наличия поля
        bool has(const std::string &key) const;
        // Установка значения (для построения строки)
        void set(const std::string &key, const std::string &value);
        // Получение сырого map (для интеграции с другим кодом)
        const std::map<std::string, std::string> &getMap();
        // Размер (число полей)
        size_t size() const;
        bool is_empty() const;
        // Итераторы для range-based for
        auto begin() const;
        auto end() const;
        // Вывод в поток, ну писать шоб
        friend std::ostream &operator<<(std::ostream &os, const DBRow &row);

        // Шаблоны преобразования при возврате
        template<typename T> T get<T>(const std::string &key) const;
}

class DBConnector {
public:
    DBConnector();
    ~DBConnector();

    bool connect(const std::string &host, const std::string &port,
        const std::string &dbname, const std::string &user, const std::string &pass);

    // Аналогом строки является мапа, а аналогом нескольних строк выступает вектор мап

    // Выполняет любой запрос и требует обработки результата
    PGresult* execute(const std::string &sql);
    // Запрос с целью проверки чего-то
    bool query(const std::string &sql);

    // Делает одну запись в таблицу с указанным названием и 
    DBRow insert_row(const std::string &table, const DBRow &setting_row);
    // Получает записи строк из указанной таблицы
    std::vector<DBRow> get_rows(
        const std::string &table, const std::string &columns = "*", const std::string &where = "");
    DBRow get_row(
        const std::string &table, const std::string &columns = "*", const std::string &where = "");
    // Обновление строки
    bool update_rows(const std::string &table, const DBRow &values, const std::string &where);
    // Вспомогательный метод для экранирования вставляемых значений
    std::string shield(const std::string &value);

    // JSON аналоги получения
    nlohmann::json get_rows_json(
        const std::string &table, const std::string &columns = "*", const std::string &where = "");
    nlohmann::json get_row_json(
        const std::string &table, const std::string &columns = "*", const std::string &where = "");

    template<typename T> std::string convert_to_string_format(const T &value);

private:
    PGconn* connection;
};

#endif