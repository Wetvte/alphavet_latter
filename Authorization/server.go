package main

import (
	"fmt"

	"os/signal"
	"syscall"

	"context"
	"crypto/tls"
	"encoding/json"
	"io"
	"log"
	"math/rand"
	"net/http"
	"net/url"
	"os"
	"strconv"
	"strings"
	"time"

	"github.com/gorilla/mux"

	"go.mongodb.org/mongo-driver/bson/primitive"

	"golang.org/x/oauth2"
	"gopkg.in/gomail.v2"
)

var server *http.Server

// LaunchServer — запускает HTTP-сервер с маршрутами
func LaunchServer() {
	router := mux.NewRouter()

	// Регистрируем обработчики для URL
	router.HandleFunc("/", HomeHandler)
	router.HandleFunc("/registration", RegistrationHandler)
	router.HandleFunc("/service_auth", ServiceAuthHandler)
	router.HandleFunc("/code_auth", CodeAuthHandler)
	router.HandleFunc("/default_auth", DefaultAuthHandler)
	router.HandleFunc("/auth_callback", ServiceAuthCallbackHandler)
	router.HandleFunc("/check_auth", CheckAuthHandler)
	router.HandleFunc("/confirm_auth", ConfirmAuthHandler)
	router.HandleFunc("/update_auth", UpdateAuthHandler)
	router.HandleFunc("/logout_all", LogoutAllHandler)

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

// Метод для отправки ответа
func WriteResponse(w *http.ResponseWriter, data *map[string]interface{}, status int) {
	(*w).WriteHeader(status)
	json.NewEncoder(*w).Encode(*data)
}

// HomeHandler — обработчик для корневого URL (/)
func HomeHandler(w http.ResponseWriter, r *http.Request) {
	// Ничо не делает, хз, зачем сделал
}

// DefaultAuthHandler — авторизирует обычной
func DefaultAuthHandler(w http.ResponseWriter, r *http.Request) {
	fmt.Println("Обработка авторизации обычной.")
	fmt.Println("Получение данных из тела запроса.")
	var requestData map[string]interface{}
	err := json.NewDecoder(r.Body).Decode(&requestData)
	if err != nil {
		http.Error(w, "Чота не получилось прочитать тело запроса", http.StatusBadRequest)
		return
	}
	fmt.Println("Получены данные в количестве: " + fmt.Sprint(len(requestData)))
	// Добываем логин токен из заголовков
	loginToken, err := GetTokenFromHeader(r.Header.Get("Authorization"))
	if err != nil || loginToken == "" {
		http.Error(w, "Это типа значит, что нет заголовка авторизации или токен некорректно передан", http.StatusUnauthorized)
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
		WriteResponse(&w, &map[string]interface{}{
			"message":    "Пользователь не найден.",
			"loginToken": loginToken,
			"loginType":  "default",
		}, 404)
		fmt.Println("Отправлен ответ с провалом.")
		return
	} else { // И соответствие пароля
		fmt.Println("Проверка пароля.")
		if (*userData)["password"].(string) != password {
			fmt.Println("Пароль не соответствует.")
			WriteResponse(&w, &map[string]interface{}{
				"message":    "Неверный пароль.",
				"loginToken": loginToken,
				"loginType":  "default",
			}, 403)
			return
		} else { // Если нет несоответствий
			// Записываем в БД (Да, пользователь уже есть, но обновляем логин токен (остальные токены создаём) и способ входа)
			// Все основные данные остаются прежними (титульная информаця, пароль и другое)
			(*userData)["status"] = "WaitingAuthorizingConfirm"
			(*userData)["loginToken"] = loginToken
			err := ReplaceFullUserInDatabase((*userData)["id"].(string), userData)
			if err != nil {
				WriteResponse(&w, &map[string]interface{}{
					"message":    "Ошибка авторизации, ну типа.",
					"loginToken": loginToken,
				}, 500)
				return
			}
			// Возвращает успех, если всё хорошо
			WriteResponse(&w, &map[string]interface{}{
				"message":    "Авторизация успешна.",
				"loginToken": loginToken,
				"loginType":  "default",
			}, 200)
		}
	}
}

// CodeAuthHandler - создаёт и отправляет код пользователю на аккаунт
func CodeAuthHandler(w http.ResponseWriter, r *http.Request) {
	fmt.Println("Обработка авторизации через код.")
	fmt.Println("Получение данных из тела запроса.")
	var requestData map[string]interface{}
	err := json.NewDecoder(r.Body).Decode(&requestData)
	if err != nil {
		http.Error(w, "Чота не получилось прочитать тело запроса", http.StatusBadRequest)
		return
	}
	fmt.Println("Получены данные в количестве: " + fmt.Sprint(len(requestData)))

	// Добываем почту
	email, ok := requestData["email"].(string)
	if !ok {
		// ошибку
	}

	// Если до этого не был отправлен код
	if requestData["stage"] == "cheking" {
		// Добываем логин токен из заголовков
		loginToken, err := GetTokenFromHeader(r.Header.Get("Authorization"))
		if err != nil || loginToken == "" {
			http.Error(w, "Это типа значит, что нет заголовка авторизации или токен некорректно передан", http.StatusUnauthorized)
			return
		}
		// Получаем пользователя
		_, err = GetFromDatabaseByValue("Users", "email", email)
		if err != nil {
			// ошибку
		}

		// Генерирует код
		code := GenerateCode(6)
		// Генерирует токен
		authorizationToken := GenerateNewAuthorizationToken(email)

		// Сохраняем в БД, для дальнейшей пляски
		_, err = InsertAuthorizationNoteToDatabase(&primitive.M{
			"status":             "AuthorizingWithCode",
			"sessionToken":       requestData["sessionToken"],
			"loginToken":         loginToken,
			"loginType":          "code",
			"code":               code,
			"email":              email,
			"authorizationToken": authorizationToken,
		})

		WriteResponse(&w, &map[string]interface{}{
			"authorizationToken": authorizationToken,
		}, 200)
	} else /*if requestData["stage"] == "verifing"*/ {
		// Если отправлен код
		// Добываем токен авторизации из заголовков
		authorizationToken, err := GetTokenFromHeader(r.Header.Get("Authorization"))
		if err != nil || authorizationToken == "" {
			http.Error(w, "Это типа значит, что нет заголовка авторизации или токен некорректно передан", http.StatusUnauthorized)
			return
		}

		// Получаем авторизацию
		authData, err := GetFromDatabaseByValue("Authorizations", "email", email)
		if err != nil {
			WriteResponse(&w, &map[string]interface{}{
				"message": "Ошибка получения записи об авторизации.",
			}, 500)
			return
		}
		// Добываем код
		code, ok := requestData["code"].(string)
		if !ok {
			WriteResponse(&w, &map[string]interface{}{
				"message": "Ошибка получения кода из записи об авторизации.",
			}, 500)
			return
		}
		// Сверяем код
		if code != (*authData)["code"].(string) {
			WriteResponse(&w, &map[string]interface{}{
				"message": "Коды не совпадают.",
			}, 400)
			return
		}

		// Удаляет запись об авторизации
		DeleteFromDatabase("Authorizations", "id", (*authData)["id"].(string))
		// Записывает авторизированного пользователя с персональными токенами
		_, err = InsertUserToDatabase(&primitive.M{
			"status":     "WaitingAuthorizingConfirm",
			"loginToken": (*authData)["loginToken"].(string),
			"email":      (*authData)["email"].(string),
		})
		if err != nil {
			WriteResponse(&w, &map[string]interface{}{
				"message": "Ошибка авторизации.",
			}, 500)
			return
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
		// Ответ по цепочке дойдёт до страницы, с которой будет запрос на автовход
		WriteResponse(&w, &map[string]interface{}{
			"message": "Авторизация успешна.",
		}, 200)
		return
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
		http.Error(w, "Это типа значит, что нет заголовка авторизации или токен некорректно передан", http.StatusUnauthorized)
		return
	}

	// Коллбэк авторизации для возвращения на клиент
	// Генерируем уникальный service_state
	service_state := fmt.Sprintf("%x", time.Now().UnixNano())
	log.Println("Сгенерирован service_state", service_state)
	// Сохраняем в БД, как юзера, для дальнейшей пляски (при коллбэке)
	_, err = InsertAuthorizationNoteToDatabase(&primitive.M{
		"status":                    "AuthorizingWithService",
		"auth_service_state":        service_state,
		"loginToken":                loginToken,
		"sessionToken":              requestData["sessionToken"],
		"loginType":                 requestData["serviceType"],
		"auth_service_callback_url": requestData["callback_url"],
	})
	log.Println("Передан callback_url", requestData["callback_url"])

	// Если ошибка
	if err != nil {
		log.Println(err)
	}
	// Создаём конфиг
	oAuthConfig, err := CreateOAuthConfig(requestData["serviceType"].(string))
	if err != nil { // Не создался конфиг, но такого не будет
		log.Println(err)
	}
	url := oAuthConfig.AuthCodeURL(service_state, oauth2.AccessTypeOnline) // Создание юрл с параметром стэйт
	log.Println("Отправляет данные для редиректа на ", url)
	WriteResponse(&w, &map[string]interface{}{
		"oauth_redirect_url": url,
	}, 200)
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
		return
	}

	// Получает тип входа и проверяет (но вряд ли будет ошибка)
	loginType := (*authData)["loginType"].(string) // r.URL.Query().Get("type")
	log.Println("Логин type:", loginType)
	if loginType != "github" && loginType != "yandex" {
		// Удаляет запись входа
		DeleteFromDatabase("Authorizations", "auth_service_state", getted_state)
		http.Error(w, "А где тип входа, сбстна (Ну или не соответствует)", http.StatusBadRequest)
		return
	}

	// Получаем code из URL
	code := r.FormValue("code") // code - первая переменная в query поле запроса
	log.Println("Code получен:", code)
	if code == "" {
		// Удаляет запись входа
		DeleteFromDatabase("Authorizations", "auth_service_state", getted_state)
		http.Error(w, "Отсутствует code.", http.StatusBadRequest)
		return
	}

	// Создаём конфиг
	config, err := CreateOAuthConfig(loginType)
	if err != nil {
		// Удаляет запись входа
		DeleteFromDatabase("Authorizations", "auth_service_state", getted_state)
		http.Error(w, "Мда... ", http.StatusInternalServerError)
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
		http.Error(w, "Мда... X2", http.StatusInternalServerError)
		return
	}

	// Делаем пост запрос на нужный для сервиса url с нашими данными
	codeExchangeResponse, err := http.PostForm(config.Endpoint.TokenURL, *url_values)
	if err != nil { // Удаляет запись входа
		DeleteFromDatabase("Authorizations", "auth_service_state", getted_state)
		http.Error(w, "Ошибка при получении токена", http.StatusInternalServerError)
		return
	}
	defer codeExchangeResponse.Body.Close()
	// Читаем тело ответа в отдельную переменную
	codeExchangeResponseBody, err := io.ReadAll(codeExchangeResponse.Body)
	log.Printf("Сырой ответ при обмене code: %s", string(codeExchangeResponseBody))

	// Проверяем статус ответа
	if err != nil || codeExchangeResponse.StatusCode != http.StatusOK { // Удаляет запись входа
		DeleteFromDatabase("Authorizations", "auth_service_state", getted_state)
		http.Error(w, "Ошибка при получении токена.", http.StatusInternalServerError)
		return
	}

	serviceTokenData := make(map[string]string)
	// Обрабатывем в соответствии с типом сервиса его ответ и записываем токены
	if loginType == "yandex" {
		// Разбираем ответ (Яндекс возвращает JSON)
		var parsedData map[string]interface{}
		err := json.Unmarshal(codeExchangeResponseBody, &parsedData)
		if err != nil { // Удаляет запись входа
			DeleteFromDatabase("Authorizations", "auth_service_state", getted_state)
			http.Error(w, "Ошибка при получении токена.", http.StatusInternalServerError)
			return
		}
		// Записываем токены
		serviceTokenData["accessToken"] = parsedData["access_token"].(string)
		serviceTokenData["refreshToken"] = parsedData["refresh_token"].(string)
		log.Println("От Яндекса получены токены: accessToken -", serviceTokenData["accessToken"], "И refreshToken -", serviceTokenData["refreshToken"])
	} else if loginType == "github" {
		// Разбираем ответ
		params, err := url.ParseQuery(string(codeExchangeResponseBody))
		if err != nil { // Удаляет запись входа
			DeleteFromDatabase("Authorizations", "auth_service_state", getted_state)
			http.Error(w, "Ошибка при обработке ответа за code.", http.StatusInternalServerError)
			return
		}
		// Проверяем на ошибки
		if errorMsg := params.Get("error"); errorMsg != "" {
			DeleteFromDatabase("Authorizations", "auth_service_state", getted_state)
			http.Error(w, "Ошибка GitHub: "+params.Get("error_description"), http.StatusInternalServerError)
			http.Redirect(w, r, callback_url, http.StatusFound)
			return
		}
		// Получаем access_token
		serviceTokenData["accessToken"] = params.Get("access_token")
		serviceTokenData["refreshToken"] = ""
		log.Println("От Github получен токен: accessToken -", serviceTokenData["accessToken"])
	}
	// Проверка accessToken
	if serviceTokenData["accessToken"] == "" {
		DeleteFromDatabase("Authorizations", "auth_service_state", getted_state)
		http.Error(w, "От Сервиса не получен токен доступа", http.StatusInternalServerError)
		return
	}

	// Получает данные с сайте с помощью клиента через токен доступа
	gettedUserData, err := GetUserDataFromService(serviceTokenData["accessToken"], loginType)
	if err != nil {
		DeleteFromDatabase("Authorizations", "auth_service_state", getted_state)
		http.Error(w, "Ошибка при чтении данных с клиента", http.StatusInternalServerError)
		return
	}
	// Сортирует полученные данные
	sortedUserData := SortServiceUserData(gettedUserData, loginType)
	log.Println("Имя пользователя: ", (*sortedUserData)["name"])
	log.Println("Емэил пользователя: ", (*sortedUserData)["email"])
	// Удаляет запись об авторизации
	DeleteFromDatabase("Authorizations", "id", (*authData)["id"].(string))
	// Записывает авторизированного пользователя с персональными токенами
	_, err = InsertUserToDatabase(&primitive.M{
		"status":     "WaitingAuthorizingConfirm",
		"id":         (*authData)["id"].(string),
		"loginToken": (*authData)["loginToken"].(string),
		"name":       (*sortedUserData)["name"],
		"email":      (*sortedUserData)["email"],
	})
	// Есть важный недостаток: при авторизации через сервис, который не выдёт почту (GitHub), каждая авторизация создаст свою запись в БД
	// P.S. Уже в прошлом, я научился получать почту с гх и без неё не будет входа - только ошибка
	if err != nil {
		log.Printf("Ошибка при создании пользователя: %v", err)
		http.Error(w, "Ошибка при создании пользователя", http.StatusInternalServerError)
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
			WriteResponse(&w, &map[string]interface{}{
				"message": message,
			}, 400)
			return
		}
		// Проверяет пароль
		message, err = CheckRegistrationPassword(password, requestData["repeat_password"].(string))
		if err != nil {
			log.Println(message)
			WriteResponse(&w, &map[string]interface{}{
				"message": message,
			}, 400)
			return
		}

		// Получает пользователя по почте
		userData, err := GetFromDatabaseByValue("Users", "email", email)
		// Проверяет наличие пользователя и пароля у него (если пользователь уже входил через сервисы, но не регался, даёт зарегаться)
		if err == nil {
			password, ok := (*userData)["password"]
			if ok && len(password.(string)) > 0 {
				log.Println("Пользователь уже зарегистрирован.")
				WriteResponse(&w, &map[string]interface{}{
					"message": "Пользователь уже зарегистрирован.",
				}, 409)
				return
			}
		}

		log.Println("Первая стадия проверки данных окончена. Приступаем к проверке действительности email.")
		// Генерирует код
		verifyCode := GenerateCode(6)
		// Пытается отправить код по емэил
		err = SendConfirmationEmail(email, verifyCode)
		if err != nil {
			log.Println("Ошибка отправки email.")
			WriteResponse(&w, &map[string]interface{}{
				"message": "Ошибка при отправке подтверждения. Возможно, почта введена некорректно.",
			}, 400)
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
			WriteResponse(&w, &map[string]interface{}{
				"message": "Ошибка при... Я не придумал.",
			}, 500)
			return
		}

		log.Println("Токен регистрации отправляется обратно. Ожидается подтверждение.")
		// Отправляет токен и статус обратно, как доказательство успеха
		WriteResponse(&w, &map[string]interface{}{
			"message":           fmt.Sprintf("Регистрация доступна. Введите код, который был выслан на почтовый ящик %s.", email),
			"registrationToken": registrationToken,
		}, 200)
	} else /*verifing*/ { // Если подтверждает регистрацию
		// Получает токен из заголовка
		log.Println("Начинаем проверку кода подтверждения.")
		registrationToken, err := GetTokenFromHeader(r.Header.Get("Authorization"))
		if err != nil || registrationToken == "" {
			log.Println("Токен некорректен.")
			WriteResponse(&w, &map[string]interface{}{
				"message": "Это типа значит, что нет заголовка регистрации или токен некорректно передан",
			}, 400)
			return
		}
		log.Println("Подтверждаем регистрацию для токена", registrationToken)
		// Проверка времени регистрации
		if IsOwnTokenExpired(registrationToken) {
			// Удаляем запись и возвращаем ответ
			log.Println("Время регистрации истекло.")
			DeleteFromDatabase("Registrations", "registrationToken", registrationToken)
			WriteResponse(&w, &map[string]interface{}{
				"message": "Время регистрации истекло.",
			}, 401)
			return
		}
		// Получает данные и проверяет токен на соответствие
		regData, err := GetFromDatabaseByValue("Registrations", "registrationToken", registrationToken)
		// Проверяет наличие записи о регистрации
		if err != nil {
			// Удаляем запись и возвращаем ответ
			DeleteFromDatabase("Registrations", "registrationToken", registrationToken)
			WriteResponse(&w, &map[string]interface{}{
				"message": "Пользователь уже зарегистрирован. Каким образом было дано разрешение, ёпт?",
			}, 409)
			return
		}

		// Проверяет код подтверждения
		if (*regData)["verifyCode"].(string) != requestData["verifyCode"] {
			log.Println("Несовпадение кодов:", (*regData)["verifyCode"].(string), "и", requestData["verifyCode"])
			// Взвращаем ответъ, что код не тот
			WriteResponse(&w, &map[string]interface{}{
				"message": "Неверный код подтверждения.",
			}, 400)
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
		})
		// *Если указать тип входа default на странице клиента при нажатии на кнопку входа, вход будет автоматическим
		if err != nil {
			WriteResponse(&w, &map[string]interface{}{
				"message": "Не удалось зарегистрировать пользователя.",
			}, 500)
			return
		}
		// И возвращает успех вместе с параметрами для входа
		WriteResponse(&w, &map[string]interface{}{
			"message": "Регистрация успешна.",
		}, 200)
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
		http.Error(w, "Это типа значит, что нет заголовка авторизации или токен некорректно передан", http.StatusUnauthorized)
		return
	}

	// Ищем пользователя по loginToken
	userData, err := GetFromDatabaseByValue("Users", "loginToken", loginToken)
	// Проверяем на успех поиска. (Можно укоротить, но не стал, т.к. мб сделаю проверку на истечение логинтокена)
	if err != nil {
		log.Println("Запрос отклонён.")
		// Пользователь не найден или ошибка
		WriteResponse(&w, &map[string]interface{}{
			"message":    "Пользователь не найден.",
			"loginToken": loginToken,
		}, 404)
		return
	} else {
		log.Println("Запрос одобрен.")
		// Пользователь найден, не ошибка
		WriteResponse(&w, &map[string]interface{}{
			"message":    "Пользователь найден.",
			"loginToken": loginToken,
			"name":       (*userData)["name"],
		}, 200)
	}
}

