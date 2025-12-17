package main

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"math/rand"
	"net/http"
	"net/smtp"
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
)

/*type UserStatus int
Не получилось заюзать
const (
	Registing = iota 			   // Регистрируется сейчас
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
	var requestData map[string]string
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
	email := requestData["email"]
	password := requestData["password"]
	// Получает пользователя по почте
	userData, err := GetUserByValue("email", email)
	// Проверяет наличие пользователя
	if err != nil {
		fmt.Println("Пользователя не существует.")
		json.NewEncoder(w).Encode(map[string]string{
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
			json.NewEncoder(w).Encode(map[string]string{
				"success_state": "fail",
				"message":       "Неверный пароль.",
				"loginToken":    loginToken,
				"loginType":     "default",
			})
			return
		} else { // Если нет несоответствий
			// Записываем в БД (Да, пользователь уже есть, но обновляем логин токен (остальные токены создаём) и способ входа)
			_, err := InsertUserToDatabase(&primitive.M{
				"status":     "WaitingAuthorizingConfirm",
				"loginToken": loginToken,
				"loginType":  "default",
				// Новые токены входа для пользователя
				"accessToken":  GenerateNewAccessToken((*userData)["id"].(string)),
				"refreshToken": GenerateNewRefreshToken((*userData)["id"].(string)),
				// Пишу, т.к. иначе, после замены, у пользователя исчезнет имя, да и замена сработает только с совпадением емэил
				"name":  (*userData)["name"],
				"email": (*userData)["email"],
			})
			if err != nil {
				json.NewEncoder(w).Encode(map[string]string{
					"success_state": "fail",
					"message":       "Ошибка авторизации, ну типа.",
					"loginToken":    loginToken,
					"loginType":     "default",
				})
				return
			}
			// Возвращает успех, если всё хорошо
			json.NewEncoder(w).Encode(map[string]string{
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
	var requestData map[string]string
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
	id, err := InsertUserToDatabase(&primitive.M{
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
	oAuthConfig, err := CreateOAuthConfig(requestData["serviceType"])
	if err != nil { // Не создался конфиг, но такого не будет
		log.Println(err)
	}
	url := oAuthConfig.AuthCodeURL(service_state, oauth2.AccessTypeOnline) // Создание юрл с параметров стэйт
	log.Println("Отправляет данные для редиректа на ", url)
	json.NewEncoder(w).Encode(map[string]string{
		"oauth_redirect_url": url,
	})
}

// ServiceAuthCallbackHandler — обрабатывает ответ от сервиса после авторизации
func ServiceAuthCallbackHandler(w http.ResponseWriter, r *http.Request) {
	// Получаем данные через state и проеряем их
	getted_state := r.URL.Query().Get("state")
	authData, err := GetUserByValue("auth_service_state", getted_state)
	callback_url := (*authData)["auth_service_callback_url"].(string) // Заранее берём коллбэк
	log.Println("Коллбэк для возврата токена сессии:", callback_url)
	// На успех получения
	if err != nil {
		// Некуда перенаправить, если всё так плохо
		http.Error(w, "State не совпадает, авторизация отменена", http.StatusBadRequest)
		http.Redirect(w, r, callback_url, http.StatusFound)
	}
	// Получает тип входа и проверяет (но вряд ли будет ошибка)
	loginType := (*authData)["loginType"].(string) // r.URL.Query().Get("type")
	log.Println("Логин токен:", loginType)
	if loginType != "github" && loginType != "yandex" {
		// Удаляет запись входа
		DeleteUsersByValue("auth_service_state", getted_state)
		http.Error(w, "А где тип входа, сбстна (Ну или не соответствует)", http.StatusBadRequest)
		http.Redirect(w, r, callback_url, http.StatusFound)
	}

	// Получаем code из URL
	code := r.FormValue("code") // code - первая переменная в query поле запроса
	log.Println("Code получен:", code)
	if code == "" {
		// Удаляет запись входа
		DeleteUsersByValue("auth_service_state", getted_state)
		http.Error(w, "Отсутствует code, ", http.StatusBadRequest)
		http.Redirect(w, r, callback_url, http.StatusFound)
	}
	// Создаём конфиг
	config, err := CreateOAuthConfig(loginType)
	if err != nil {
		// Удаляет запись входа
		DeleteUsersByValue("auth_service_state", getted_state)
		http.Error(w, "Мда... ", http.StatusConflict)
		http.Redirect(w, r, callback_url, http.StatusFound)
	}
	// Обмениваем code на токен доступа
	token, err := config.Exchange(context.Background(), code)
	if err != nil {
		// Удаляет запись входа и идёт обратно
		DeleteUsersByValue("auth_service_state", getted_state)
		http.Error(w, "Не удалось получить токен, ", http.StatusInternalServerError)
		http.Redirect(w, r, callback_url, http.StatusFound)
	}
	if token == nil {
		log.Println("Токен nil, но ошибки нет — вероятно, проблема с форматом ответа")

		// Сырой ответ
		resp, _ := http.PostForm(config.Endpoint.TokenURL, url.Values{
			"client_id":     {config.ClientID},
			"client_secret": {config.ClientSecret},
			"code":          {code},
			"redirect_uri":  {config.RedirectURL},
		})
		body, _ := io.ReadAll(resp.Body)
		log.Printf("Сырой ответ GitHub: %s", string(body))
	}
	log.Println("Token получен:", token)

	// Создаём HTTP-клиент с токеном для запросов к GitHub API
	client := config.Client(context.Background(), token)
	// Получаем из него информацию
	userData, err := GetServiceUserDataFromClient(client, loginType)
	if err != nil {
		// Удаляет запись входа
		DeleteUsersByValue("auth_service_state", getted_state)
		http.Error(w, "Ошибка декодирования JSON", http.StatusInternalServerError)
		http.Redirect(w, r, callback_url, http.StatusFound)
	}
	sortedUserData := SortServiceUserData(userData, loginType)
	log.Println("Имя пользователя: ", (*sortedUserData)["name"])
	log.Println("Емэил пользователя: ", (*sortedUserData)["email"])
	// Вставить в базу данных залогиненного пользователя,
	// но, перед этим, удаляет старую запись со статусом авторизации, так как с сервиса был получен емэил, а в записи его нет,
	// и, если вдруг пользователь уже был зареган с таким емэйлом, перезапишется его зареганая запись,
	// а запись с авторизацией так и будет висеть
	DeleteUserFromDatabase("id", (*authData)["id"].(string))
	_, err = InsertUserToDatabase(&primitive.M{
		"status":       "WaitingAuthorizingConfirm",
		"id":           (*authData)["id"].(string),
		"loginToken":   (*authData)["loginToken"].(string),
		"loginType":    loginType,
		"name":         (*sortedUserData)["name"],
		"email":        (*sortedUserData)["email"],
		"accessToken":  token.AccessToken,
		"refreshToken": token.RefreshToken,
	})
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

	// Если проверяет на возможность регистрации
	if requestData["stage"] == "filling" {
		// Получает общие переданные данные регистрации
		email := requestData["email"].(string)
		name := requestData["name"].(string)
		password := requestData["password"].(string)
		// Проверяет имя
		message, err := CheckRegistrationName(name)
		if err != nil {
			json.NewEncoder(w).Encode(map[string]string{
				"success_state": "fail",
				"message":       message,
			})
			return
		}
		// Проверяет пароль
		message, err = CheckRegistrationPassword(password, requestData["repeat_password"].(string))
		if err != nil {
			json.NewEncoder(w).Encode(map[string]string{
				"access_state": "denied",
				"message":      message,
			})
			return
		}

		// Получает пользователя по почте
		userData, err := GetUserByValue("email", email)
		// Проверяет наличие пользователя и пароля у него (если пользователь входил через сервисы, но не регался, даёт зарегаться)
		if err != nil && len((*userData)["password"].(string)) > 0 {
			json.NewEncoder(w).Encode(map[string]string{
				"access_state": "denied",
				"message":      "Пользователь уже зарегистрирован.",
			})
			return
		}

		// Генерирует код
		verifyCode := fmt.Sprint(rand.New(rand.NewSource(time.Now().UnixNano())).Intn(9000) + 1000) // от 1000 до 9999
		// Пытается отправить код по емэил
		err = SendConfirmationEmail(email, verifyCode)
		if err != nil {
			json.NewEncoder(w).Encode(map[string]string{
				"access_state": "denied",
				"message":      "Ошибка при отправке подтверждения. Возможно, почта введена некорректно.",
			})
			return
		}
		// Генерирует токен регистрации
		registrationToken := GenerateNewRegistrationToken(email)
		// Сохраняем в БД, как юзера, для дальнейшей пляски (при подтверждении)
		id, err := InsertUserToDatabase(&primitive.M{
			"status":            "Registring",
			"registrationToken": registrationToken,
			"verifyCode":        verifyCode,
			"email":             email,
			"name":              name,
			"password":          password,
		})
		// Если ошибка
		if err != nil || len(id) == 0 || id == "0" {
			json.NewEncoder(w).Encode(map[string]string{
				"access_state": "denied",
				"message":      "Ошибка при... Я не придумал.",
			})
			return
		}

		// Отправляет токен и статус обратно, как доказательство успеха
		json.NewEncoder(w).Encode(map[string]string{
			"access_state":      "allowed",
			"message":           fmt.Sprintf("Регистрация доступна. Введите код, который был выслан на почтовый ящик %s.", email),
			"registrationToken": registrationToken,
		})
	} else /*verifing*/ { // Если подтверждает регистрацию
		// Получает токен из заголовка
		registrationToken, err := GetTokenFromHeader(r.Header.Get("Authorization"))
		if err != nil || registrationToken == "" {
			json.NewEncoder(w).Encode(map[string]string{
				"access_state": "denied",
				"message":      "Это типа значит, что нет заголовка регистрации или токен некорректно передан",
			})
			return
		}
		// Проверка времени регистрации
		if IsOwnTokenExpired(registrationToken) {
			// Удаляем запись и возвращаем ответ
			DeleteUsersByValue("registrationToken", registrationToken)
			json.NewEncoder(w).Encode(map[string]string{
				"success_state": "fail",
				"message":       "Время регистрации истекло.",
			})
			return
		}
		// Получает данные и проверяет токен на соответствие
		regData, err := GetUserByValue("registrationToken", registrationToken)
		// Проверяет наличие записи о регистрации
		if err != nil {
			// Удаляем запись и возвращаем ответ
			DeleteUsersByValue("registrationToken", registrationToken)
			json.NewEncoder(w).Encode(map[string]string{
				"access_state": "denied",
				"message":      "Пользователь уже зарегистрирован. Каким образом было дано разрешение, ёпт?",
			})
			return
		}

		// Проверяет код подтверждения
		if (*regData)["verifyCode"].(string) != requestData["verifyCode"] {
			// Удаляем запись и возвращаем ответ
			DeleteUsersByValue("registrationToken", registrationToken)
			json.NewEncoder(w).Encode(map[string]string{
				"success_state": "fail",
				"message":       "Неверный код подтверждения.",
			})
			return
		}
		// Создаёт токены доступа
		accessToken := GenerateNewAccessToken((*regData)["id"].(string))
		refreshToken := GenerateNewRefreshToken((*regData)["id"].(string))
		// Регистрирует пользователя в БД
		id, err := InsertUserToDatabase(&primitive.M{ // Если указать тип входа default на странице клиента при нажатии на вход, вход будет автоматическим
			"status":       "WaitingAuthorizingConfirm",
			"loginType":    "default",
			"loginToken":   (*regData)["loginToken"].(string),
			"name":         (*regData)["name"].(string),
			"email":        (*regData)["email"].(string),
			"password":     (*regData)["password"].(string),
			"access_token": accessToken,
			"refreshToken": refreshToken,
		})
		if err != nil || len(id) == 0 {
			json.NewEncoder(w).Encode(map[string]string{
				"success_state": "fail",
				"message":       "Не удалось зарегистрировать пользователя.",
			})
			return
		}
		// И возвращает успех вместе с параметрами для входа
		json.NewEncoder(w).Encode(map[string]string{
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
	availableSymbols := "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:^_-!?*()[]<> " // Пробел можно
	for _, char := range name {
		if !strings.ContainsRune(availableSymbols, char) {
			return "Имя может состоять только из латинских букв, арабских цифр, символов :^_-!?*()[]<> и пробелов.", fmt.Errorf("Чо за имя?")
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
	availableSymbols := "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:^_-!?*()[]<> " // Пробел нельзя
	for _, char := range password {
		if !strings.ContainsRune(availableSymbols, char) {
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
	// Настройки SMTP для mail
	smtpServer := "smtp.mail.ru:465"
	from := os.Getenv("MAIL")
	password := os.Getenv("MAIL_PASSWORD")

	// Текст письма
	subject := "Код подтверждения"
	body := fmt.Sprintf(
		"Здравствуйте!\n\nВаш код подтверждения: %s.\n\nИспользуйте его в приложении.", code)

	message := fmt.Sprintf("From: %s\r\n"+
		"To: %s\r\n"+
		"Subject: %s\r\n"+
		"\r\n%s", from, email, subject, body)

	auth := smtp.PlainAuth("", from, password, smtpServer[:len(smtpServer)-4])

	err := smtp.SendMail(smtpServer, auth, from, []string{email}, []byte(message))
	return err
}

// CheckAuthHandler — проверяет возможность авто авторизации по loginToken
func CheckAuthHandler(w http.ResponseWriter, r *http.Request) {
	// Добываем логин токен из заголовков
	loginToken, err := GetTokenFromHeader(r.Header.Get("Authorization"))
	if err != nil || loginToken == "" {
		http.Error(w, "Это типа значит, что нет заголовка авторизации или токен некорректно передан", http.StatusBadRequest)
		return
	}

	// Ищем пользователя по loginToken
	userData, err := GetUserByValue("loginToken", loginToken)
	// Проверяем на успех поиска. (Можно укоротить, но не стал, т.к. мб сделаю проверку на истечение логинтокена)
	if err != nil {
		// Пользователь не найден или ошибка
		json.NewEncoder(w).Encode(map[string]string{
			"access_state": "denied",
			"message":      "Пользователь не найден.",
			"loginToken":   loginToken,
			"loginType":    (*userData)["loginType"].(string),
		})
		return
	} else {
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
	// Добываем логин токен из заголовков
	loginToken, err := GetTokenFromHeader(r.Header.Get("Authorization"))
	if err != nil || loginToken == "" {
		http.Error(w, "Это типа значит, что нет заголовка авторизации или токен некорректно передан", http.StatusBadRequest)
		return
	}

	// Ищем пользователя по loginToken
	userData, err := GetUserByValue("loginToken", loginToken)
	// Проверяем на успех поиска. (Можно укоротить, но не стал, т.к. мб сделаю проверку на истечение логинтокена)
	// + Если на конфирм перешли, то, скорее всего, запись есть, но, всё же, не помешает
	if err != nil {
		// Пользователь не найден или ошибка
		json.NewEncoder(w).Encode(map[string]string{
			"confirm_state": "fail",
			"message":       "Пользователь не найден.",
			"loginToken":    loginToken,
			"loginType":     (*userData)["loginType"].(string),
		})
		return
	} else {
		// Пользователь найден, не ошибка
		// Обновить запись в БД
		(*userData)["loginToken"] = ""           // Обнуляет строку токена входа, чтобы больше нельзя было получить данные по нему снова
		(*userData)["status"] = "Authorized"     // Пользователь считается авторизованным
		_, err := InsertUserToDatabase(userData) // Автоматически заменит старые данные
		if err != nil {
			// Пользователь не обновлён
			json.NewEncoder(w).Encode(map[string]string{
				"confirm_state": "fail",
				"message":       "Пользователь не найден.",
				"loginToken":    loginToken,
				"loginType":     (*userData)["loginType"].(string),
			})
			return
		}
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
	refreshToken, err := GetTokenFromHeader(r.Header.Get("Authorization"))
	if err != nil || refreshToken == "" {
		http.Error(w, "Отсутствует refreshToken", http.StatusBadRequest)
		return
	}

	// Если найден - пытается получить новые токены (а, за одно, и пользователя)
	newAccessToken, newRefreshToken, oldUserData, err := GetNewTokens(refreshToken)
	// Проверяем на успех поиска
	if err != nil {
		// Пользователь не найден или ошибка
		if oldUserData == nil {
			log.Println("При обновлении токенов пользователь не найден.")
		}
		log.Println("Запрос на обновление данных провален на этапе получения токенов.")
		json.NewEncoder(w).Encode(map[string]string{
			"update_result": "fail",
		})
		return
	} else {
		// Собираем актуальные основные данные аккаунта
		var actualData *primitive.M
		// Если пользователь входил до этого через код
		if (*oldUserData)["loginType"].(string) == "default" {
			// Да, бред получать из БД, так как уже есть oldUserData, можно взять из него, но я решил сохранить принцип с accessToken
			userData, err := GetUserByValue("accessToken", newAccessToken)
			if err != nil {
				log.Println("Запрос на обновление данных провален на этапе обновления данных по токенам.")
				log.Printf("Ошибка при создании пользователя: %v", err)
				json.NewEncoder(w).Encode(map[string]string{
					"update_result": "fail",
				})
				return
			}
			actualData = &primitive.M{
				"name":  (*userData)["name"],
				"email": (*userData)["email"],
			}
		} else {
			// Создаём токен доступа на основе полученного токена
			// Создаём источник токенов, который не обновляет токен
			// Создаём транспорт, который использует этот источник
			// Создаём клиент с этим транспортом
			// Получаем из него информацию
			userData, err := GetServiceUserDataFromClient(&http.Client{Transport: &oauth2.Transport{
				Base: http.DefaultTransport,
				Source: oauth2.StaticTokenSource(&oauth2.Token{
					TokenType:   "Bearer",
					AccessToken: newAccessToken,
				}),
			}}, (*oldUserData)["loginType"].(string))
			if err != nil {
				log.Println("Запрос на обновление данных провален на этапе обновления данных по токенам.")
				log.Printf("Ошибка при создании пользователя: %v", err)
				json.NewEncoder(w).Encode(map[string]string{
					"update_result": "fail",
				})
				return
			}
			actualData = SortServiceUserData(userData, (*oldUserData)["loginType"].(string))
		}

		// Если в новых данных емэил привязан, ставит его
		var actual_email string
		if (*actualData)["email"] != "Not linked" {
			actual_email = (*actualData)["email"].(string)
		} else { // Иначе ставит его только если пользователь не зареган
			if password, ok := (*oldUserData)["password"]; ok && len(password.(string)) > 0 {
				actual_email = (*oldUserData)["email"].(string)
			} else {
				actual_email = "Not linked"
			}
		}
		// Вставить в базу данных новые данные пользователя
		id, err := InsertUserToDatabase(&primitive.M{
			"status":       "Authorized",
			"loginType":    (*oldUserData)["loginType"].(string),
			"loginToken":   "", // И так был недействителен, так как это не вход, а проверка на акутальность авторизации
			"accessToken":  "", // Уже недействителен, так как данные получены
			"refreshToken": newRefreshToken,
			"name":         (*actualData)["name"],
			"email":        actual_email,
		})
		if err != nil {
			log.Println("Запрос на обновление данных провален на этапе обновления данных в БД.")
			json.NewEncoder(w).Encode(map[string]string{
				"update_result": "fail",
			})
			return
		}
		// Отправить результат обновления
		json.NewEncoder(w).Encode(map[string]string{
			"update_result": "success",
			"refreshToken":  newRefreshToken,
			"id":            id,
			"name":          (*actualData)["name"].(string),
			"email":         actual_email,
		})
		log.Println("Запрос на обновление данных успешно завершён.")
	}
}

// LogoutAllHandler - Обработчик выхода со всех устройств
func LogoutAllHandler(w http.ResponseWriter, r *http.Request) {
	// Добываем рефреш токен из заголовков
	refreshToken, err := GetTokenFromHeader(r.Header.Get("Authorization"))
	if err != nil || refreshToken == "" {
		http.Error(w, "Отсутствует refreshToken", http.StatusBadRequest)
		return
	}

	// Ищем пользователя по refreshToken
	userData, err := GetUserByValue("refreshToken", refreshToken)
	if err != nil {
		fmt.Println("Пользователь не найден, выход не осуществлён.")
		return
	}
	(*userData)["refreshToken"] = ""
	(*userData)["status"] = "LogoutedGlobal"
	id, err := InsertUserToDatabase(userData)
	if id != (*userData)["id"].(string) || err != nil {
		fmt.Println("Пользователь найден, но выход не осуществлён.")
		return
	}
}

// Получает клиент, на котором подключается к сервису и получает данные пользователя с сервиса
func GetServiceUserDataFromClient(client *http.Client, loginType string) (*map[string]interface{}, error) {
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
	tokenParsed, parse_err := jwt.Parse(token_string, func(token *jwt.Token) (interface{}, error) {
		_, success := token.Method.(*jwt.SigningMethodHMAC) // Если не схожи методы хэширования, сразу ошибка
		if success {
			return nil, jwt.ErrInvalidKey
		}
		return os.Getenv("OWN_TOKEN_GENERATE_KEY"), nil
	})
	if parse_err != nil || !tokenParsed.Valid {
		return true // ошибка разбора или недействительный токен
	}
	claims, success := tokenParsed.Claims.(jwt.MapClaims)
	if success {
		return true // не удалось получить claims
	}
	expFloat, success := claims["exp"].(float64)
	if success {
		return true // поле exp отсутствует или не число
	}

	expTime := int64(expFloat)
	return expTime < time.Now().Unix()
}

// Получает новые токены
// access, refresh, err
func GetNewTokens(refreshToken string) (string, string, *primitive.M, error) {
	log.Println("Получение токенов.")
	userData, err := GetUserByValue("refreshToken", refreshToken)
	if err != nil {
		return "", "", nil, err // нет пользователя с таким токеном
	}
	// Если токены этого типо сервиса
	if (*userData)["loginType"].(string) == "default" {
		tokenParsed, err := jwt.Parse(refreshToken, func(token *jwt.Token) (interface{}, error) {
			if _, success := token.Method.(*jwt.SigningMethodHMAC); success { // Если методы хэширования не схожи, ошибка
				return nil, jwt.ErrInvalidKey
			}
			return os.Getenv("OWN_TOKEN_GENERATE_KEY"), nil
		})
		if err != nil || !tokenParsed.Valid {
			return "", "", userData, err // ошибка разбора или недействительный токен
		}
		claims, success := tokenParsed.Claims.(jwt.MapClaims)
		if success {
			return "", "", userData, err // не удалось получить claims
		}
		expTime, success := claims["exp"].(int64)
		if success {
			return "", "", userData, err // поле exp отсутствует или не число
		}
		if expTime < time.Now().Unix() {
			return "", "", userData, err // токен истечён
		}
		id, success := claims["sub"].(string)
		if success {
			return "", "", userData, err // claim 'sub' отсутствует или не строка
		}
		if id != (*userData)["id"].(string) {
			return "", "", userData, err // не знаю как, но айди пользователя не соответствует вверенному ему токену
		}
		// Создаём новые токены
		newAccessToken := GenerateNewAccessToken((*userData)["id"].(string))
		newRefreshToken := GenerateNewRefreshToken((*userData)["id"].(string))
		// Помещаем в бд
		(*userData)["accessToken"] = newAccessToken
		(*userData)["refreshToken"] = newRefreshToken
		_, err = InsertUserToDatabase(userData)
		if err != nil {
			return "", "", userData, err
		}
		return newAccessToken, newRefreshToken, userData, nil
	} else { // Если токены сервисов
		// Получаем конфигурацию
		config, err := CreateOAuthConfig((*userData)["loginType"].(string))
		if err != nil {
			fmt.Print(err) // Не создался конфиг, но такого не будет
		}
		// Создаём TokenSource с текущим refreshToken
		tokenSource := config.TokenSource(
			context.Background(),
			&oauth2.Token{RefreshToken: refreshToken},
		)
		// Получаем новый токен (автоматически обновится при необходимости)
		newToken, err := tokenSource.Token()
		if err != nil {
			return "", "", userData, err
		}

		// Вставляем токены в БД
		(*userData)["accessToken"] = newToken.AccessToken
		(*userData)["refreshToken"] = newToken.RefreshToken
		_, err = InsertUserToDatabase(userData) // Обрабатывать ошибку не вижу смысла
		if err != nil {
			return "", "", userData, err
		}
		log.Println("Токены успешно получены.")
		return newToken.AccessToken, newToken.RefreshToken, userData, nil
	}
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
func GetUserByValue(key_name string, key string) (*primitive.M, error) {
	fmt.Println("Получение из БД по ключу ", key_name, "|", key)
	collection := mongoClient.Database("LatterProject").Collection("Users")
	log.Println("Получение пользователя: 45%.")
	if collection == nil {
		return nil, fmt.Errorf("Не удалось создать коллекцию Users.")
	}
	log.Println("Получение пользователя: 65%,")
	// Принимает переменную и возвращает первого встреченного пользователя
	var user primitive.M
	err := collection.FindOne(
		context.Background(),
		primitive.M{key_name: key},
	).Decode(&user)
	log.Println("Получение пользователя: 100%.")
	return &user, err
}
func DeleteUsersByValue(key_name string, key string) error {
	collection := mongoClient.Database("LatterProject").Collection("Users")
	_, err := collection.DeleteMany(context.Background(), primitive.M{key_name: key})
	return err
}

// Вставляет пользователя в базу данных, или заменяет данные, если уже есть id, err
func InsertUserToDatabase(data *primitive.M) (string, error) {
	log.Println("Попытка добавить пользователя.")
	collection := mongoClient.Database("LatterProject").Collection("Users")
	log.Println("Добавление пользователя: 10%.")
	// Входя через сервис, повтора не будет, так что просто присваиваем айди и вставляем.
	if (*data)["status"].(string) == "AuthorizingWithService" {
		(*data)["id"] = GenerateUniqueID()
		_, err := collection.InsertOne(context.Background(), data)
		log.Println("Добавление пользователя: 100%.")
		return (*data)["id"].(string), err
	}

	// Пробиваем по емэйл (если только он привязан)
	var userWithSameEmail *primitive.M
	var err error
	if email, ok := (*data)["email"]; ok && email.(string) != "" && email.(string) != "Not linked" {
		userWithSameEmail, err = GetUserByValue("email", (*data)["email"].(string))
	} else {
		userWithSameEmail = nil
		err = fmt.Errorf("Нельзя проверить на возможность привязки к существующей записи.")
	}
	log.Println("Добавление пользователя: 25%.")
	// Если емэйла такого нет
	if err != nil {
		// Если присвоен пользователю айди заранее
		if id, ok := (*data)["id"]; ok && id.(string) != "" {
			// Проверяет на занятость и, если нужно, присваивает новый
			_, err := GetUserByValue("id", id.(string))
			if err != nil {
				(*data)["id"] = GenerateUniqueID()
			}
		}
		// Вставляет в БД
		_, err := collection.InsertOne(context.Background(), data)
		log.Println("Добавление пользователя: 100%.")
		return (*data)["id"].(string), err
	} else { // Если уже есть такой емэил, перезаписываем данные, оставляя айди, и
		(*data)["id"] = (*userWithSameEmail)["id"].(string)
		log.Println("Добавление пользователя: 40%.")
		// если у старой записи есть пароль...
		if password, ok := (*userWithSameEmail)["password"]; ok && len(password.(string)) > 0 {
			log.Println("Добавление пользователя: 60%.")
			// Если текущая авторизация проведена через сервис
			if (*data)["loginType"].(string) != "default" {
				log.Println("Добавление пользователя: 70%.")
				// Новым данным передаём пароль для будущей авторизации через код
				// (если через код, то пусть пароль будет переданный, если он передан, ведь в теории возможна смена пароля)
				(*data)["password"] = password.(string)
			}
			// Если у старой записи есть имя, оно остаётся (т.к. это имя указано при регистрации, или изменен на сайте)
			// (т.к. пароль сохранён, при его установке поставилось имя с сайта, поэтому меняем только если нет каким-то образом имени)
			if name, ok := (*userWithSameEmail)["name"]; ok && len(name.(string)) > 0 {
				log.Println("Добавление пользователя: 80%.")
				(*data)["name"] = name.(string)
			}
		}
		// Вставляет в БД
		_, err := collection.ReplaceOne(context.Background(), bson.M{"id": (*userWithSameEmail)["id"]}, data)
		log.Println("Добавление пользователя: 100%.")
		return (*data)["id"].(string), err
	}
}

// Удаляет, если есть, из БД
func DeleteUserFromDatabase(key_name string, key string) error {
	fmt.Println("Удаление из БД по по ключу " + key_name)
	collection := mongoClient.Database("LatterProject").Collection("Users")

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
	switch serviceType {
	case "github":
		var result_email string
		if email, ok := (*data)["email"]; ok && len(email.(string)) > 0 {
			result_email = email.(string)
		} else {
			result_email = "Not linked"
		}
		return &primitive.M{
			"name":  (*data)["login"],
			"email": result_email,
		}
	case "yandex":
		var result_email string
		if email, ok := (*data)["default_email"]; ok && len(email.(string)) > 0 {
			result_email = email.(string)
		} else {
			result_email = "Not linked"
		}
		return &primitive.M{
			"name":  (*data)["display_name"],
			"email": result_email,
		}
	default:
		return &primitive.M{}
	}
}

// Мэин
func main() {
	LoadEnv()
	LaunchServer()
}
