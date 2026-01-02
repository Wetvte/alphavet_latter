#ifndef CONFIG_LOADER_H
#define CONFIG_LOADER_H

#include <string>
#include <unordered_map>

// Вспомогательный класс для хранения загруженной конфигурации
struct Config {
public:
    std::string get(const std::string& key) const;

private:
    std::unordered_map<std::string, std::string> values;
    
    // Дружественный класс, чтобы ConfigLoader мог заполнять body
    friend class ConfigLoader;
};

// Статический класс для загрузки конфигурации
class ConfigLoader {
public:
    static Config load(const std::string& filepath);

private:
    // Вспомогательный метод для парсинга строки "ключ=значение"
    static bool parse_line(const std::string& line, std::string& key, std::string& value);
};

#endif