// ConfirmAuthHandler - Подтверждает авторизацию по loginToken.
// (По сути, копия CheckAuth, но с отличиями - удаляет токен с БД и отправляет данные)
func ConfirmAuthHandler(w http.ResponseWriter, r *http.Request) {
	log.Println("Запрос на подтверждение входа и выдачу параметров.")
	// Добываем логин токен из заголовков
	loginToken, err := GetTokenFromHeader(r.Header.Get("Authorization"))
	if err != nil || loginToken == "" {
		http.Error(w, "Это типа значит, что нет заголовка авторизации или токен некорректно передан", http.StatusUnauthorized)
		return
	}

	// Ищем пользователя по loginToken
	userData, err := GetFromDatabaseByValue("Users", "loginToken", loginToken)
	// Проверяем на успех поиска. (Можно укоротить, но не стал, т.к. мб сделаю проверку на истечение логинтокена)
	// + Если на конфирм перешли, то, скорее всего, запись есть, но, всё же, не помешает
	if err != nil {
		log.Println("Запрос отклонён.")
		// Пользователь не найден или ошибка
		WriteResponse(&w, &map[string]interface{}{
			"message":    "Пользователь не найден.",
			"loginToken": loginToken,
		}, 404)
		return
	} else {
		// Указывает, что пользователь не сможет подтвердить вход без авторизации
		(*userData)["loginToken"] = ""

		// Получить последний добавленный токен обновления
		var refreshToken string = ""
		if refreshTokensSlice, ok := (*userData)["refreshTokens"].([]interface{}); ok && len(refreshTokensSlice) > 0 {
			refreshToken = refreshTokensSlice[len(refreshTokensSlice)-1].(string)
			(*userData)["status"] = "Authorized" // Пользователь считается авторизованным до конца
		} else {
			(*userData)["status"] = "LogoutedGlobal" // Пользователь считается авторизованным до конца
		}

		// Обновляет запись в зависимости от успеха получения токена обновления
		log.Println("Обновление статуса.")
		err := ReplaceFullUserInDatabase((*userData)["id"].(string), userData) // Заменит старые данные
		if err != nil {
			log.Println("Запрос на подтверждение входа отклонён из-за ошибки обновления статуса.")
			// Пользователь не обновлён
			WriteResponse(&w, &map[string]interface{}{
				"message":    "Пользователь не обновлён.",
				"loginToken": loginToken,
			}, 500)
			return
		}

		// Если не удалось получить токен ранее, габелла
		if refreshToken == "" {
			log.Println("Запрос на подтверждение входа отклонён из-за отсутствия токена обновления.")
			// Пользователь не обновлён
			WriteResponse(&w, &map[string]interface{}{
				"message":    "Токен обновления утерян или не был присвоен.",
				"loginToken": loginToken,
			}, 401)
			return
		}

		log.Println("Возвращение подтверждения.")
		// И вернуть
		log.Println("Вход одобрен для имени", (*userData)["name"].(string))
		WriteResponse(&w, &map[string]interface{}{
			"message":    "Вход одобрен.",
			"loginToken": loginToken,

			"id":           (*userData)["id"].(string),
			"email":        (*userData)["email"].(string),
			"name":         (*userData)["name"].(string),
			"accessToken":  (*userData)["accessToken"].(string),
			"refreshToken": refreshToken,
		}, 200)
	}
}

