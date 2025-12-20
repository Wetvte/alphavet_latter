package main

import (
	"context"
	"crypto/tls"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"math/rand"
	"net/http"
	"net/url"
	"os"
	"os/signal"
	"strconv"
	"strings"
	"syscall"
	"time"

	"github.com/golang-jwt/jwt/v5"
	"github.com/gorilla/mux"
	"github.com/joho/godotenv"

	"go.mongodb.org/mongo-driver/bson"
	"go.mongodb.org/mongo-driver/bson/primitive"
	"go.mongodb.org/mongo-driver/mongo"
	"go.mongodb.org/mongo-driver/mongo/options"

	"golang.org/x/oauth2"
	"gopkg.in/gomail.v2"
)

/*type UserStatus int
Не получилось заюзать
const (
	Registring = iota 			   // Регистрируется сейчас
	AuthorizingWithService         // Входит через сервис
	WaitingAuthorizingConfirm        // Авторизировался в БД, но не получил данные для входа
	Authorized                     // Вошёл
	LogoutedGlobal				   // Запись есть, но нигде не авторизован
)*/

var server *http.Server
var mongoClient *mongo.Client

// LoadEnv — загружает переменные окружения из .env-файла
func LoadEnv() {
	err := godotenv.Load("auth_config.env")
	if err != nil {
		log.Fatal("Ошибка загрузки .env:", err)
	}
}

// LaunchServer — запускает HTTP-сервер с маршрутами
func LaunchServer() {
	router := mux.NewRouter()

	// Регистрируем обработчики для URL
	router.HandleFunc("/", HomeHandler)
	router.HandleFunc("/registration", RegistrationHandler)
	router.HandleFunc("/service_auth", ServiceAuthHandler)
	router.HandleFunc("/default_auth", DefaultAuthHandler)
	router.HandleFunc("/auth_callback", ServiceAuthCallbackHandler)
	router.HandleFunc("/check_auth", CheckAuthHandler)
	router.HandleFunc("/confirm_auth", ConfirmAuthHandler)
	router.HandleFunc("/update_auth", UpdateAuthHandler)
	router.HandleFunc("/logout_all", LogoutAllHandler)
	/// Вписать новые маршруты

	server = &http.Server{
		Addr:    ":" + os.Getenv("SERVER_PORT"),
		Handler: router, // Используем маршрутизатор gorilla/mux
	}

	log.Println("Сервер запускается...")
	// Канал для сигналов системы
	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	// Запуск сервера параллельно main
	go func() {
		err := server.ListenAndServe()
		if err != nil && err != http.ErrServerClosed {
			log.Fatalf("Ошибка сервера: %v", err)
		}
	}()
	log.Println("Сервер запущен на порту " + os.Getenv("SERVER_PORT"))
	// Подключение к БД
	ConnectToMongoDatabase()

	<-quit // Ждём SIGINT/SIGTERM
	log.Println("Получен сигнал. Останавливается сервер...")

	// Тайм‑аут на завершение текущих запросов
	ctx, cancel := context.WithTimeout(context.Background(), time.Second*3)
	defer cancel()

	if err := server.Shutdown(ctx); err != nil {
		log.Fatalf("Ошибка при остановке: %v", err)
		server.Close()
	}

	// Отключение от БД
	DisconnectFromMongoDatabase()
	log.Println("Сервер остановлен.")
}

// Handlers

// HomeHandler — обработчик для корневого URL (/)
func HomeHandler(w http.ResponseWriter, r *http.Request) {
	// Ничо не делает, хз, зачем сделал
}

// DefaultAuthHandler — авторизирует через код
func DefaultAuthHandler(w http.ResponseWriter, r *http.Request) {
	fmt.Println("Обработка авторизации через код.")
	fmt.Println("Получение данных из тела запроса.")
	var requestData map[string]interface{}
	err := json.NewDecoder(r.Body).Decode(&requestData)
	if err != nil {
		http.Error(w, "Чота не получилось прочитать тело запроса", http.StatusBadRequest)
	}
	fmt.Println("Получены данные в количестве: " + fmt.Sprint(len(requestData)))
	// Добываем логин токен из заголовков
	loginToken, err := GetTokenFromHeader(r.Header.Get("Authorization"))
	if err != nil || loginToken == "" {
		http.Error(w, "Это типа значит, что нет заголовка авторизации или токен некорректно передан", http.StatusBadRequest)
		return
	}
	// Получает переданные данные авторизации
	email := requestData["email"].(string)
	password := requestData["password"].(string)
	// Получает пользователя по почте
	userData, err := GetFromDatabaseByValue("Users", "email", email)
	// Проверяет наличие пользователя
	if err != nil || (*userData)["password"] == nil || (*userData)["password"] == "" {
		fmt.Println("Пользователя не существует.")
		json.NewEncoder(w).Encode(map[string]interface{}{
			"success_state": "fail",
			"message":       "Пользователь не найден.",
			"loginToken":    loginToken,
			"loginType":     "default",
		})
		fmt.Println("Отправлен ответ с провалом.")
		return
	} else { // И соответствие пароля
		fmt.Println("Проверка пароля.")
		if (*userData)["password"].(string) != password {
			fmt.Println("Пароль не соответствует.")
			json.NewEncoder(w).Encode(map[string]interface{}{
				"success_state": "fail",
				"message":       "Неверный пароль.",
				"loginToken":    loginToken,
				"loginType":     "default",
			})
			return
		} else { // Если нет несоответствий
			// Записываем в БД (Да, пользователь уже есть, но обновляем логин токен (остальные токены создаём) и способ входа)
			// Все основные данные остаются прежними (титульная информаця, пароль и другое)
			(*userData)["status"] = "WaitingAuthorizingConfirm"
			(*userData)["loginType"] = "default"
			(*userData)["loginToken"] = loginToken
			(*userData)["accessToken"] = GenerateNewAccessToken((*userData)["id"].(string))
			(*userData)["refreshToken"] = GenerateNewRefreshToken((*userData)["id"].(string))
			_, err := InsertUserToDatabase(userData)
			if err != nil {
				json.NewEncoder(w).Encode(map[string]interface{}{
					"success_state": "fail",
					"message":       "Ошибка авторизации, ну типа.",
					"loginToken":    loginToken,
					"loginType":     "default",
				})
				return
			}
			// Возвращает успех, если всё хорошо
			json.NewEncoder(w).Encode(map[string]interface{}{
				"success_state": "success",
				"message":       "Авторизация успешна.",
				"loginToken":    loginToken,
				"loginType":     "default",
			})
		}
	}
}

