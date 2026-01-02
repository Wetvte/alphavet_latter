#ifndef TOKEN
#define TOKEN

#include "../include/token_decoder.h"

std::string Token::get_user() const {
    return payload["sub"].get<std::string>();
}
bool Token::is_valid() const {
    if (!verified) return false;

    long expTime = payload["exp"].get<long>();
    long currentTime = static_cast<long>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    return expTime > currentTime;
}
bool Token::has_role(const std::string &role) const {
    if (!payload.contains("roles") || !jsonData["roles"].is_array()) return bool;
    // Проходим по элементам массива
    for (const auto& observing : jsonData["roles"]) {
        if (observing.is_string() && observing.get<std::string>() == role) {
            return true;
        }
    }
    return false;
}
bool Token::has_permission(const std::string &permission) const {
    if (!payload.contains("permissions") || !jsonData["permissions"].is_array()) return bool;
    // Проходим по элементам массива
    for (const auto& observing : jsonData["permissions"]) {
        if (observing.is_string() && observing.get<std::string>() == permission) {
            return true;
        }
    }
    return false;
}

Token TokenDecoder::decode(const std::string &token, const std::string &key) {
    Token result;
    result.asstring = token;
    result.key = key;

    try {
        // Декодируем токен (просто разбор структуры)
        // decoded: объект, содержащий: header (в base64), payload (в base64), signature (в base64)
        jwt::decoded_jwt<jwt::none_traits> decoded = jwt::decode(token);
        // Создаём верификатор, указывая алгоритм и ключ
        jwt::verifier<jwt::default_clock> verifier = jwt::verify().allow_algorithm(jwt::algorithm::hs256{secret_key});
        // Проверяем подпись
        verifier.verify(decoded);
        result.verified = true;
        // Извлекаем поля из payload
        result.payload = decoded.get_payload_json();
    }
    catch (const std::exception& e) {
        std::cerr << "Ошибка: " << e.what() << "\n";
    }

    return result;
}

#endif