package main

import (
	"log"

	"github.com/joho/godotenv"
)

// LoadEnv — загружает переменные окружения из .env-файла
func LoadEnv() {
	err := godotenv.Load("auth_config.env")
	if err != nil {
		log.Fatal("Ошибка загрузки .env:", err)
	}
}
