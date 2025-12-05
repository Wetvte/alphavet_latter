package main

import (
	"fmt"
	"time"

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

	// Для работы с OAuth
	"golang.org/x/oauth2"
)

var server *http.Server
var githubOAuthConfig *oauth2.Config
var mongoClient *mongo.Client

func LoadEnv() {
	// Загружаем переменные из .env
	err := godotenv.Load("authoDB.env")
	if err != nil {
		log.Fatal("Ошибка загрузки .env:", err)
	}

}

func LaunchServer() {
	router := mux.NewRouter()
	router.HandleFunc("/", HomeHandler)
	router.HandleFunc("/login", LoginHandler)
	router.HandleFunc("/autho_callback", AuthoCallbackHandler)

	server = &http.Server{
		Addr:    ":8080",
		Handler: router,
	}
	server.ListenAndServe()
}
func CloseServer() {
	// Корректно останавливаем сервер
	if err := server.Close(); err != nil {
		log.Fatalf("Ошибка при остановке сервера: %v", err)
	}

	log.Println("Сервер остановлен")
}
func HomeHandler(w http.ResponseWriter, r *http.Request) {
	w.Write([]byte("Добро пожаловать в сервис авторизации!"))
}
func LoginHandler(w http.ResponseWriter, r *http.Request) {
	state := fmt.Sprintf("%x", time.Now().UnixNano())
	fmt.Printf("Current ClientID: %s\n", githubOAuthConfig.ClientID)
	url := githubOAuthConfig.AuthCodeURL(state, oauth2.AccessTypeOnline)
	fmt.Printf("Generated URL: %s\n", url) // проверьте, есть ли client_id в URL
	http.Redirect(w, r, url, http.StatusFound)
}
func AuthoCallbackHandler(w http.ResponseWriter, r *http.Request) {
	code := r.FormValue("code")

	// Получаем токен
	token, err := githubOAuthConfig.Exchange(context.Background(), code)
	if err != nil {
		http.Error(w, "Не удалось получить токен", http.StatusInternalServerError)
		return
	}

	// Используем токен для запросов к API GitHub
	client := githubOAuthConfig.Client(context.Background(), token)
	resp, err := client.Get("https://api.github.com/user")
	if err != nil {
		http.Error(w, "Ошибка запроса к GitHub API", http.StatusInternalServerError)
		return
	}
	defer resp.Body.Close()

	// Здесь можно сохранить данные пользователя в MongoDB
	w.Write([]byte("Успешная авторизация!"))
}
func CreateOAuthConfig() *oauth2.Config {
	log.Printf("ClientID из .env: %s", os.Getenv("GITHUB_CLIENT_ID"))
	log.Printf("ClientSecret из .env: %s", os.Getenv("GITHUB_CLIENT_SECRET"))
	log.Printf("Callback URL из .env: %s", os.Getenv("CALLBACK_URL"))
	return &oauth2.Config{
		ClientID:     os.Getenv("GITHUB_CLIENT_ID"),
		ClientSecret: os.Getenv("GITHUB_CLIENT_SECRET"),
		RedirectURL:  os.Getenv("CALLBACK_URL"),
		Scopes:       []string{"user", "repo"}, // запрашиваемые права
		Endpoint: oauth2.Endpoint{
			AuthURL:  "https://github.com/login/oauth/authorize",
			TokenURL: "https://github.com/login/oauth/access_token",
		},
	}
}

func ConnectToDatabase() {
	// Получаем строку подключения
	mongoURI := os.Getenv("MONGODB_URI")
	if mongoURI == "" {
		log.Fatal("Переменная MONGODB_URI не найдена в .env")
	}

	// Создаёт клиент mongoDB для чтения бд
	clientOptions := options.Client().ApplyURI(mongoURI)
	mongoClient, err := mongo.Connect(context.Background(), clientOptions)
	if err != nil {
		log.Fatal("Ошибка подключения к MongoDB:", err)
	}

	// Проверяет подключение
	err = mongoClient.Ping(context.Background(), nil)
	if err != nil {
		log.Fatal("Ошибка проверки подключения:", err)
	}

	log.Println("Успешно подключились к mongoDB")
}

// Закрыть соединение при завершении
func DisconnectFromDatabase() {
	err := mongoClient.Disconnect(context.Background())
	if err != nil {
		log.Fatal(err)
	}
}

func main() {
	LoadEnv()

	githubOAuthConfig = CreateOAuthConfig()

	LaunchServer()
	defer CloseServer()
}