// ServiceAuthHandler — перенаправляет пользователя на страницу авторизации сервиса
func ServiceAuthHandler(w http.ResponseWriter, r *http.Request) {
	fmt.Println("Обработка авторизации через сервис.")
	fmt.Println("Получение данных из тела запроса.")
	var requestData map[string]interface{}
	err := json.NewDecoder(r.Body).Decode(&requestData)
	if err != nil {
		http.Error(w, "Чота не получилось прочитать тело запроса", http.StatusBadRequest)
	}
	fmt.Println("Получены данные в количестве: " + fmt.Sprint(len(requestData)))

	// Проверяет тип сервиса авторизации
	if requestData["serviceType"] != "github" && requestData["serviceType"] != "yandex" {
		http.Error(w, "А чо с запросом то", http.StatusBadRequest)
		return
	}
	// Добываем логин токен из заголовков
	loginToken, err := GetTokenFromHeader(r.Header.Get("Authorization"))
	if err != nil || loginToken == "" {
		http.Error(w, "Это типа значит, что нет заголовка авторизации или токен некорректно передан", http.StatusBadRequest)
		return
	}

	// Коллбэк авторизации для возвращения на клиент
	// Генерируем уникальный service_state
	service_state := fmt.Sprintf("%x", time.Now().UnixNano())
	log.Println("Сгенерирован service_state", service_state)
	// Сохраняем в БД, как юзера, для дальнейшей пляски (при коллбэке)
	id, err := InsertAuthorizationNoteToDatabase(&primitive.M{
		"status":                    "AuthorizingWithService",
		"auth_service_state":        service_state,
		"loginToken":                loginToken,
		"sessionToken":              requestData["sessionToken"],
		"loginType":                 requestData["serviceType"],
		"auth_service_callback_url": requestData["callback_url"],
	})
	log.Println("Перадан callback_url", requestData["callback_url"])

	// Если ошибка
	if err != nil || len(id) == 0 || id == "0" {
		log.Println(err)
	}
	// Создаём конфиг
	oAuthConfig, err := CreateOAuthConfig(requestData["serviceType"].(string))
	if err != nil { // Не создался конфиг, но такого не будет
		log.Println(err)
	}
	url := oAuthConfig.AuthCodeURL(service_state, oauth2.AccessTypeOnline) // Создание юрл с параметров стэйт
	log.Println("Отправляет данные для редиректа на ", url)
	json.NewEncoder(w).Encode(map[string]interface{}{
		"oauth_redirect_url": url,
	})
}

// ServiceAuthCallbackHandler — обрабатывает ответ от сервиса после авторизации
func ServiceAuthCallbackHandler(w http.ResponseWriter, r *http.Request) {
	// Получаем данные через state и проеряем их
	getted_state := r.URL.Query().Get("state")

	// По state получаем данные авторизации сервисом
	authData, err := GetFromDatabaseByValue("Authorizations", "auth_service_state", getted_state)
	callback_url := (*authData)["auth_service_callback_url"].(string) // Заранее берём коллбэк
	log.Println("Коллбэк для возврата токена сессии:", callback_url)
	// Проверка на успех получения
	if err != nil {
		// Некуда перенаправить, если всё так плохо
		http.Error(w, "State не совпадает, авторизация отменена", http.StatusBadRequest)
		http.Redirect(w, r, callback_url, http.StatusFound)
		return
	}

	// Получает тип входа и проверяет (но вряд ли будет ошибка)
	loginType := (*authData)["loginType"].(string) // r.URL.Query().Get("type")
	log.Println("Логин type:", loginType)
	if loginType != "github" && loginType != "yandex" {
		// Удаляет запись входа
		DeleteFromDatabase("Authorizations", "auth_service_state", getted_state)
		http.Error(w, "А где тип входа, сбстна (Ну или не соответствует)", http.StatusBadRequest)
		http.Redirect(w, r, callback_url, http.StatusFound)
		return
	}

	// Получаем code из URL
	code := r.FormValue("code") // code - первая переменная в query поле запроса
	log.Println("Code получен:", code)
	if code == "" {
		// Удаляет запись входа
		DeleteFromDatabase("Authorizations", "auth_service_state", getted_state)
		http.Error(w, "Отсутствует code, ", http.StatusBadRequest)
		http.Redirect(w, r, callback_url, http.StatusFound)
		return
	}

	// Создаём конфиг
	config, err := CreateOAuthConfig(loginType)
	if err != nil {
		// Удаляет запись входа
		DeleteFromDatabase("Authorizations", "auth_service_state", getted_state)
		http.Error(w, "Мда... ", http.StatusConflict)
		http.Redirect(w, r, callback_url, http.StatusFound)
		return
	}

	// А дальше уже совсем пиздец идёт, ес честно, я не знаю, как на самом деле можно сделать, но сейчас картина такая:
	// yandex - норм поц такой, может code через OAuth получить и выдать токены допуступа (оба).
	// но github вообще поехавший какой-то. Он мало того, что не поддерживает такие мувы и возвращает всё в query строке через левый запрос,
	// так ещё и не выдаёт refresh токен, а только access. УЖАС !!!

	// Создаём структуру данных-конфиг для обмены code
	url_values, err := CreateUrlValues(loginType)
	url_values.Set("code", code)
	if loginType == "yandex" {
		url_values.Set("grant_type", "authorization_code")
	}
	if err != nil { // Удаляет запись входа
		DeleteFromDatabase("Authorizations", "auth_service_state", getted_state)
		http.Error(w, "Мда... X2", http.StatusConflict)
		http.Redirect(w, r, callback_url, http.StatusFound)
		return
	}

	// Делаем пост запрос на нужный для сервиса url с нашими данными
	codeExchangeResponse, err := http.PostForm(config.Endpoint.TokenURL, *url_values)
	if err != nil { // Удаляет запись входа
		DeleteFromDatabase("Authorizations", "auth_service_state", getted_state)
		http.Error(w, "Ошибка при получении токена", http.StatusConflict)
		http.Redirect(w, r, callback_url, http.StatusFound)
		return
	}
	defer codeExchangeResponse.Body.Close()
	// Читаем тело ответа в отдельную переменную
	codeExchangeResponseBody, err := io.ReadAll(codeExchangeResponse.Body)
	log.Printf("Сырой ответ при обмене code: %s", string(codeExchangeResponseBody))

	// Проверяем статус ответа
	if err != nil || codeExchangeResponse.StatusCode != http.StatusOK { // Удаляет запись входа
		DeleteFromDatabase("Authorizations", "auth_service_state", getted_state)
		http.Error(w, "Ошибка при получении токена", http.StatusConflict)
		http.Redirect(w, r, callback_url, http.StatusFound)
		return
	}

	tokenData := make(map[string]string)
	// Обрабатывем в соответствии с типом сервиса его ответ и записываем токены
	if loginType == "yandex" {
		// Разбираем ответ (Яндекс возвращает JSON)
		var parsedData map[string]interface{}
		err := json.Unmarshal(codeExchangeResponseBody, &parsedData)
		if err != nil { // Удаляет запись входа
			DeleteFromDatabase("Authorizations", "auth_service_state", getted_state)
			http.Error(w, "Ошибка при получении токена.", http.StatusConflict)
			http.Redirect(w, r, callback_url, http.StatusFound)
			return
		}
		// Записываем токены
		tokenData["accessToken"] = parsedData["access_token"].(string)
		tokenData["refreshToken"] = parsedData["refresh_token"].(string)
		log.Println("От Яндекса получены токены: accessToken -", tokenData["accessToken"], "И refreshToken -", tokenData["refreshToken"])
	} else if loginType == "github" {
		// Разбираем ответ
		params, err := url.ParseQuery(string(codeExchangeResponseBody))
		if err != nil { // Удаляет запись входа
			DeleteFromDatabase("Authorizations", "auth_service_state", getted_state)
			http.Error(w, "Ошибка при обработке ответа за code.", http.StatusConflict)
			http.Redirect(w, r, callback_url, http.StatusFound)
			return
		}
		// Проверяем на ошибки
		if errorMsg := params.Get("error"); errorMsg != "" {
			DeleteFromDatabase("Authorizations", "auth_service_state", getted_state)
			http.Error(w, "Ошибка GitHub: "+params.Get("error_description"), http.StatusConflict)
			http.Redirect(w, r, callback_url, http.StatusFound)
			return
		}
		// Получаем access_token
		tokenData["accessToken"] = params.Get("access_token")
		tokenData["refreshToken"] = ""
		log.Println("От Github получен токен: accessToken -", tokenData["accessToken"])
	}

	// Проверка accessToken
	if tokenData["accessToken"] == "" {
		DeleteFromDatabase("Authorizations", "auth_service_state", getted_state)
		http.Error(w, "От Сервиса не получен токен доступа", http.StatusConflict)
		http.Redirect(w, r, callback_url, http.StatusFound)
		return
	}

	// Получает данные с клиента
	gettedUserData, err := GetServiceUserDataByAccessToken(tokenData["accessToken"], loginType)
	if err != nil {
		DeleteFromDatabase("Authorizations", "auth_service_state", getted_state)
		http.Error(w, "Ошибка при чтении данных с клиента", http.StatusConflict)
		http.Redirect(w, r, callback_url, http.StatusFound)
		return
	}

	// Сортирует полученные данные
	sortedUserData := SortServiceUserData(gettedUserData, loginType)
	log.Println("Имя пользователя: ", (*sortedUserData)["name"])
	log.Println("Емэил пользователя: ", (*sortedUserData)["email"])
	// На случай авторизации через gh
	var gh_login string
	if loginType == "github" {
		gh_login = (*sortedUserData)["login"].(string)
	} else {
		gh_login = ""
	}
	// Вставить в базу данных залогиненного пользователя,
	// но, перед этим, удаляет старую запись со статусом авторизации, так как с сервиса был получен емэил, а в записи его нет,
	// и, если вдруг пользователь уже был зареган с таким емэйлом, перезапишется его зареганая запись,
	// а запись с авторизацией так и будет висеть
	// ReplaceFullUserInDatabase не подойдёт, т.к. тогда перезапишется запись входа, а, если вдруг была запись зареганного
	// пользователя до этого, она не соединится с ней и будет две записи: для входа через код и через сервис;
	// удаляя запись об авторизации, новая запись будет сливаться по емэил с существующей записью регистрации/другой атворизации
	// P.S. Добавил коллекции в БД для авторизации и регистрации, так что механика поменялась
	DeleteFromDatabase("Authorizations", "id", (*authData)["id"].(string))
	_, err = InsertUserToDatabase(&primitive.M{
		"status":       "WaitingAuthorizingConfirm",
		"id":           (*authData)["id"].(string),
		"gh_login":     gh_login,
		"loginToken":   (*authData)["loginToken"].(string),
		"loginType":    loginType,
		"name":         (*sortedUserData)["name"],
		"email":        (*sortedUserData)["email"],
		"accessToken":  tokenData["accessToken"],
		"refreshToken": tokenData["refreshToken"],
	})
	// Есть важный недостаток: при авторизации через сервис, который не выдёт почту (GitHub), каждая авторизация создаст свою запись в БД
	if err != nil {
		log.Printf("Ошибка при создании пользователя: %v", err)
		http.Redirect(w, r, callback_url, http.StatusFound)
	}
	log.Println("Сохранение кук сессии ", (*authData)["sessionToken"].(string))
	// Ставим куки
	http.SetCookie(w, &http.Cookie{
		Name:     "sessionToken",
		Value:    (*authData)["sessionToken"].(string),
		Path:     "/",
		HttpOnly: true,
		// Secure: true,
		Domain: "localhost",
	})
	log.Println("Перенаправление на ", callback_url)
	http.Redirect(w, r, callback_url, http.StatusFound)
}

