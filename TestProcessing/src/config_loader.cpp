#ifndef CONFIG_LOADER
#define CONFIG_LOADER

#include "../include/config_loader.h"
#include <fstream>
#include <sstream>

#include <iostream> // для вывода проверки

std::string Config::get(const std::string& key) const {
    auto it = values.find(key);
    if (it != values.end()) {
        return it->second;
    }
    return ""; 
}

Config ConfigLoader::load(const std::string& filepath) {
    Config config;
    std::ifstream file(filepath);
    
    if (!file.is_open()) {
        std::cout << "Файл не был открыт." << std::endl;
        return config;  // Возвращаем пустой Config, если файл не открылся
    }

    std::cout << "Файл env открыт." << std::endl;

    //  Проверка чтения конфига из файла
    std::string line;
    while (std::getline(file, line)) {
        std::cout << "Получена строка из env: " << line << std::endl;
        std::string key, value;
        if (parse_line(line, key, value)) {
            std::cout << "Разделена на: " << key << " и " << value << std::endl;
            config.values[key] = value;
        }
    }
    
    file.close();
    return config;
}

// Вспомогательный метод для парсинга строки вида "ключ=значение"
bool ConfigLoader::parse_line(const std::string& line, std::string& key, std::string& value) {
    if (line.empty() || line[0] == '#' || line[0] == ';') {
        return false;  // Пропускаем пустые строки и комментарии
    }

    size_t pos = line.find('=');
    if (pos == std::string::npos) {
        return false;  // Нет знака '=' — некорректная строка
    }

    key = line.substr(0, pos);
    value = line.substr(pos + 1);

    // Удаляем пробелы в начале и конце ключа и значения
    key.erase(0, key.find_first_not_of(" \t"));
    key.erase(key.find_last_not_of(" \t") + 1);
    value.erase(0, value.find_first_not_of(" \t"));
    value.erase(value.find_last_not_of(" \t") + 1);

    return true;
}

#endif