// UpdateAuthHandler - Обработчик для получения данных при наличии токена доступа
func UpdateAuthHandler(w http.ResponseWriter, r *http.Request) {
	log.Println("Получен запрос на обновление данных.")
	// Добываем токен из заголовков
	accessToken, err := GetTokenFromHeader(r.Header.Get("Authorization"))
	if err != nil || accessToken == "" {
		http.Error(w, "Отсутствует accessToken", http.StatusUnauthorized)
		return
	}

	userData, err := GetFromDatabaseByValue("Users", "accessToken", accessToken)
	if err != nil {
		// Обработка отсутствия пользователя
		log.Println("Запрос на обновление данных провален. Пользователь не обнаружен.")
		WriteResponse(&w, &map[string]interface{}{
			"message": "Пользователь не найден.",
		}, 404)
		return
	}

	if IsOwnTokenExpired(accessToken) {
		log.Println("Запрос на обновление данных провален на этапе проверки актальности токена.")
		WriteResponse(&w, &map[string]interface{}{
			"message": "Токен недействителен.",
		}, 401)
		return
	}

	log.Println("Запрос на обновление данных успешно завершён.")
	// Отправить результат обновления
	WriteResponse(&w, &map[string]interface{}{
		"id":    (*userData)["id"].(string),
		"name":  (*userData)["name"].(string),
		"email": (*userData)["email"].(string),
	}, 200)
}