// Регистрация нового пользователя
func RegistrationHandler(w http.ResponseWriter, r *http.Request) {
	// Получает данные из тела запроса
	var requestData map[string]interface{}
	err := json.NewDecoder(r.Body).Decode(&requestData)
	if err != nil {
		http.Error(w, "Чота не получилось прочитать тело запроса", http.StatusBadRequest)
	}
	log.Println("Обработка запроса на регистрацию с параметром", requestData["stage"])

	// Если проверяет на возможность регистрации
	if requestData["stage"] == "filling" {
		log.Println("Приступаем к проверке данных на доступ к регистрации.")
		// Получает общие переданные данные регистрации
		email := requestData["email"].(string)
		name := requestData["name"].(string)
		password := requestData["password"].(string)
		// Проверяет имя
		message, err := CheckRegistrationName(name)
		if err != nil {
			log.Println(message)
			json.NewEncoder(w).Encode(map[string]interface{}{
				"success_state": "fail",
				"message":       message,
			})
			return
		}
		// Проверяет пароль
		message, err = CheckRegistrationPassword(password, requestData["repeat_password"].(string))
		if err != nil {
			log.Println(message)
			json.NewEncoder(w).Encode(map[string]interface{}{
				"access_state": "denied",
				"message":      message,
			})
			return
		}

		// Получает пользователя по почте
		userData, err := GetFromDatabaseByValue("Users", "email", email)
		// Проверяет наличие пользователя и пароля у него (если пользователь уже входил через сервисы, но не регался, даёт зарегаться)
		if err == nil {
			password, ok := (*userData)["password"]
			if ok && len(password.(string)) > 0 {
				log.Println("Пользователь уже зарегистрирован.")
				json.NewEncoder(w).Encode(map[string]interface{}{
					"access_state": "denied",
					"message":      "Пользователь уже зарегистрирован.",
				})
				return
			}
		}

		log.Println("Первая стадия проверки данных окончена. Приступаем к проверке действительности email.")
		// Генерирует код
		verifyCode := fmt.Sprint(rand.New(rand.NewSource(time.Now().UnixNano())).Intn(9000) + 1000) // от 1000 до 9999
		// Пытается отправить код по емэил
		err = SendConfirmationEmail(email, verifyCode)
		if err != nil {
			log.Println("Ошибка отправки email.")
			json.NewEncoder(w).Encode(map[string]interface{}{
				"access_state": "denied",
				"message":      "Ошибка при отправке подтверждения. Возможно, почта введена некорректно.",
			})
			return
		}
		log.Println("Email отправлен.")
		// Генерирует токен регистрации
		registrationToken := GenerateNewRegistrationToken(email)
		log.Println("Сгенерирован токен регистрации ", registrationToken)
		// Сохраняем в БД, как юзера, для дальнейшей пляски (при подтверждении)
		_, err = InsertRegistrationNoteToDatabase(&primitive.M{
			"status":            "Registring",
			"registrationToken": registrationToken,
			"verifyCode":        verifyCode,
			"email":             email,
			"name":              name,
			"password":          password,
		})
		// Если ошибка
		if err != nil {
			json.NewEncoder(w).Encode(map[string]interface{}{
				"access_state": "denied",
				"message":      "Ошибка при... Я не придумал.",
			})
			return
		}

		log.Println("Токен регистрации отправляется обратно. Ожидается подтверждение.")
		// Отправляет токен и статус обратно, как доказательство успеха
		json.NewEncoder(w).Encode(map[string]interface{}{
			"access_state":      "allowed",
			"message":           fmt.Sprintf("Регистрация доступна. Введите код, который был выслан на почтовый ящик %s.", email),
			"registrationToken": registrationToken,
		})
	} else /*verifing*/ { // Если подтверждает регистрацию
		// Получает токен из заголовка
		log.Println("Начинаем проверку кода подтверждения.")
		registrationToken, err := GetTokenFromHeader(r.Header.Get("Authorization"))
		if err != nil || registrationToken == "" {
			log.Println("Токен некорректен.")
			json.NewEncoder(w).Encode(map[string]interface{}{
				"access_state": "denied",
				"message":      "Это типа значит, что нет заголовка регистрации или токен некорректно передан",
			})
			return
		}
		log.Println("Подтверждаем регистрацию для токена", registrationToken)
		// Проверка времени регистрации
		if IsOwnTokenExpired(registrationToken) {
			// Удаляем запись и возвращаем ответ
			log.Println("Время регистрации истекло.")
			DeleteFromDatabase("Registrations", "registrationToken", registrationToken)
			json.NewEncoder(w).Encode(map[string]interface{}{
				"success_state": "fail",
				"message":       "Время регистрации истекло.",
			})
			return
		}
		// Получает данные и проверяет токен на соответствие
		regData, err := GetFromDatabaseByValue("Registrations", "registrationToken", registrationToken)
		// Проверяет наличие записи о регистрации
		if err != nil {
			// Удаляем запись и возвращаем ответ
			DeleteFromDatabase("Registrations", "registrationToken", registrationToken)
			json.NewEncoder(w).Encode(map[string]interface{}{
				"access_state": "denied",
				"message":      "Пользователь уже зарегистрирован. Каким образом было дано разрешение, ёпт?",
			})
			return
		}

		// Проверяет код подтверждения
		if (*regData)["verifyCode"].(string) != requestData["verifyCode"] {
			log.Println("Несовпадение кодов:", (*regData)["verifyCode"].(string), "и", requestData["verifyCode"])
			// Взвращаем ответъ, что код не тот
			json.NewEncoder(w).Encode(map[string]interface{}{
				"success_state": "fail",
				"message":       "Неверный код подтверждения.",
			})
			return
		}
		log.Println("Коды совпадают.")
		// Регистрирует пользователя в БД
		DeleteFromDatabase("Registrations", "id", (*regData)["id"].(string))
		_, err = InsertUserToDatabase(&primitive.M{
			"status":   "LogoutedGlobal",
			"name":     (*regData)["name"].(string),
			"email":    (*regData)["email"].(string),
			"password": (*regData)["password"].(string),

			"gh_login": "", // Нужен для привязки к gh.
			// Если акк на основе гх уже был до этой регистрации, его к ней привязать не получится.
			// Если авторизоваться через гх с открытой почтой после этой регистрации, он привяжется к записи
		})
		// *Если указать тип входа default на странице клиента при нажатии на кнопку входа, вход будет автоматическим
		if err != nil {
			json.NewEncoder(w).Encode(map[string]interface{}{
				"success_state": "fail",
				"message":       "Не удалось зарегистрировать пользователя.",
			})
			return
		}
		// И возвращает успех вместе с параметрами для входа
		json.NewEncoder(w).Encode(map[string]interface{}{
			"success_state": "success",
			"message":       "Регистрация успешна.",
		})
	}
}
func CheckRegistrationName(name string) (string, error) {
	// Проверяем заполненность имени
	if name == "" {
		return "Имя не может быть пустым.", fmt.Errorf("Чо за имя?")
	}
	// Проверка начала и окончания имени
	if name[0] == ' ' {
		return "Имя не может начинаться на пробел.", fmt.Errorf("Зачем...")
	}
	if name[len(name)-1] == ' ' {
		return "Имя не может оканчиваться на пробел.", fmt.Errorf("Зачем...")
	}
	// Отсутствие запрещённых символов
	availableSymbols := "АаБбВвГгДдЕеЁёЖжЗзИиЙйКкЛлМмНнОоПпРрСсТтУуФфХхЦцЧчШшЩщЪъЫыЬьЭэЮюЯяAaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz0123456789:^_-!?*()[]<> " // Пробел можно
	for _, char := range name {
		if !strings.ContainsRune(availableSymbols, char) {
			log.Println("Ошибка в имени: Недопустимый символ -", char)
			return "Имя может состоять только из русских или латинских букв, арабских цифр, символов :^_-!?*()[]<> и пробелов.", fmt.Errorf("Чо за имя?")
		}
	}
	// Длина
	min_duration, err := strconv.Atoi(os.Getenv("NAME_MIN_DURATION"))
	if err != nil {
		min_duration = 1
	}
	max_duration, err := strconv.Atoi(os.Getenv("NAME_MAX_DURATION"))
	if err != nil {
		max_duration = 10
	}
	if len(name) < min_duration || len(name) > max_duration {
		return fmt.Sprintf("Длина имени должна быть в пределах от %d до %d символов",
			min_duration, max_duration), err
	}
	return "Имя одобрено.", nil
}
func CheckRegistrationPassword(password string, repeat_password string) (string, error) {
	// Проверяем заполненность основных полей
	if password == "" {
		return "Пароль не может быть пустым.", fmt.Errorf("Чо за пароль?")
	}
	// Отсутствие запрещённых символов
	availableSymbols := "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz0123456789:^_-!?*()[]<> " // Пробел нельзя
	for _, char := range password {
		if !strings.ContainsRune(availableSymbols, char) {
			log.Println("Ошибка в пароле: Недопустимый символ -", char)
			return "Пароль может состоять только из латинских букв, арабских цифр и символов :^_-!?*()[]<>", fmt.Errorf("Чо за пароль?")
		}
	}
	// Длина
	min_duration, err := strconv.Atoi(os.Getenv("PASSWORD_MIN_DURATION"))
	if err != nil {
		min_duration = 1
	}
	max_duration, err := strconv.Atoi(os.Getenv("PASSWORD_MAX_DURATION"))
	if err != nil {
		max_duration = 10
	}
	if len(password) < min_duration || len(password) > max_duration {
		return fmt.Sprintf("Длина пароля должна быть в пределах от %d до %d символов",
			min_duration, max_duration), err
	}
	// Совпадение пароля
	if password != repeat_password {
		return "Пароли не совпадают.", err
	}
	return "Пароль одобрен.", nil
}
func SendConfirmationEmail(email string, code string) error {
	// Настройки SMTP
	smtpServer := "smtp.mail.ru"
	smtpPort := 465
	from := os.Getenv("MAIL")
	password := os.Getenv("MAIL_PASSWORD_OUTSIDE")

	// Текст письма
	subject := "Код подтверждения AlphavetLatter."
	body := fmt.Sprintf(
		"Здравствуйте!\n\nВаш код подтверждения: %s.\n\nИспользуйте его на сайте.", code)

	// Создаём сообщение
	m := gomail.NewMessage()
	m.SetHeader("From", from)
	m.SetHeader("To", email)
	m.SetHeader("Subject", subject)
	m.SetBody("text/plain", body)

	// Настраиваем SMTP-клиент с TLS
	d := gomail.NewDialer(smtpServer, smtpPort, from, password)
	d.TLSConfig = &tls.Config{InsecureSkipVerify: true} // Безопасное подключение будет при false, но на localhost не робит

	// Отправляем
	err := d.DialAndSend(m)
	log.Println("Результат ошибки отправки письма:", err)
	return err
}

