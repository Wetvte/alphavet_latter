#ifndef DB_CONNECTOR_H
#define DB_CONNECTOR_H

#include <vector>
#include <string>
#include <iostream>
#include <map>
#include "libpq-fe.h"

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
        const std::map<std::string, std::string> get_map() const;
        // Размер (число полей)
        size_t size() const;
        bool is_empty() const;
        // Итераторы для range-based for
        auto begin() const;
        auto end() const;
        // Вывод в поток, ну писать шоб
        friend std::ostream &operator<<(std::ostream &os, const DBRow &row);

        // Шаблоны преобразования при возврате
        template<typename T> T get(const std::string &key);
        template<> std::string get<std::string>(const std::string &key);
        template<> int get<int>(const std::string &key);
        template<> std::vector<int> get<std::vector<int>>(const std::string &key);
        template<> std::vector<std::string> get<std::vector<std::string>>(const std::string &key);
        template<> std::vector<std::vector<int>> get<std::vector<std::vector<int>>>(const std::string &key);
};

class DBConnector {
public:
    DBConnector();
    ~DBConnector();

    bool connect(const std::string &host, const std::string &port,
        const std::string &dbname, const std::string &user, const std::string &pass);
    void disconnect();
    // Аналогом строки является мапа, а аналогом нескольних строк выступает вектор мап

    // Выполняет любой запрос и требует обработки результата
    PGresult* execute(const std::string &sql);
    // Запрос с целью проверки чего-то
    bool query(const std::string &sql);

    // Делает одну запись в таблицу с указанным названием и 
    DBRow insert_row(const std::string &table, const DBRow &setting_row);
    // Получает записи строк из указанной таблицы
    std::vector<DBRow> get_rows(const std::string &table, const std::string &columns = "*", const std::string &where = "");
    DBRow get_row(const std::string &table, const std::string &columns = "*", const std::string &where = "");
    // Обновление строки
    int update_rows(const std::string &table, const DBRow &values, const std::string &where);
    // Вспомогательный метод для экранирования вставляемых значений
    std::string shield(const std::string &value);

    std::string convert_to_string_format(const std::string &value);
    std::string convert_to_string_format(const int &value);
    std::string convert_to_string_format(const std::vector<std::string> &value);
    std::string convert_to_string_format(const std::vector<int> &value);
    std::string convert_to_string_format(const std::vector<std::vector<int>> &value);

private:
    PGconn* connection;
};

// Шаблоны преобразования template<typename T>
template<typename T> T DBRow::get(const std::string& key) {
    return data[key];
}
template<> std::string DBRow::get<std::string>(const std::string &key) {
    return data[key];
}
template<> int DBRow::get<int>(const std::string &key) {
    try {
        return std::stoi(data[key]);
    } catch (const std::exception&) {
        return 0;
    }
}
template<> std::vector<int> DBRow::get<std::vector<int>>(const std::string &key) {
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
            try {
                result.push_back(std::stoi(current));
            } catch (const std::exception&) {
                result.push_back(0);
            }
            current = "";
        }
        else {
            current += symbol;
        }
    }

    return result;
}
template<> std::vector<std::vector<int>> DBRow::get<std::vector<std::vector<int>>>(const std::string &key) {
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
template<> std::vector<std::string> DBRow::get<std::vector<std::string>>(const std::string &key) {
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
        else if (symbol == ',' && !inQuotes) {
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
#endif