// RefreshAccessHandler - обработчик для обновления токенов
func RefreshAccessHandler(w http.ResponseWriter, r *http.Request) {
	log.Println("Получен запрос на обновление токенов.")
	// Добываем рефреш токен из заголовков
	refreshToken, err := GetTokenFromHeader(r.Header.Get("Authorization"))
	if err != nil || refreshToken == "" {
		http.Error(w, "Отсутствует refreshToken", http.StatusUnauthorized)
		return
	}
	userData, err := GetFromDatabaseByValue("Users", "refreshToken", refreshToken)
	if err != nil {
		// Обработка отсутствия пользователя
		log.Println("Запрос на обновление токенов провален. Пользователь не обнаружен.")
		WriteResponse(&w, &map[string]interface{}{
			"message": "Пользователь не найден.",
		}, 404)
		return
	}
	if IsOwnTokenExpired(refreshToken) {
		log.Println("Запрос на обновление токенов провален на этапе проверки актальности токена.")
		WriteResponse(&w, &map[string]interface{}{
			"message": "Токен недействителен.",
		}, 401)
		return
	}

	// Создаёт токены
	newAccessToken := GenerateNewAccessToken((*userData)["email"].(string),
		(*userData)["roles"].([]string), (*userData)["permissons"].([]string))
	newRefreshToken := GenerateNewRefreshToken((*userData)["email"].(string))

	// Заменяет новым токеном обновления старый
	refreshTokensSlice := (*userData)["refreshTokens"].([]interface{})
	for i := 0; i < len(refreshTokensSlice); i++ {
		if refreshTokensSlice[i].(string) == refreshToken {
			refreshTokensSlice[i] = newRefreshToken
			(*userData)["refreshTokens"] = refreshTokensSlice
			break
		}
	}
	// Обновляем токен доступа
	(*userData)["accessToken"] = newAccessToken

	log.Println("Запрос на обновление токенов успешно завершён.")
	// Отправить результат обновления
	WriteResponse(&w, &map[string]interface{}{
		"accessToken":  newAccessToken,
		"refreshToken": newRefreshToken,
	}, 200)
}