// CheckAuthHandler — проверяет возможность авто авторизации по loginToken
func CheckAuthHandler(w http.ResponseWriter, r *http.Request) {
	log.Println("Запрос на проверку возможности входа пользователя.")
	// Добываем логин токен из заголовков
	loginToken, err := GetTokenFromHeader(r.Header.Get("Authorization"))
	if err != nil || loginToken == "" {
		http.Error(w, "Это типа значит, что нет заголовка авторизации или токен некорректно передан", http.StatusBadRequest)
		return
	}

	// Ищем пользователя по loginToken
	userData, err := GetFromDatabaseByValue("Users", "loginToken", loginToken)
	// Проверяем на успех поиска. (Можно укоротить, но не стал, т.к. мб сделаю проверку на истечение логинтокена)
	if err != nil {
		log.Println("Запрос отклонён.")
		// Пользователь не найден или ошибка
		json.NewEncoder(w).Encode(map[string]interface{}{
			"access_state": "denied",
			"message":      "Пользователь не найден.",
			"loginToken":   loginToken,
			"loginType":    (*userData)["loginType"],
		})
		return
	} else {
		log.Println("Запрос одобрен.")
		// Пользователь найден, не ошибка
		json.NewEncoder(w).Encode(map[string]interface{}{
			"access_state": "allowed",
			"message":      "Пользователь найден.",
			"loginToken":   loginToken,
			"loginType":    (*userData)["loginType"],
			"name":         (*userData)["name"],
		})
	}
}

