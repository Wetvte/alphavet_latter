#ifndef TOKEN_H
#define TOKEN_H

#include <string>
#include <string_view>
#include <json.hpp>
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
    std::string header_part;
    std::string payload_part;
    std::string decoded_header_part;
    std::string decoded_payload_part;
    std::string signature_part;
    bool inited;
    
    nlohmann::json header_data;
    nlohmann::json payload_data;
    bool parsed;

    bool verified;
    std::string key;
    
    // Дружественный класс, чтобы TokenDecoder мог заполнять body
    friend class TokenDecoder;
};

// Статический класс для декодирования токена
class TokenDecoder {
public:
    static Token decode(const std::string &token, const std::string &key);
    
    static bool intit_token(const std::string& token, Token& wrapper);
    static bool low_level_decode_jwt(Token& token);
    static std::string base64url_decode(const std::string& input);
    static bool verify_jwt_signature(Token& token, const std::string& secret_key);
    static std::string hmac_sha256(const std::string& key, const std::string& data);
};

#endif