// LogoutAllHandler - Обработчик выхода со всех устройств
func LogoutAllHandler(w http.ResponseWriter, r *http.Request) {
	// Добываем рефреш токен из заголовков
	accessToken, err := GetTokenFromHeader(r.Header.Get("Authorization"))
	if err != nil || accessToken == "" {
		http.Error(w, "Отсутствует accessToken", http.StatusUnauthorized)
		return
	}

	// Ищем пользователя по accessToken
	userData, err := GetFromDatabaseByValue("Users", "accessToken", accessToken)
	if err != nil {
		fmt.Println("Пользователь не найден, выход не осуществлён.")
		WriteResponse(&w, &map[string]interface{}{
			"message": "Пользователь не найден.",
		}, 404)
		return
	}
	(*userData)["accessToken"] = ""
	(*userData)["refreshTokens"] = []interface{}{}
	(*userData)["status"] = "LogoutedGlobal"
	err = ReplaceFullUserInDatabase((*userData)["id"].(string), userData)
	if err != nil {
		fmt.Println("Пользователь найден, но выход не осуществлён.")
		WriteResponse(&w, &map[string]interface{}{
			"message": "Ошибка выхода.",
		}, 500)
	}
}

// Система для получения данных с посторонних серверов
// Отправляем Get запрос с токеном на указанный url
func DoGetRequestToService(url string, tokenPrefix string, accessToken string) (*http.Response, error) {
	// Создаём HTTP‑клиент с таймаутом
	client := &http.Client{
		Timeout: 10 * time.Second,
	}
	// Формируем запрос
	req, err := http.NewRequest("GET", url, nil)
	if err != nil {
		return nil, fmt.Errorf("ошибка создания запроса: %w", err)
	}
	// Добавляем заголовок Authorization
	// Для большинства сервисов: "Bearer <TOKEN>"
	// Для GitHub: "token <TOKEN>"
	req.Header.Set("Authorization", tokenPrefix+" "+accessToken)
	req.Header.Set("Accept", "application/json")

	// Выполняем запрос
	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("Ошибка выполнения запроса: %w", err)
	}
	// defer resp.Body.Close() не здесь
	return resp, nil
}