// ConfirmAuthHandler - Подтверждает авторизацию по loginToken.
// (По сути, копия CheckAuth, но с отличиями - удаляет токен с БД и отправляет данные)
func ConfirmAuthHandler(w http.ResponseWriter, r *http.Request) {
	log.Println("Запрос на подтверждение входа и выдачу параметров.")
	// Добываем логин токен из заголовков
	loginToken, err := GetTokenFromHeader(r.Header.Get("Authorization"))
	if err != nil || loginToken == "" {
		http.Error(w, "Это типа значит, что нет заголовка авторизации или токен некорректно передан", http.StatusBadRequest)
		return
	}

	// Ищем пользователя по loginToken
	userData, err := GetFromDatabaseByValue("Users", "loginToken", loginToken)
	// Проверяем на успех поиска. (Можно укоротить, но не стал, т.к. мб сделаю проверку на истечение логинтокена)
	// + Если на конфирм перешли, то, скорее всего, запись есть, но, всё же, не помешает
	if err != nil {
		log.Println("Запрос отклонён.")
		// Пользователь не найден или ошибка
		json.NewEncoder(w).Encode(map[string]interface{}{
			"confirm_state": "fail",
			"message":       "Пользователь не найден.",
			"loginToken":    loginToken,
			"loginType":     (*userData)["loginType"].(string),
		})
		return
	} else {
		log.Println("Обновление статуса.")
		// Пользователь найден, не ошибка
		// Обновить запись в БД
		(*userData)["loginToken"] = ""                                         // Обнуляет строку токена входа, чтобы больше нельзя было получить данные по нему снова
		(*userData)["status"] = "Authorized"                                   // Пользователь считается авторизованным
		err := ReplaceFullUserInDatabase((*userData)["id"].(string), userData) // Заменит старые данные
		if err != nil {
			log.Println("Запрос на подтверждение входа отклонён из-за ошибки обновления статуса.")
			// Пользователь не обновлён
			json.NewEncoder(w).Encode(map[string]interface{}{
				"confirm_state": "fail",
				"message":       "Пользователь не найден.",
				"loginToken":    loginToken,
				"loginType":     (*userData)["loginType"].(string),
			})
			return
		}
		log.Println("Возвращение подтверждения.")
		// И вернуть
		log.Println("Вход одобрен для имени", (*userData)["name"].(string))
		json.NewEncoder(w).Encode(map[string]interface{}{
			"confirm_state": "success",
			"message":       "Вход одобрен.",
			"loginToken":    loginToken,
			"loginType":     (*userData)["loginType"].(string),

			"id":           (*userData)["id"].(string),
			"email":        (*userData)["email"].(string),
			"name":         (*userData)["name"].(string),
			"accessToken":  (*userData)["accessToken"].(string),
			"refreshToken": (*userData)["refreshToken"].(string),
		})
	}
}

// UpdateAuthHandler - Обработчик для проверки состояния
func UpdateAuthHandler(w http.ResponseWriter, r *http.Request) {
	log.Println("Получен запрос на обновление данных.")
	// Добываем рефреш токен из заголовков
	accessToken, err := GetTokenFromHeader(r.Header.Get("Authorization"))
	if err != nil || accessToken == "" {
		http.Error(w, "Отсутствует accessToken", http.StatusBadRequest)
		return
	}

	userData, err := GetFromDatabaseByValue("Users", "accessToken", accessToken)
	if err != nil {
		// Обработка отсутствия пользователя
	}
	loginType := (*userData)["loginType"].(string)

	// Обработка проверки обычной авторизации
	if loginType == "default" {
		log.Println("Обработка проверки обычной авторизации. Начнём с токенов.")
		if IsOwnTokenExpired(accessToken) {
			log.Println("Access токен истечён.")
			if IsOwnTokenExpired((*userData)["refreshToken"].(string)) {
				log.Println("И Refresh токен истечён.")
				// Обработка возврата без данных
				log.Println("Запрос на обновление данных провален.")
				json.NewEncoder(w).Encode(map[string]interface{}{
					"update_result": "fail",
				})
				return
			} else {
				// Обработка создания accessToken (refresh не обновляется)
				newAccessToken := GenerateNewAccessToken((*userData)["id"].(string))
				(*userData)["accessToken"] = newAccessToken
				// Сохранить в БД
				err := ReplaceFullUserInDatabase((*userData)["id"].(string), userData)
				if err != nil {
					log.Println("Запрос на обновление данных провален на этапе сохранения новых токенов.")
					json.NewEncoder(w).Encode(map[string]interface{}{
						"update_result": "fail",
					})
					return
				}
			}
		}
		// Если токен не истечён - Успешный возврат данных после окончания блока условий //
	} else {
		serviceUserData, err := GetServiceUserDataByAccessToken(accessToken, loginType)
		if err != nil {
			// Если вход через яндекс, пытается обновить токены
			if loginType == "yandex" {
				newAccessToken, newRefreshToken, err := GetNewTokensFromYandex((*userData)["refreshToken"].(string))
				if err == nil {
					serviceUserData, err = GetServiceUserDataByAccessToken(newAccessToken, loginType)
					if err != nil {
						log.Println("Запрос на обновление данных провален на этапе повторного получения данных по новому токену.")
						json.NewEncoder(w).Encode(map[string]interface{}{
							"update_result": "fail",
						})
						return
					}

					// Обновляет токены
					(*userData)["accessToken"] = newAccessToken
					(*userData)["refreshToken"] = newRefreshToken
					// Сортирует полученные данные
					actualUserData := SortServiceUserData(serviceUserData, loginType)
					log.Println("Имя пользователя: ", (*actualUserData)["name"])
					log.Println("Емэил пользователя: ", (*actualUserData)["email"])
					// Если в новых данных емэил привязан, ставит его
					var actual_email string
					if (*actualUserData)["email"] != "Not linked" {
						actual_email = (*actualUserData)["email"].(string)
					} else { // Иначе ставит его только если пользователь не зареган
						if password, ok := (*userData)["password"]; ok && len(password.(string)) > 0 {
							actual_email = (*userData)["email"].(string)
						} else {
							actual_email = "Not linked"
						}
					}
					// Обновляет в записи данные
					(*userData)["name"] = (*actualUserData)["name"]
					(*userData)["email"] = actual_email
					// Сохранить в БД
					err := ReplaceFullUserInDatabase((*userData)["id"].(string), userData)
					if err != nil {
						log.Println("Запрос на обновление данных провален на этапе сохранения новых токенов.")
						json.NewEncoder(w).Encode(map[string]interface{}{
							"update_result": "fail",
						})
						return
					}
				}
				// Не получилось обновить - дальше
			} else {
				// Если не получилось обновить токены Яндекса, или вход через GH, то попадает сюда
				// Обработка возврата без данных
				log.Println("Запрос на обновление данных провален на этапе получения информации с клиента.")
				json.NewEncoder(w).Encode(map[string]interface{}{
					"update_result": "fail",
				})
				return
			}
		}
		// Сюда попадает алгоритм, если получилось достать данные пользователя с сервиса (с первого раза или полсле обновления)
	}

	log.Println("Запрос на обновление данных успешно завершён.")
	// Отправить результат обновления
	json.NewEncoder(w).Encode(map[string]interface{}{
		"update_result": "success",

		"accessToken":  (*userData)["accessToken"].(string),
		"refreshToken": (*userData)["refreshToken"].(string),
		"id":           (*userData)["id"].(string),
		"name":         (*userData)["name"].(string),
		"email":        (*userData)["email"].(string),
	})
}

