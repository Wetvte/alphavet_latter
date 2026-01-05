#ifndef TOKEN
#define TOKEN

#include <iostream>
#include <sstream>
#include "../include/token_decoder.h"

std::string Token::get_user() const {
    return payload_data["sub"].get<std::string>();
}
bool Token::is_valid() const {
    if (!verified) return false;

    long expTime = payload_data["exp"].get<long>();
    long currentTime = static_cast<long>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    return expTime > currentTime;
}
bool Token::has_role(const std::string &role) const {
    if (!payload_data.contains("roles") || !payload_data["roles"].is_array()) return false;
    // Проходим по элементам массива
    for (const auto& observing : payload_data["roles"]) {
        if (observing.is_string() && observing.get<std::string>() == role) {
            return true;
        }
    }
    return false;
}
bool Token::has_permission(const std::string &permission) const {
    if (!payload_data.contains("permissions") || !payload_data["permissions"].is_array()) return false;
    // Проходим по элементам массива
    for (const auto& observing : payload_data["permissions"]) {
        if (observing.is_string() && observing.get<std::string>() == permission) {
            return true;
        }
    }
    return false;
}

Token TokenDecoder::decode(const std::string &token, const std::string &key) {
    Token result;
    
    try {
        // Инициализируем токен
        if (!intit_token(token, result)) {
            std::cerr << "Ошибка: токен невалиден!" << std::endl;
            return result;
        }

        // Декодируем
        if (!low_level_decode_jwt(result)) {
            std::cerr << "Ошибка: не удалось декодировать токен!" << std::endl;
            return result;
        }

        // Проверяем подпись
        if (!verify_jwt_signature(result, key)) {
            std::cerr << "Ошибка: подпись токена невалидна!" << std::endl;
            return result;
        }

        std::cout << "Токен валиден!" << std::endl;
    }
    catch (const std::exception& e) {
        std::cout << e.what() << "\n";
    }

    return result;
}

// Разбивает строку по разделителю
bool TokenDecoder::intit_token(const std::string& token, Token& wrapper) {
    std::vector<std::string> parts;
    std::stringstream ss(token);
    std::string part;
    while (std::getline(ss, part, '.')) {
        parts.push_back(part);
    }

    if (parts.size() != 3) {
        return false;
    }

    wrapper.asstring = token;
    wrapper.header_part = parts[0];
    wrapper.payload_part = parts[1];
    wrapper.signature_part = parts[2];
    wrapper.inited = true;

    return true;
}

// Низкоуровневое декодирование JWT
bool TokenDecoder::low_level_decode_jwt(Token& token) {
    if (!token.inited) return false;

    try {
        // Декодируем base64url
        token.decoded_header_part = base64url_decode(token.header_part);
        token.decoded_payload_part = base64url_decode(token.payload_part);


        // Парсим JSON с помощью nlohmann::json
        token.header_data = nlohmann::json::parse(token.decoded_header_part);
        token.payload_data = nlohmann::json::parse(token.decoded_payload_part);

        token.parsed = true;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Ошибка при парсинге токена." << e.what() << std::endl;
        return false;
    }
}

// Декодирует base64url (без проверки подписи)
std::string TokenDecoder::base64url_decode(const std::string& input) {
    // Заменяем '-' на '+', '_' на '/'
    std::string temp = input;
    for (char& c : temp) {
        if (c == '-') c = '+';
        else if (c == '_') c = '/';
    }

    // Добавляем padding '=' до длины, кратной 4
    while (temp.size() % 4 != 0) {
        temp += '=';
    }

    // Декодируем через OpenSSL BIO
    BIO* bio = BIO_new_mem_buf(temp.data(), static_cast<int>(temp.size()));
    BIO* b64 = BIO_new(BIO_f_base64());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);  // Без переносов строк

    // Буфер для результата
    const int buffer_size = static_cast<int>(temp.size());  // Грубая оценка
    std::string result(buffer_size, '\0');
    int decoded_length = BIO_read(bio, result.data(), buffer_size);

    BIO_free_all(bio);

    if (decoded_length <= 0) {
        throw std::runtime_error("Не получилось сделать деродирование base64");
    }

    result.resize(decoded_length);  // Обрезаем до реальной длины
    return result;
}

// Проверяет подпись JWT (HS256)
bool TokenDecoder::verify_jwt_signature(Token& token, const std::string& secret_key) {
    if (!token.inited || !token.parsed) return false;

    try {
        // Формируем строку для подписи: header.payload
        std::string signing_input = token.header_part + "." + token.payload_part;

        // Вычисляем HMAC‑SHA256 от signing_input
        std::string expected_signature = hmac_sha256(secret_key, signing_input);

        // Декодируем исходную подпись из токена
        std::string actual_signature = base64url_decode(token.signature_part);

        // Сравниваем подписи
        token.verified = (expected_signature == actual_signature);

        return token.verified;

    } catch (const std::exception& e) {
        std::cerr << "Ошибка проверки подписи: " << e.what() << std::endl;
        return false;
    }
}

// Вычисляет HMAC‑SHA256
std::string TokenDecoder::hmac_sha256(const std::string& key, const std::string& data) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    HMAC(
        EVP_sha256(),
        key.data(), static_cast<int>(key.size()),
        reinterpret_cast<const unsigned char*>(data.data()), static_cast<int>(data.size()),
        digest, nullptr
    );
    return std::string(reinterpret_cast<char*>(digest), SHA256_DIGEST_LENGTH);
}

#endif