// Декодируем ответ в данные
func GetDataFromDefaultResponseBody(response io.ReadCloser) (*map[string]interface{}, error) {
	var data map[string]interface{}
	err := json.NewDecoder(response).Decode(&data)
	if err != nil {
		return nil, fmt.Errorf("Ошибка декодирования: %v\n", err)
	}
	return &data, nil
}
func GetDataFromArrayResponseBody(response io.ReadCloser) (*[]map[string]interface{}, error) {
	var data []map[string]interface{}
	err := json.NewDecoder(response).Decode(&data)
	if err != nil {
		return nil, fmt.Errorf("Ошибка декодирования: %v\n", err)
	}
	return &data, nil
}

// Отправляет GET‑запрос с access_token
func GetUserDataFromService(accessToken string, serviceType string) (*map[string]interface{}, error) {
	// Выбираем префикс для токена в заголовке
	var tokenPrefix string
	if serviceType == "github" {
		tokenPrefix = "token"
	} else {
		tokenPrefix = "Bearer"
	}
	// Получаем нужный юрл
	url := GetServiceLoginURL(serviceType)
	log.Println("Получение данных с клиента: 17%.")

	// Делаем запрос, полчаем ответ
	mainDataResponse, err := DoGetRequestToService(url, tokenPrefix, accessToken)
	// Проверяем статус
	if mainDataResponse.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("Неуспешный статус: %d", mainDataResponse.StatusCode)
	}
	defer mainDataResponse.Body.Close()
	log.Println("Получение данных с клиента: 35%.")
	// Получаем данные
	mainData, err := GetDataFromDefaultResponseBody(mainDataResponse.Body)
	if err != nil {
		return nil, fmt.Errorf("Ошибка декодирования: %v\n", err)
	}

	log.Println("Получение данных с клиента: 76%.")
	// Чебурда с получением емэила, если сервис - гит хаб, т.к. этот злыдень не хочет и email и логин возвращать нормально ВМЕСТЕ
	if serviceType == "github" {
		log.Println("ПОЛУЧЕНИЕ ДАННЫХ С GITHUB!")
		log.Println("ДЕЛАЕМ ЗАПРОС НА /emails")
		// Делаем запрос, полчаем ответ
		emailsDataResponse, err := DoGetRequestToService(url+"/emails", tokenPrefix, accessToken)
		if err != nil {
			return nil, fmt.Errorf("Ошибка декодирования: %v\n", err)
		}
		defer emailsDataResponse.Body.Close()
		// Проверяем статус, чтобы только при успешном пытаться получить емэил
		if emailsDataResponse.StatusCode == http.StatusOK {
			// Получаем данные
			log.Println("ЗАПРОС НОРМ!")
			emailsData, err := GetDataFromArrayResponseBody(emailsDataResponse.Body)
			if err == nil {
				log.Println("ПРОВЕРКА ДАННЫХ ЕМЭЙЛОВ!", emailsData)
				for _, emailData := range *emailsData {
					log.Println("ПРОВЕРКА ЕМЭЙЛА", emailData)
					// Проверяем поле "primary" (должно быть bool и true)
					primary, ok := emailData["primary"].(bool)
					if !ok || !primary {
						continue // Не основной — пропускаем
					}
					// Проверяем поле "verified" (должно быть bool и true)
					verified, ok := emailData["verified"].(bool)
					if !ok || !verified {
						continue // Не подтверждён — пропускаем
					}
					// Получаем email (должно быть строкой)
					email, ok := emailData["email"].(string)
					if !ok || email == "" {
						continue // Нет email или не строка — пропускаем
					}

					log.Println("ВОТ ОН НОРМ ТАКОЙ, ВОЗВРАЩАЕМ!")
					(*mainData)["email"] = email
					break
				}
			}
		}
	}

	log.Println("Получение данных с клиента: 100%.")
	return mainData, nil
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
			"name":  result_name,
			"email": result_email,
		}
	case "yandex":
		// Email
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

// Ссылки на ресурсы для получения данных по токену
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

// OAuth данные для запроса в виде config
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
		Scopes:       []string{"user", "user:email"},
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

// OAuth данные для запроса в виде url
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

// Генерация кода подтверждения
func GenerateCode(length int) string {
	multip := 10 * (length - 1)
	return fmt.Sprint(rand.New(rand.NewSource(time.Now().UnixNano())).Intn(9*multip) + multip)
}
