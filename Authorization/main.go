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
	"go.mongodb.org/mongo-driver/bson"
	"go.mongodb.org/mongo-driver/mongo"
	"go.mongodb.org/mongo-driver/mongo/options"

	// Для работы с OAuth
	"encoding/json"

	"golang.org/x/oauth2"
)

var server *http.Server
var githubOAuthConfig *oauth2.Config
var mongoClient *mongo.Client

func LoadEnv() {
	// Загружаем переменные из .env
	err := godotenv.Load("authoData.env")
	if err != nil {
		log.Fatal("Ошибка загрузки .env:", err)
	}

}

// Для работы с сервером
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
	defer CloseServer()
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
	// Получаем code из URL
	code := r.FormValue("code")
	if code == "" {
		http.Error(w, "Отсутствует code", http.StatusBadRequest)
		return
	}

	// Обмениваем code на токен
	token, err := githubOAuthConfig.Exchange(context.Background(), code)
	if err != nil {
		http.Error(w, "Не удалось получить токен", http.StatusInternalServerError)
		return
	}

	// Создаём авторизованный HTTP‑клиент
	client := githubOAuthConfig.Client(context.Background(), token)

	// Делаем запрос к GitHub API за данными пользователя
	resp, err := client.Get("https://api.github.com/user")
	if err != nil {
		http.Error(w, "Ошибка запроса к GitHub API", http.StatusInternalServerError)
		return
	}
	defer resp.Body.Close() // Обязательно закрываем тело ответа

	// Проверяем статус HTTP
	if resp.StatusCode != http.StatusOK {
		http.Error(w, fmt.Sprintf("Ошибка GitHub API: %d", resp.StatusCode), http.StatusBadGateway)
		return
	}

	// Читаем и декодируем JSON
	var userData map[string]interface{}
	err = json.NewDecoder(resp.Body).Decode(&userData)
	if err != nil {
		http.Error(w, "Ошибка декодирования JSON", http.StatusInternalServerError)
		return
	}

	ConnectToMongoDatabase()
	// Отбирает информацию
	requiredUserInfo := bson.M{
		"ghLogin": userData["login"], // GH Логин - string
		"ghId":    userData["id"],    // GH Айди - float64 (strconv.ParseFloat(userData["id"], 64))
		"ghName":  userData["name"],  // GH Имя - name
		"email":   userData["email"]}

	// Добавляет пользователя
	err = InsertUserToDatabase(mongoClient, requiredUserInfo)

	// Если ошибка, авторизируем заново
	if err != nil {
		log.Fatalf("Ошибка при создании пользователя: %v", err)
		http.Redirect(w, r, "/login", http.StatusFound)
	}

	DisconnectFromMongoDatabase()
}
func CreateOAuthConfig() *oauth2.Config {
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

// Для работы с БД
func ConnectToMongoDatabase() {
	// Проверяет, есть ли подключение уже. Если есть, останавливает попытку
	err := mongoClient.Ping(context.Background(), nil)
	if err == nil {
		return
	}

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

	log.Println("Успешное подключение к mongoDB")
}
func InsertUserToDatabase(client *mongo.Client, data bson.M) error {
	collection := client.Database("LettersProject").Collection("Users")
	_, err := collection.InsertOne(context.Background(), data)
	return err
}
func DisconnectFromMongoDatabase() {
	err := mongoClient.Disconnect(context.Background())
	if err != nil {
		log.Fatal(err)
	}
}

// Мэин
func main() {
	LoadEnv()

	githubOAuthConfig = CreateOAuthConfig()

	LaunchServer()
}
