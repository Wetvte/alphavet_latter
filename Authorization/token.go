package main

import (
	"fmt"
	"time"

	"encoding/json"
	"log"
	"os"
	"strconv"

	"github.com/golang-jwt/jwt/v5"

	"go.mongodb.org/mongo-driver/bson/primitive"
)

// Генерирует базовые разрешения для роли
func GenerateBasePermissionsListForRole(role string) primitive.A {
	switch role {
	case "Student":
		return primitive.A{"user:roles:read", "user:disciplines:read", "user:tries:read", "user:fullname:write", "user:status:write",
			"discipline:teacherslist", "discipline:testslist", "discipline:student:add", "discipline:user:remove",
			"test:tries:read",
			"questions:list",
			"news:write"}
	case "Teacher":
		return primitive.A{"user:roles:read", "user:disciplines:read", "user:tries:read", "user:fullname:write", "user:status:write",
			"discipline:teacherslist", "discipline:testslist", "discipline:studentslist", "discipline:create", "discipline:delete",
			"discipline:text:write", "discipline:test:add", "discipline:teacher:add", "discipline:user:remove",
			"test:create", "test:text:write", "test:tries:read", "test:status:write", "test:question:add", "test:question:remove", "test:question:update",
			"questions:list", "question:read", "question:create", "question:update", "question:delete",
			"news:write"}
	case "Tester":
		return primitive.A{
			"users:list", "user:roles:read", "user:disciplines:read", "user:tries:read",
			"user:fullname:write", "user:roles:write", "user:status:write",
			"discipline:teacherslist", "discipline:testslist", "discipline:studentslist", "discipline:create", "discipline:delete",
			"discipline:teacher:add", "discipline:user:remove",
			"test:tries:read", "test:status:write", "test:question:remove",
			"questions:list", "question:read", "question:delete",
			"news:write"}
	case "Admin":
		return primitive.A{"users:note",
			"users:list", "user:roles:read", "user:disciplines:read", "user:tries:read",
			"user:fullname:write", "user:roles:write", "user:status:write", "user:block",
			"discipline:teacherslist", "discipline:testslist", "discipline:studentslist", "discipline:create", "discipline:delete",
			"discipline:teacher:add", "discipline:user:remove",
			"test:tries:read", "test:status:write", "test:question:remove",
			"questions:list", "question:read", "question:delete",
			"news:write"}
	default:
		return primitive.A{}
	}
}
func GenerateBasePermissionsListForRoles(roles primitive.A) primitive.A {
	var result primitive.A
	for _, role := range roles {
		result = CombinePermissions(result, GenerateBasePermissionsListForRole(role.(string)))
	}
	return result;
}

// Убирает повторения
func CombinePermissions(standart primitive.A, additions primitive.A) primitive.A {
	result := standart
	for _, permission := range additions {
		// Флаг найденного совпадения
		found := false

		// Перебираем элементы result
		for _, current := range result {
			if current.(string) == permission.(string) {
				found = true
				break
			}
		}

		if !found {
			result = append(result, permission)
		}
	}
	return result
}

// Генерирует новые токены для этого типо сервиса
func GenerateNewAccessToken(sub string, roles primitive.A, permissions primitive.A) string {
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, jwt.MapClaims{
		"sub":         sub,
		"type":        "access",
		"roles":       roles,
		"permissions": permissions,
		"exp":         GetTimeFromNow(60), // 1 минута
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
		"exp":  GetTimeFromNow(60 * 60 * 24 * 7), // 7 дней
	})
	result, err := token.SignedString([]byte(os.Getenv("OWN_TOKEN_GENERATE_KEY")))
	if err != nil {
		result = ""
	}
	return result
}
func GenerateNewAdminToken() string {
	return GenerateNewAccessToken("1", primitive.A{"Admin"}, GenerateBasePermissionsListForRole("Admin"))
}

// Парсит токен и возвращает claims
func ParseToken(token_string string) (*jwt.MapClaims, error) {
	var claims jwt.MapClaims
	token_parsed, parse_err := jwt.ParseWithClaims(token_string, &claims, func(token *jwt.Token) (interface{}, error) {
		_, success := token.Method.(*jwt.SigningMethodHMAC) // Если не схожи методы хэширования, сразу ошибка
		if success {
			return []byte(os.Getenv("OWN_TOKEN_GENERATE_KEY")), nil
		}
		log.Println("Токен ", token_string, " не подходит под дехэш.")
		return nil, jwt.ErrInvalidKey
	})
	if parse_err != nil || !token_parsed.Valid {
		log.Println("Токен", token_string, "недействителен.")
		return nil, fmt.Errorf("Ошибка разбора или недействительный токен.")
	}
	return &claims, nil
}

// Проверяет, токен, созданный этим типо сервисом, исчез, или нет
func IsOwnTokenExpired(token_string string) bool {
	claims, err := ParseToken(token_string)
	if err != nil {
		log.Println("Токен", token_string, "недействителен.")
		return true // ошибка разбора или недействительный токен
	}

	return IsOwnTokenClaimsExpired(claims)
}
func IsOwnTokenClaimsExpired(claims *jwt.MapClaims) bool {
	var expTime int64
	exp := (*claims)["exp"]
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

	return IsExpired(expTime)
}

// Проверяет, вышло ли время
func IsExpired(end int64) bool {
	current := time.Now().Unix()
	log.Println("Время истечения:", end, "а сейчас", current)
	return end < current
}

// Возвращает текущее время + секунды в unix
func GetTimeFromNow(seconds int) int64 {
	return time.Now().Add(time.Second * time.Duration(seconds)).Unix()
}