// Отправляет GET‑запрос с access_token
func GetUserDataFromService(accessToken string, serviceType string) (*map[string]interface{}, error) {
	// Создаём HTTP‑клиент с таймаутом
	client := &http.Client{
		Timeout: 10 * time.Second,
	}
	// Формируем запрос
	req, err := http.NewRequest("GET", GetServiceLoginURL(serviceType), nil)
	if err != nil {
		return nil, fmt.Errorf("ошибка создания запроса: %w", err)
	}
	// Добавляем заголовок Authorization
	// Для большинства сервисов: "Bearer <TOKEN>"
	// Для GitHub: "token <TOKEN>"
	if serviceType == "github" {
		req.Header.Set("Authorization", "token "+accessToken)
	} else {
		req.Header.Set("Authorization", "Bearer "+accessToken)
	}
	req.Header.Set("Accept", "application/json")

	// Выполняем запрос
	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("Ошибка выполнения запроса: %w", err)
	}
	defer resp.Body.Close()
	// Проверяем статус
	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("Неуспешный статус: %d", resp.StatusCode)
	}
	var resultData map[string]interface{}
	err = json.NewDecoder(resp.Body).Decode(&resultData)
	if err != nil {
		return nil, fmt.Errorf("Ошибка декодирования: %v\n", err)
	}
	return &resultData, nil
}
func GetNewTokensFromYandex(refreshToken string) (string, string, error) {
	// Формируем параметры запроса
	url_values := url.Values{}
	url_values.Set("grant_type", "refresh_token")
	url_values.Set("refresh_token", refreshToken)
	// Создаём POST‑запрос
	req, err := http.NewRequest(
		"POST",
		"https://oauth.yandex.ru/token",
		strings.NewReader(url_values.Encode()),
	)
	if err != nil {
		return "", "", fmt.Errorf("Ошибка создания запроса: %w", err)
	}
	// Устанавливаем заголовки
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	// Выполняем запрос
	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return "", "", fmt.Errorf("ошибка выполнения запроса: %w", err)
	}
	defer resp.Body.Close()
	// Проверяем статус
	if resp.StatusCode != http.StatusOK {
		return "", "", fmt.Errorf("HTTP ошибка: %d", resp.StatusCode)
	}

	// Декодируем ответ
	var response map[string]interface{}
	err = json.NewDecoder(resp.Body).Decode(&response)
	if err != nil {
		return "", "", fmt.Errorf("ошибка декодирования JSON: %w", err)
	}
	// Получаем access_token (обязательно, я сказал)
	var accessToken string
	if at, ok := response["access_token"]; ok {
		accessToken = at.(string)
	} else {
		return "", "", fmt.Errorf("в ответе отсутствует access_token")
	}
	// Получаем refresh_token (может отсутствовать)
	if rt, ok := response["access_token"]; ok {
		refreshToken = rt.(string)
	}
	return accessToken, refreshToken, nil
}

// LogoutAllHandler - Обработчик выхода со всех устройств
func LogoutAllHandler(w http.ResponseWriter, r *http.Request) {
	// Добываем рефреш токен из заголовков
	accessToken, err := GetTokenFromHeader(r.Header.Get("Authorization"))
	if err != nil || accessToken == "" {
		http.Error(w, "Отсутствует accessToken", http.StatusBadRequest)
		return
	}

	// Ищем пользователя по accessToken
	userData, err := GetFromDatabaseByValue("Users", "accessToken", accessToken)
	if err != nil {
		fmt.Println("Пользователь не найден, выход не осуществлён.")
		return
	}
	(*userData)["accessToken"] = ""
	(*userData)["refreshToken"] = ""
	(*userData)["loginType"] = ""
	(*userData)["status"] = "LogoutedGlobal"
	err = ReplaceFullUserInDatabase((*userData)["id"].(string), userData)
	if err != nil {
		fmt.Println("Пользователь найден, но выход не осуществлён.")
		return
	}
}

// Получает клиент, на котором подключается к сервису и получает данные пользователя с сервиса
func GetServiceUserDataByAccessToken(accessToken string, loginType string) (*map[string]interface{}, error) {
	// Создаём клиента по accessToken
	client := oauth2.NewClient(context.Background(), oauth2.StaticTokenSource(&oauth2.Token{AccessToken: accessToken}))
	// Получаем данные пользователя из Service API в виде ответа
	log.Println("Получение данных с клиента: 5%.")
	serviceResponse, err := client.Get(GetServiceLoginURL(loginType))
	if err != nil {
		return nil, err
	}
	defer serviceResponse.Body.Close() // На всякий случай не забыть закрыть созданный респонс
	// Проверяем чтобы ошибка - не ошибка. Типо
	log.Println("Получение данных с клиента: 55%.")
	if serviceResponse.StatusCode != http.StatusOK {
		return nil, err
	}
	// Декодируем тело ответа, получая из ответа с данными мапу данных ^_^
	var userData map[string]interface{}
	err = json.NewDecoder(serviceResponse.Body).Decode(&userData)
	if err != nil {
		return nil, err
	}
	log.Println("Получение данных с клиента: 100%.")
	return &userData, nil
}
func GetServiceLoginURL(serviceType string) string {
	switch serviceType {
	case "github":
		return "https://api.github.com/user"
	case "yandex":
		return "https://login.yandex.ru/info?format=json"
	default:
		return ""
	}
}

// CreateOAuthConfig — создаёт конфигурацию OAuth2
func CreateOAuthConfig(serviceType string) (*oauth2.Config, error) {
	fmt.Println("Создание конфига: " + serviceType)
	switch serviceType {
	case "github":
		return CreateGithubOAuthConfig(), nil
	case "yandex":
		return CreateYandexOAuthConfig(), nil
	default:
		return nil, fmt.Errorf("Ошибка создания кофига.")
	}
}
func CreateGithubOAuthConfig() *oauth2.Config {
	return &oauth2.Config{
		ClientID:     os.Getenv("GITHUB_CLIENT_ID"),
		ClientSecret: os.Getenv("GITHUB_CLIENT_SECRET"),
		RedirectURL:  os.Getenv("CALLBACK_URL"),
		Scopes:       []string{"user"},
		Endpoint: oauth2.Endpoint{
			AuthURL:  "https://github.com/login/oauth/authorize",
			TokenURL: "https://github.com/login/oauth/access_token",
		},
	}
}
func CreateYandexOAuthConfig() *oauth2.Config {
	return &oauth2.Config{
		ClientID:     os.Getenv("YANDEX_CLIENT_ID"),
		ClientSecret: os.Getenv("YANDEX_CLIENT_SECRET"),
		RedirectURL:  os.Getenv("CALLBACK_URL"),
		Scopes:       []string{"login:info", "login:email"},
		Endpoint: oauth2.Endpoint{
			AuthURL:  "https://oauth.yandex.ru/authorize",
			TokenURL: "https://oauth.yandex.ru/token",
		},
	}
}

