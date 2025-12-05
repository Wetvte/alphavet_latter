package main

import (
	// Для запуска localhost
	"net/http"

	"github.com/gorilla/mux"
	
	// Для работы в MongoDB
	"context"
	"log"
	"os"

	"github.com/joho/godotenv"
	"go.mongodb.org/mongo-driver/mongo"
	"go.mongodb.org/mongo-driver/mongo/options"
)

func LaunchServer() {
	router := mux.NewRouter()
	router.HandleFunc("/", HomeHandler)
	router.HandleFunc("/login", HomeHandler)
	
	http.ListenAndServe(":8080", router)
}
func HomeHandler(w http.ResponseWriter, r *http.Request) {
	w.Write([]byte("Добро пожаловать в сервис авторизации!"))
}
func LoginHandler(w http.ResponseWriter, r *http.Request) {
	w.Write([]byte("Добро пожаловать на страницу авторизации!"))
}

func ConnectToDatabase() {
	// Загружаем переменные из .env
	err := godotenv.Load()
	if err != nil {
		log.Fatal("Ошибка загрузки .env:", err)
	}

	// Получаем строку подключения
	mongoURI := os.Getenv("MONGODB_URI")
	if mongoURI == "" {
		log.Fatal("Переменная MONGODB_URI не найдена в .env")
	}

	// Создаёт клиент mongoDB для чтения бд
	clientOptions := options.Client().ApplyURI(mongoURI)
	client, err := mongo.Connect(context.Background(), clientOptions)
	if err != nil {
		log.Fatal("Ошибка подключения к MongoDB:", err)
	}

	// Проверяет подключение
	err = client.Ping(context.Background(), nil)
	if err != nil {
		log.Fatal("Ошибка проверки подключения:", err)
	}

	log.Println("Успешно подключились к mongoDB")

	// Закрыть соединение при завершении
	defer func() {
		if err = client.Disconnect(context.Background()); err != nil {
			log.Fatal(err)
		}
	}()

	// Здесь код работы с БД
}

func main() {

}
