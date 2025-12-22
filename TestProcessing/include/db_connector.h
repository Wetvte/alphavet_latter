#ifndef DB_CONNECTOR_H
#define DB_CONNECTOR_H

#include "libpq-fe.h"
#include <string>
#include <vector>
#include <map>

class DBConnector {
public:
    DBConnector();
    ~DBConnector();

    struct Row {
        private:
            std::map<std::string, std::string> data;
        public:
        // Констрктааары
        explicit Row();
        explicit Row(const std::map<std::string, std::string>& data);

        // Доступ по ключу (с проверкой)
        std::string operator[](const std::string& key) const;
        // Проверка наличия поля
        bool has(const std::string& key) const;
        // Установка значения (для построения строки)
        void set(const std::string& key, const std::string& value);
        // Получение сырого map (для интеграции с другим кодом)
        const std::map<std::string, std::string>& getMap();
        // Размер (число полей)
        size_t size() const;
        bool is_empty() const;
        // Итераторы для range-based for
        auto begin() const;
        auto end() const;
        // Вывод в поток, ну писать шоб
        friend std::ostream& operator<<(std::ostream& os, const Row& row);
    }

    bool connect(const std::string& host, const std::string& port,
        const std::string& dbname, const std::string& user, const std::string& pass);

    // Аналогом строки является мапа, а аналогом нескольних строк выступает вектор мап

    // Выполняет любой запрос и требует обработки результата
    PGresult* execute(const std::string& sql);
    // Запрос с целью проверки чего-то
    bool query(const std::string& sql);
    // Делает одну запись в таблицу с указанным названием и 
    bool insert_row(const std::string& table, const Row& setting_row, Row& result_row);
    // Получает записи строк из указанной таблицы
    bool get_rows(
        const std::string& table, const std::string& columns = "*", const std::string& where = "", std::vector<Row>& result);

private:
    PGconn* connection;
};

#endif