func CreateUrlValues(serviceType string) (*url.Values, error) {
	fmt.Println("Создание конфига: " + serviceType)
	switch serviceType {
	case "github":
		return CreateGithubUrlValues(), nil
	case "yandex":
		return CreateYandexUrlValues(), nil
	default:
		return nil, fmt.Errorf("Ошибка создания кофига.")
	}
}
func CreateGithubUrlValues() *url.Values {
	return &url.Values{
		"client_id":     {os.Getenv("GITHUB_CLIENT_ID")},
		"client_secret": {os.Getenv("GITHUB_CLIENT_SECRET")},
		"redirect_uri":  {os.Getenv("CALLBACK_URL")},
	}
}
func CreateYandexUrlValues() *url.Values {
	return &url.Values{
		"client_id":     {os.Getenv("YANDEX_CLIENT_ID")},
		"client_secret": {os.Getenv("YANDEX_CLIENT_SECRET")},
		"redirect_uri":  {os.Getenv("CALLBACK_URL")},
	}
}

// Добываем логин токен из заголовков
func GetTokenFromHeader(header string) (string, error) {
	fmt.Println("Получение данных из заголовка: " + header)
	if header == "" {
		return "", fmt.Errorf("Заголовок пуст.")
	}
	// Проверяем формат: "Bearer <token>"
	parts := strings.Split(header, " ")
	if len(parts) != 2 || parts[0] != "Bearer" {
		return "", fmt.Errorf("Заголовок некорректен.")
	}
	return parts[1], nil // Наш токен
}

