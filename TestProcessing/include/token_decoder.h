#ifndef TOKEN_H
#define TOKEN_H

#include <nlohmann/json.hpp>
#include <jwt-cpp/jwt.h>

// Вспомогательный класс для хранения загруженного токена
struct Token {
public:
    bool is_valid() const;
    std::string get_user() const;
    bool has_role(const std::string &permission) const;
    bool has_permission(const std::string &permission) const;

private:
    std::string asstring;
    std::string key;
    
    nlohmann::json payload;
    bool verified;
    
    // Дружественный класс, чтобы TokenDecoder мог заполнять body
    friend class TokenDecoder;
};

// Статический класс для декодирования токена
class TokenDecoder {
public:
    static Token decode(const std::string &token, const std::string &key);
};

#endif
