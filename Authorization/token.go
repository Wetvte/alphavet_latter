package main

import (
	"time"

	"encoding/json"
	"log"
	"os"
	"slices"
	"strconv"

	"github.com/golang-jwt/jwt/v5"
)

// Генерирует базовые разрешения для роли
func GenerateBasePermissionsListForRole(role string) []string {
	switch role {
	case "Student":
		return []string{"user:roles:read", "user:disciplines:read", "user:tries:read", "user:fullname:write", "user:status:write",
			"discipline:teacherslist", "discipline:testslist", "discipline:student:add", "discipline:user:remove",
			"test:tries:read",
			"tries/answer/change",
			"news:write"}
	case "Teacher":
		return []string{"user:roles:read", "user:disciplines:read", "user:tries:read", "user:fullname:write", "user:status:write",
			"discipline:teacherslist", "discipline:testslist", "discipline:studentslist", "discipline:create", "discipline:delete",
			"discipline:text:write", "discipline:test:add", "discipline:teacher:add", "discipline:user:remove",
			"test:create", "test:tries:read", "test:status:write", "test:question:add", "test:question:remove", "test:question:update",
			"question:read", "question:create", "question:update", "question:delete",
			"news:write"}
	case "Admin":
		return []string{"users:list", "user:roles:read", "user:disciplines:read", "user:tries:read", "user:fullname:write",
			"user:roles:write", "user:status:write", "user:block",
			"discipline:teacherslist", "discipline:testslist", "discipline:studentslist", "discipline:create", "discipline:delete",
			"discipline:teacher:add", "discipline:user:remove",
			"test:tries:read", "test:status:write", "test:question:remove",
			"questions:list", "question:read", "question:delete",
			"news:write"}
	default:
		return []string{}
	}
}

// Убирает повторения
func CombinePermissions(standart []string, additions []string) []string {
	var result []string = standart
	for _, permission := range additions {
		if !slices.Contains(result, permission) {
			result = append(result, permission)
		}
	}
	return result
}

// Генерирует новые токены для этого типо сервиса
func GenerateNewAccessToken(sub string, roles []string, permissions []string) string {
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, jwt.MapClaims{
		"sub":         sub,
		"type":        "access",
		"roles":       roles,
		"permissions": permissions,
		"exp":         time.Now().Add(time.Minute).Unix(), // 1 минута
	})
	result, err := token.SignedString([]byte(os.Getenv("OWN_TOKEN_GENERATE_KEY")))
	if err != nil {
		result = ""
	}
	return result
}

func GenerateNewRefreshToken(sub string) string {
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, jwt.MapClaims{
		"sub":  sub,
		"type": "refresh",
		"exp":  time.Now().Add(time.Hour * 24 * 7).Unix(), // 7 дней
	})
	result, err := token.SignedString([]byte(os.Getenv("OWN_TOKEN_GENERATE_KEY")))
	if err != nil {
		result = ""
	}
	return result
}
func GenerateNewRegistrationToken(sub string) string {
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, jwt.MapClaims{
		"sub":  sub,
		"type": "registration",
		"exp":  time.Now().Add(time.Minute * 5).Unix(), // 5 минут
	})
	result, err := token.SignedString([]byte(os.Getenv("OWN_TOKEN_GENERATE_KEY")))
	if err != nil {
		result = ""
	}
	return result
}
func GenerateNewAuthorizationToken(sub string) string {
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, jwt.MapClaims{
		"sub":  sub,
		"type": "authorization",
		"exp":  time.Now().Add(time.Minute * 5).Unix(), // 5 минут
	})
	result, err := token.SignedString([]byte(os.Getenv("OWN_TOKEN_GENERATE_KEY")))
	if err != nil {
		result = ""
	}
	return result
}

// Проверяет, токен, созданный этим типо сервисом, исчез, или нет
func IsOwnTokenExpired(token_string string) bool {
	var claims jwt.MapClaims
	tokenParsed, parse_err := jwt.ParseWithClaims(token_string, &claims, func(token *jwt.Token) (interface{}, error) {
		_, success := token.Method.(*jwt.SigningMethodHMAC) // Если не схожи методы хэширования, сразу ошибка
		if success {
			return []byte(os.Getenv("OWN_TOKEN_GENERATE_KEY")), nil
		}
		log.Println("Токен ", token_string, " не подходит под дехэш.")
		return nil, jwt.ErrInvalidKey
	})
	if parse_err != nil || !tokenParsed.Valid {
		log.Println("Токен", token_string, "недействителен.")
		return true // ошибка разбора или недействительный токен
	}

	var expTime int64
	exp := claims["exp"]
	switch t := exp.(type) {
	case float64:
		expTime = time.Unix(int64(t), 0).Unix()
	case int64:
		expTime = time.Unix(t, 0).Unix()
	case json.Number:
		// json.Number может содержать число в виде строки
		num, err := t.Int64()
		if err != nil {
			expTime = 0
		} else {
			expTime = time.Unix(num, 0).Unix()
		}
	case string:
		// Если exp сохранён как строка
		num, err := strconv.ParseInt(t, 10, 64)
		if err != nil {
			expTime = 0
		} else {
			expTime = time.Unix(num, 0).Unix()
		}
	default:
		expTime = 0
	}

	timeNow := time.Now().Unix()
	log.Println("Токен ", token_string, " годен до", expTime, "а сейчас", timeNow)
	return expTime < timeNow
}