// Генерирует новые токены для этого типо сервиса
func GenerateNewAccessToken(id string) string {
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, jwt.MapClaims{
		"sub":  id,
		"type": "access",
		"exp":  time.Now().Add(time.Minute * 10).Unix(), // 10 минут
	})
	result, err := token.SignedString([]byte(os.Getenv("OWN_TOKEN_GENERATE_KEY")))
	if err != nil {
		result = ""
	}
	return result
}
func GenerateNewRefreshToken(id string) string {
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, jwt.MapClaims{
		"sub":  id,
		"type": "refresh",
		"exp":  time.Now().Add(time.Hour * 24 * 7).Unix(), // 7 дней
	})
	result, err := token.SignedString([]byte(os.Getenv("OWN_TOKEN_GENERATE_KEY")))
	if err != nil {
		result = ""
	}
	return result
}
func GenerateNewRegistrationToken(email string) string {
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, jwt.MapClaims{
		"sub":  email,
		"type": "registration",
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

// ConnectToMongoDatabase — подключает к MongoDB
func ConnectToMongoDatabase() {
	log.Println("Подключение к MongoDB...")
	if mongoClient != nil {
		return
	} // Если уже подключено — выходим

	mongoURI := os.Getenv("MONGODB_URI")
	if mongoURI == "" {
		log.Fatal("Переменная MONGODB_URI не найдена в .env")
	}

	var err error
	mongoClient, err = mongo.Connect(context.Background(), options.Client().ApplyURI(mongoURI))
	if err != nil {
		log.Fatal("Ошибка подключения к MongoDB: ", err)
	}

	// Проверяет подключение
	err = mongoClient.Ping(context.Background(), nil)
	if err != nil {
		log.Fatal("Ошибка проверки подключения:", err)
	}

	log.Println("Успешное подключение к MongoDB.")
}

// Вставляет пользователя в базу данных, или заменяет данные, если уже есть | id, err
// * ВАЖНО!!! Ипользовать ТОЛЬКО для добавления новых записей, а не с расчётом на втозамену старой записи с парочкой изменённых значений
// Если помещает запись об авторизации через сервис - добавляет без проверок и не смешивает (она потом удаляется кодом извне без замены/смешивания)
// Если добавляет запись о регистрации, она смешивается с уже имеющейся авторизацией через сервис (если есть), либо создаёт новую
// Если добавляет пользователя, его запись смешивается с суще
func InsertUserToDatabase(data *primitive.M) (string, error) {
	log.Println("Попытка добавить пользователя.")
	collection := mongoClient.Database("LatterProject").Collection("Users")
	log.Println("Добавление пользователя: 10%.")

	// Пробиваем по емэйл (если только он привязан)
	var userWithSameEmail *primitive.M
	var emailSearchErr error
	if email, hasEmail := (*data)["email"]; hasEmail && email.(string) != "" && email.(string) != "Not linked" {
		userWithSameEmail, emailSearchErr = GetFromDatabaseByValue("Users", "email", (*data)["email"].(string))
	} else {
		userWithSameEmail = nil
		emailSearchErr = fmt.Errorf("Нельзя проверить на возможность привязки к существующей записи.")
	}

	// Пробиваем по логину ГитХаба (если только он привязан). По феншую должно получиться успешно только при авторизации через гх
	var userWithSameGHLogin *primitive.M
	var ghloginSearchErr error
	if gh_login, hasGHLogin := (*data)["gh_login"]; hasGHLogin && gh_login.(string) != "" {
		userWithSameGHLogin, ghloginSearchErr = GetFromDatabaseByValue("Users", "email", (*data)["email"].(string))
	} else {
		userWithSameGHLogin = nil
		ghloginSearchErr = fmt.Errorf("Нельзя проверить на возможность привязки к существующей записи.")
	}

	log.Println("Добавление пользователя: 25%.")
	// Если емэйла такого нет среди аккаунтов
	// либо он есть, но у пользователя его логин не привязан к этому емаил.
	// (защита от ситуации, когда пользователь создал аккаунт на основе гх, не открыв почту, потом зарегал акк через код,
	// после чего открыл почту в гх и решил войти через него, чтобы не потерять данные от аккаунта на основе гх в результате
	// привязки его к новому регнутому аккаунту)
	if emailSearchErr != nil || (ghloginSearchErr == nil && (*userWithSameEmail)["id"] != (*userWithSameGHLogin)["id"]) {
		// Если найдена запись по логину гх
		if ghloginSearchErr == nil {
			log.Println("Добавление пользователя: 52%.")
			// Меняет данные, которые нужно и те, которые не помешало бы
			(*userWithSameGHLogin)["status"] = (*data)["status"]
			(*userWithSameGHLogin)["loginType"] = (*data)["loginType"]
			(*userWithSameGHLogin)["loginToken"] = (*data)["loginToken"]
			(*userWithSameGHLogin)["accessToken"] = (*data)["accessToken"]
			(*userWithSameGHLogin)["refreshToken"] = (*data)["refreshToken"]
			// Если найденная запись не зарегистрирована кодом, не помешает обновить данные имени и почты
			if password, ok := (*userWithSameGHLogin)["password"]; !ok || password == nil || len(password.(string)) == 0 {
				(*userWithSameGHLogin)["name"] = (*data)["name"]
				(*userWithSameGHLogin)["email"] = (*data)["email"]
			}
			log.Println("Добавление пользователя: 82%.")
			// Замена по логину гх
			_, err := collection.ReplaceOne(context.Background(),
				bson.M{"gh_login": (*userWithSameGHLogin)["gh_login"]}, userWithSameGHLogin)
			log.Println("Добавление пользователя: 100% (замена старой записи gh login с новыми параметрами).")
			return (*userWithSameGHLogin)["id"].(string), err
		} else { // Если это 100% новая запись для такого пользователя
			log.Println("Добавление пользователя: 55%.")
			// Если присвоен пользователю айди заранее
			if id, ok := (*data)["id"]; ok && len(id.(string)) > 0 {
				// Проверяет на занятость и, если нужно, присваивает новый
				_, err := GetFromDatabaseByValue("Users", "id", id.(string))
				if err != nil {
					(*data)["id"] = GenerateUniqueID()
				}
			} else { // Если не присвоем заранее, присваиваем
				(*data)["id"] = GenerateUniqueID()
			}
			// Инициализирует неназначенные изначальные данные // ini

			// Добавляет
			log.Println("Добавление пользователя: 85%.")
			_, err := collection.InsertOne(context.Background(), data)
			log.Println("Добавление пользователя: 100% (полностью новая запись).")
			return (*data)["id"].(string), err
		}
	} else { // Если уже есть такой емэил (и (только при входе через через gh) к нему привязан его логин),
		// перезаписываем данные, оставляя айди (и в моментах пароль)
		// Обновляем айди по
		log.Println("Добавление пользователя: 75%.")
		// Меняет данные, которые нужно и те, которые не помешало бы
		(*userWithSameEmail)["status"] = (*data)["status"]
		(*userWithSameEmail)["loginType"] = (*data)["loginType"]
		(*userWithSameEmail)["loginToken"] = (*data)["loginToken"]
		(*userWithSameEmail)["accessToken"] = (*data)["accessToken"]
		(*userWithSameEmail)["refreshToken"] = (*data)["refreshToken"]
		// Если найденная запись не зарегистрирована кодом, не помешает обновить данные имени и почты
		if password, ok := (*userWithSameEmail)["password"]; !ok || password == nil || len(password.(string)) == 0 {
			(*userWithSameEmail)["name"] = (*data)["name"]
		}
		// Если записывается вход через гх с открытой почтой (который 100% первый, т.к. не прокнуло условие намного выше), привязываем к найденной записи
		if new_gh_login, hasGHLogin := (*data)["gh_login"]; hasGHLogin && new_gh_login.(string) != "" {
			(*userWithSameEmail)["gh_login"] = new_gh_login.(string)
		}
		// Заменяет в БД
		_, err := collection.ReplaceOne(context.Background(),
			bson.M{"id": (*userWithSameEmail)["id"]}, userWithSameEmail)
		log.Println("Добавление пользователя: 100% (замена старой записи email с новыми параметрами).")
		return (*userWithSameEmail)["id"].(string), err
	}
}
func ReplaceFullUserInDatabase(id string, new_data *primitive.M) error {
	log.Println("Замена пользователя полностью по айди", id)
	collection := mongoClient.Database("LatterProject").Collection("Users")
	n_id, ok := (*new_data)["id"]
	if !ok || n_id == nil || n_id == "" {
		log.Println("Корректировка айди перед заменой.")
		(*new_data)["id"] = id
	}
	_, err := collection.ReplaceOne(context.Background(), bson.M{"id": id}, new_data)
	return err
}

func InsertAuthorizationNoteToDatabase(data *primitive.M) (string, error) {
	// Входя через сервис, повтора не будет, так что просто присваиваем айди и вставляем.
	collection := mongoClient.Database("LatterProject").Collection("Authorizations")
	(*data)["id"] = GenerateUniqueID()
	_, err := collection.InsertOne(context.Background(), data)
	log.Println("Добавление записи входа по", (*data)["id"].(string))
	return (*data)["id"].(string), err
}
func InsertRegistrationNoteToDatabase(data *primitive.M) (string, error) {
	// Входя через сервис, повтора не будет, так что просто присваиваем айди и вставляем.
	collection := mongoClient.Database("LatterProject").Collection("Registrations")
	(*data)["id"] = GenerateUniqueID()
	_, err := collection.ReplaceOne(context.Background(), &primitive.M{"email": (*data)["email"].(string)}, data,
		options.Replace().SetUpsert(true))
	log.Println("Добавление записи регистрации для", (*data)["email"].(string))
	return (*data)["id"].(string), err
}

// Возвращает из базы данных
func GetFromDatabaseByValue(db_name string, key_name string, key string) (*primitive.M, error) {
	fmt.Println("Получение из БД", db_name, "по ключу", key_name, "|", key)
	collection := mongoClient.Database("LatterProject").Collection(db_name)
	log.Println("Получение", db_name, ": 45%.")
	if collection == nil {
		return nil, fmt.Errorf("Не удалось получить коллекцию " + db_name)
	}
	log.Println("Получение", db_name, ": 65%,")
	// Принимает переменную и возвращает первого встреченного пользователя
	var user primitive.M
	err := collection.FindOne(
		context.Background(),
		primitive.M{key_name: key},
	).Decode(&user)
	log.Println("Получение", db_name, ": 100%.")
	return &user, err
}

// Удаляет всех пользователей с параметрами
func DeleteUsersByValue(key_name string, key string) error {
	collection := mongoClient.Database("LatterProject").Collection("Users")
	_, err := collection.DeleteMany(context.Background(), primitive.M{key_name: key})
	return err
}

// Удаляет, если есть, из БД
func DeleteFromDatabase(db_name string, key_name string, key string) error {
	fmt.Println("Удаление из БД", db_name, "по ключу", key_name, "|", key)
	collection := mongoClient.Database("LatterProject").Collection(db_name)
	if collection == nil {
		return fmt.Errorf("Не удалось получить коллекцию " + db_name)
	}
	result, err := collection.DeleteOne(context.Background(), primitive.M{key_name: key})
	if err != nil {
		return err
	}

	// Если ни один документ не был удалён
	if result.DeletedCount == 0 {
		return err // нечего удалять
	}

	return nil
}
func GenerateUniqueID() string {
	// Берём наносекунды с эпохи Unix
	timestamp := time.Now().UnixNano()
	// Добавляем случайное число от 0 до 999
	random := rand.Intn(1000)
	// Собираем в одну строку
	return fmt.Sprintf("%d%03d", timestamp, random)
}

// отключается от БД
func DisconnectFromMongoDatabase() {
	fmt.Println("Отключение от БД.")
	err := mongoClient.Disconnect(context.Background())
	if err != nil {
		log.Fatal(err)
	}
}

// Сортируют информацию пользователя сервисов, оставляя только нужное
// имя, мэил, данные
func SortServiceUserData(data *map[string]interface{}, serviceType string) *primitive.M {
	log.Println("Начало сортировки полученной информации.")
	var result *primitive.M
	switch serviceType {
	case "github":
		// Email
		var result_email string
		if email, ok := (*data)["email"]; ok && email != nil && len(email.(string)) > 0 {
			result_email = email.(string)
		} else {
			result_email = "Not linked"
		}
		// Name
		var result_name string
		if name, ok := (*data)["name"]; ok && name != nil && len(name.(string)) > 0 {
			result_name = name.(string)
		} else {
			result_name = (*data)["login"].(string)
		}
		result = &primitive.M{
			"login": (*data)["login"].(string),
			"name":  result_name,
			"email": result_email,
		}
	case "yandex":
		var result_email string
		if email, ok := (*data)["default_email"]; ok && email != nil && len(email.(string)) > 0 {
			result_email = email.(string)
		} else {
			result_email = "Not linked"
		}
		result = &primitive.M{
			"name":  (*data)["display_name"],
			"email": result_email,
		}
	default:
		result = &primitive.M{}
	}

	log.Println("Информация отсортирована.")
	return result
}

// Мэин
func main() {
	LoadEnv()
	LaunchServer()
}
