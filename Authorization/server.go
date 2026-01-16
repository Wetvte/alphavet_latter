package main

import (
	"fmt"

	"os/signal"
	"syscall"

	"bytes"
	"context"
	"crypto/tls"
	"encoding/json"
	"io"
	"log"
	"math"
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
	router.HandleFunc("/init/registration", InitRegistrationHandler)
	router.HandleFunc("/verify/registration", VerifyRegistrationHandler)
	router.HandleFunc("/auth/service", ServiceAuthHandler)
	router.HandleFunc("/init/authorization", InitAuthorizationHandler)     // code
	router.HandleFunc("/verify/authorization", VerifyAuthorizationHandler) // code
	router.HandleFunc("/auth/default", DefaultAuthHandler)
	router.HandleFunc("/auth_callback", ServiceAuthCallbackHandler)
	router.HandleFunc("/auth/check", CheckAuthHandler)
	router.HandleFunc("/auth/confirm", ConfirmAuthHandler)
	router.HandleFunc("/sub", GetSubHandler)
	router.HandleFunc("/refresh", RefreshAccessHandler)
	router.HandleFunc("/logout/all", LogoutAllHandler)
	router.HandleFunc("/logout/user", LogoutUserHandler)
	router.HandleFunc("/users/data/change", UserDataChangeHandler)

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

	if message, ok := (*data)["message"]; ok {
		log.Println("Отправлен статус", status, "с сообщением:", message.(string))
	} else {
		log.Println("Отправлен статус", status)
	}
}

// HomeHandler — обработчик для корневого URL (/)
func HomeHandler(w http.ResponseWriter, r *http.Request) {
	// Ничо не делает, хз, зачем сделал
}

// DefaultAuthHandler — авторизирует обычной
func DefaultAuthHandler(w http.ResponseWriter, r *http.Request) {
	fmt.Println("Обработка авторизации обычной.")
	requestData, err := GetDataFromBody(&r.Body)
	if err != nil {
		WriteResponse(&w, &map[string]interface{}{
			"message": "Чота не получилось прочитать тело запроса.",
		}, 400)
		return
	}
	// Добываем логин токен из заголовков
	login_token, err := GetTokenFromHeader(r.Header.Get("Authorization"))
	if err != nil || login_token == "" {
		WriteResponse(&w, &map[string]interface{}{
			"message": "Это типа значит, что нет заголовка авторизации или токен некорректно передан.",
		}, 400)
		return
	}
	// Получает переданные данные авторизации
	email := (*requestData)["email"].(string)
	password := (*requestData)["password"].(string)
	// Получает пользователя по почте
	userData, err := GetFromDatabaseByValue("Users", "email", email)
	// Проверяет наличие пользователя
	if err != nil || (*userData)["password"] == nil || (*userData)["password"] == "" {
		fmt.Println("Пользователя не существует.")
		WriteResponse(&w, &map[string]interface{}{
			"message":     "Пользователь не найден.",
			"login_token": login_token,
		}, 404)
		fmt.Println("Отправлен ответ с провалом.")
		return
	} else { // И соответствие пароля
		fmt.Println("Проверка пароля.")
		if (*userData)["password"].(string) != password {
			fmt.Println("Пароль не соответствует.")
			WriteResponse(&w, &map[string]interface{}{
				"message":     "Неверный пароль.",
				"login_token": login_token,
			}, 403)
			return
		} else { // Если нет несоответствий
			// Запрос на модуль тестировани для получения статуса пользователя
			user_server_status, err, message := GetUserServerStatus((*userData)["id"].(string))
			if err != nil {
				fmt.Println(message)
				WriteResponse(&w, &map[string]interface{}{
					"message":     message,
					"login_token": login_token,
				}, 500)
				return
			}
			// Проверяем статус
			if user_server_status == "Blocked" {
				fmt.Println("Пользователь заблокирован.")
				WriteResponse(&w, &map[string]interface{}{
					"message":     "Пользователь заблокирован.",
					"login_token": login_token,
				}, 403)
				return
			}

			// Записываем в БД (Да, пользователь уже есть, но обновляем логин токен (остальные токены создаём) и способ входа)
			// Все основные данные остаются прежними (титульная информаця, пароль и другое)
			(*userData)["status"] = "WaitingAuthorizingConfirm"
			(*userData)["login_token"] = login_token
			_, err = InsertUserToDatabase(userData)
			if err != nil {
				WriteResponse(&w, &map[string]interface{}{
					"message":     "Ошибка авторизации, ну типа.",
					"login_token": login_token,
				}, 500)
				return
			}
			// Возвращает успех, если всё хорошо
			WriteResponse(&w, &map[string]interface{}{
				"message":     "Авторизация успешна.",
				"login_token": login_token,
			}, 200)
		}
	}
}

// VerifyAuthorizationHandler - создаёт и отправляет код пользователю на аккаунт
func VerifyAuthorizationHandler(w http.ResponseWriter, r *http.Request) {
	fmt.Println("Обработка авторизации через код.")
	requestData, err := GetDataFromBody(&r.Body)
	if err != nil {
		WriteResponse(&w, &map[string]interface{}{
			"message": "Чота не получилось прочитать тело запроса.",
		}, 400)
		return
	}
	// Добываем state
	state, ok := (*requestData)["state"].(string)
	if !ok {
		WriteResponse(&w, &map[string]interface{}{
			"message": "Чота не получилось прочитать состояние запроса",
		}, 400)
		return
	}

	// Добываем токен из заголовков
	login_token, err := GetTokenFromHeader(r.Header.Get("Authorization"))
	if err != nil || login_token == "" {
		WriteResponse(&w, &map[string]interface{}{
			"message": "Это типа значит, что нет заголовка авторизации или токен некорректно передан.",
		}, 400)
		return
	}

	// Получаем авторизацию
	auth_data, err := GetFromDatabaseByValue("Authorizations", "state", state)
	if err != nil {
		WriteResponse(&w, &map[string]interface{}{
			"message": "Запись авторизации не найдена.",
		}, 401)
		return
	}
	// Добываем код
	code, ok := (*requestData)["code"].(string)
	if !ok {
		WriteResponse(&w, &map[string]interface{}{
			"message": "Код в запросе отсутствует.",
		}, 400)
		return
	}
	// Сверяем код
	if code != (*auth_data)["code"].(string) {
		WriteResponse(&w, &map[string]interface{}{
			"message": "Неверный код.",
		}, 409)
		return
	}
	if IsExpired((*auth_data)["exp_time"].(int64)) {
		DeleteFromDatabase("Registrations", "state", state)
		WriteResponse(&w, &map[string]interface{}{
			"message": "Код больше не действителен.",
		}, 401)
		return
	}

	// Удаляет запись авторизации
	DeleteFromDatabase("Authotizations", "state", state)
	// Записывает авторизированного пользователя с персональными токенами
	_, err = InsertUserToDatabase(&primitive.M{
		"status":      "WaitingAuthorizingConfirm",
		"login_token": login_token,
		"email":       (*auth_data)["email"].(string),
	})
	if err != nil {
		WriteResponse(&w, &map[string]interface{}{
			"message": "Ошибка авторизации.",
		}, 500)
		return
	}

	// Ответ по цепочке дойдёт до страницы, с которой будет запрос на автовход
	WriteResponse(&w, &map[string]interface{}{
		"message": "Авторизация успешна.",
	}, 200)
	return
}

// Для создания кода и подготовки подтверждения кодом (пока только авторизация, но в теории можно перенести сюда создание кода для регистрации)
func InitRegistrationHandler(w http.ResponseWriter, r *http.Request) {
	fmt.Println("Создание кода для регитрации.")
	requestData, err := GetDataFromBody(&r.Body)
	if err != nil {
		WriteResponse(&w, &map[string]interface{}{
			"message": "Чота не получилось прочитать тело запроса.",
		}, 400)
		return
	}

	log.Println("Приступаем к проверке данных на доступ к регистрации.")
	// Проверяет имена
	// Ник
	nickname := (*requestData)["nickname"].(string)
	message, err := CheckName(nickname, false)
	if err != nil {
		log.Println(message)
		WriteResponse(&w, &map[string]interface{}{
			"message": message,
		}, 400)
		return
	}
	// Фамилия
	last_name := (*requestData)["last_name"].(string)
	message, err = CheckName(last_name, true)
	if err != nil {
		log.Println(message)
		WriteResponse(&w, &map[string]interface{}{
			"message": message,
		}, 400)
		return
	}
	// Имя
	first_name := (*requestData)["first_name"].(string)
	message, err = CheckName(first_name, true)
	if err != nil {
		log.Println(message)
		WriteResponse(&w, &map[string]interface{}{
			"message": message,
		}, 400)
		return
	}
	// Отчество
	patronimyc := (*requestData)["patronimyc"].(string)
	message, err = CheckName(patronimyc, true)
	if err != nil {
		log.Println(message)
		WriteResponse(&w, &map[string]interface{}{
			"message": message,
		}, 400)
		return
	}
	// Проверяет пароль
	password := (*requestData)["password"].(string)
	message, err = CheckPasswordAndRepeat(password, (*requestData)["repeat_password"].(string))
	if err != nil {
		log.Println(message)
		WriteResponse(&w, &map[string]interface{}{
			"message": message,
		}, 400)
		return
	}

	// Получает пользователя по почте
	email := (*requestData)["email"].(string)
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
	code := GenerateCode(6)
	// Пытается отправить код по емэил
	err = SendConfirmationEmail(email, code)
	if err != nil {
		log.Println("Ошибка отправки email.")
		WriteResponse(&w, &map[string]interface{}{
			"message": "Ошибка при отправке подтверждения. Возможно, почта введена некорректно.",
		}, 400)
		return
	}
	log.Println("Email отправлен.")
	// Генерирует state регистрации
	registration_state := GenerateState()
	log.Println("Сгенерирован state регистрации ", registration_state)
	// Сохраняем в БД, как юзера, для дальнейшей пляски (при подтверждении)
	_, err = InsertRegistrationNoteToDatabase(&primitive.M{
		"status":     "Registring",
		"state":      registration_state,
		"code":       code,
		"email":      email,
		"nickname":   nickname,
		"first_name": first_name,
		"last_name":  last_name,
		"patronimyc": patronimyc,
		"password":   password,
		"exp_time":   GetTimeFromNow(60 * 5), // 5 минут доступно
	})
	// Если ошибка
	if err != nil {
		WriteResponse(&w, &map[string]interface{}{
			"message": "Ошибка при... Я не придумал.",
		}, 500)
		return
	}

	log.Println("State регистрации отправляется обратно. Ожидается подтверждение.")
	// Отправляет state обратно, как доказательство успеха
	WriteResponse(&w, &map[string]interface{}{
		"message": fmt.Sprintf("Регистрация доступна. Введите код, который был выслан на почтовый ящик %s.", email),
		"state":   registration_state,
	}, 200)
}

func InitAuthorizationHandler(w http.ResponseWriter, r *http.Request) {
	fmt.Println("Создание кода для авторизации.")
	requestData, err := GetDataFromBody(&r.Body)
	if err != nil {
		WriteResponse(&w, &map[string]interface{}{
			"message": "Чота не получилось прочитать тело запроса.",
		}, 400)
		return
	}

	// Получаем почту
	email := (*requestData)["email"].(string)
	// Получаем пользователя
	userData, err := GetFromDatabaseByValue("Users", "email", email)
	if err != nil {
		WriteResponse(&w, &map[string]interface{}{
			"message": "Пользователь не найден.",
		}, 404)
	}

	// Запрос на модуль тестировани для получения статуса пользователя
	user_server_status, err, message := GetUserServerStatus((*userData)["id"].(string))
	if err != nil {
		fmt.Println(message)
		WriteResponse(&w, &map[string]interface{}{
			"message": message,
		}, 500)
		return
	}
	// Проверяем статус
	if user_server_status == "Blocked" {
		fmt.Println("Пользователь заблокирован.")
		WriteResponse(&w, &map[string]interface{}{
			"message": "Пользователь заблокирован.",
		}, 403)
		return
	}

	// Генерирует код
	code := GenerateCode(6)
	// Генерируем уникальный state
	authorization_state := GenerateState()

	// Отправляет код Основному модулю
	text := "Получен запрос на вход " + time.Now().Format("15:04 02.01.2006") + " .Код подтверждения: " + code
	recipient := (*userData)["id"].(string)
	news := map[string]interface{}{
		"recipient": recipient,
		"text":      text,
	}
	newsResponse, err := DoPostRequestToService(os.Getenv("TEST_MODULE_URL")+"/news/send",
		"Bearer", GenerateNewAdminToken(), &news)
	defer newsResponse.Body.Close()
	if err != nil || newsResponse.StatusCode != 200 {
		fmt.Println("Не удалось отправить код. Запрос -", newsResponse)
		WriteResponse(&w, &map[string]interface{}{
			"message": "Не удалось отправить код.",
		}, 500)
		return
	}

	// Сохраняем в БД, для дальнейшей пляски
	_, err = InsertAuthorizationNoteToDatabase(&primitive.M{
		"status":     "AuthorizingWithCode",
		"login_type": "code",
		"code":       code,
		"state":      authorization_state,
		"email":      email,
		"exp_time":   GetTimeFromNow(60 * 5), // 5 минут на вход
	})

	log.Println("State авторизации отправляется обратно. Ожидается подтверждение.")
	// Отправляет state обратно, как доказательство успеха
	WriteResponse(&w, &map[string]interface{}{
		"message": "Код отправлен на аккаунт, к которому привязана почта " + email,
		"state":   authorization_state,
	}, 200)
}

// Перенаправляет пользователя на страницу авторизации сервиса
func ServiceAuthHandler(w http.ResponseWriter, r *http.Request) {
	fmt.Println("Обработка авторизации через сервис.")
	requestData, err := GetDataFromBody(&r.Body)
	if err != nil {
		WriteResponse(&w, &map[string]interface{}{
			"message": "Чота не получилось прочитать тело запроса.",
		}, 400)
	}

	// Проверяет тип сервиса авторизации
	if (*requestData)["service_type"] != "github" && (*requestData)["service_type"] != "yandex" {
		WriteResponse(&w, &map[string]interface{}{
			"message": "А что с вопросом-то?",
		}, 400)
		return
	}
	// Добываем логин токен из заголовков
	login_token, err := GetTokenFromHeader(r.Header.Get("Authorization"))
	if err != nil || login_token == "" {
		WriteResponse(&w, &map[string]interface{}{
			"message": "Это типа значит, что нет заголовка авторизации или токен некорректно передан.",
		}, 400)
		return
	}

	// Коллбэк авторизации для возвращения на клиент
	// Генерируем уникальный service_state
	service_state := GenerateState()
	log.Println("Сгенерирован service_state", service_state)
	// Сохраняем в БД, как юзера, для дальнейшей пляски (при коллбэке)
	_, err = InsertAuthorizationNoteToDatabase(&primitive.M{
		"status":                    "AuthorizingWithService",
		"auth_service_state":        service_state,
		"login_token":               login_token,
		"session_token":             (*requestData)["session_token"],
		"login_type":                (*requestData)["service_type"],
		"auth_service_callback_url": (*requestData)["callback_url"],
	})
	log.Println("Передан callback_url", (*requestData)["callback_url"])

	// Если ошибка
	if err != nil {
		WriteResponse(&w, &map[string]interface{}{
			"message": "Это типа значит, что ну кароче, жесть.",
		}, 500)
		log.Println(err)
		return
	}
	// Создаём конфиг
	oAuthConfig, err := CreateOAuthConfig((*requestData)["service_type"].(string))
	if err != nil { // Не создался конфиг, но такого не будет
		WriteResponse(&w, &map[string]interface{}{
			"message": "Это типа значит, что ну кароче, жесть тоже да.",
		}, 500)
		log.Println(err)
		return
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
	auth_data, err := GetFromDatabaseByValue("Authorizations", "auth_service_state", getted_state)
	callback_url := (*auth_data)["auth_service_callback_url"].(string) // Заранее берём коллбэк
	log.Println("Коллбэк для возврата токена сессии:", callback_url)
	// Проверка на успех получения
	if err != nil {
		// Некуда перенаправить, если всё так плохо
		http.Error(w, "State не совпадает, авторизация отменена", http.StatusBadRequest)
		return
	}

	// Получает тип входа и проверяет (но вряд ли будет ошибка)
	login_type := (*auth_data)["login_type"].(string) // r.URL.Query().Get("type")
	log.Println("Логин type:", login_type)
	if login_type != "github" && login_type != "yandex" {
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
	config, err := CreateOAuthConfig(login_type)
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
	url_values, err := CreateUrlValues(login_type)
	url_values.Set("code", code)
	if login_type == "yandex" {
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
	if login_type == "yandex" {
		// Разбираем ответ (Яндекс возвращает JSON)
		var parsedData map[string]interface{}
		err := json.Unmarshal(codeExchangeResponseBody, &parsedData)
		if err != nil { // Удаляет запись входа
			DeleteFromDatabase("Authorizations", "auth_service_state", getted_state)
			http.Error(w, "Ошибка при получении токена.", http.StatusInternalServerError)
			return
		}
		// Записываем токены
		serviceTokenData["access_token"] = parsedData["access_token"].(string)
		serviceTokenData[" refresh_token"] = parsedData["refresh_token"].(string)
		log.Println("От Яндекса получены токены: access_token -", serviceTokenData["access_token"], "И  refresh_token -", serviceTokenData[" refresh_token"])
	} else if login_type == "github" {
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
		serviceTokenData["access_token"] = params.Get("access_token")
		serviceTokenData["refresh_token"] = ""
		log.Println("От Github получен токен: access_token -", serviceTokenData["access_token"])
	}
	// Проверка access_token
	if serviceTokenData["access_token"] == "" {
		DeleteFromDatabase("Authorizations", "auth_service_state", getted_state)
		http.Error(w, "От Сервиса не получен токен доступа", http.StatusInternalServerError)
		return
	}

	// Получает данные с сайта с помощью клиента через токен доступа
	gettedUserData, err := GetUserDataFromService(serviceTokenData["access_token"], login_type)
	if err != nil {
		DeleteFromDatabase("Authorizations", "auth_service_state", getted_state)
		http.Error(w, "Ошибка при чтении данных с клиента", http.StatusInternalServerError)
		return
	}
	// Сортирует полученные данные
	sortedUserData := SortServiceUserData(gettedUserData, login_type)
	log.Println("Имя пользователя: ", (*sortedUserData)["nickname"])
	log.Println("Емэил пользователя: ", (*sortedUserData)["email"])
	// Удаляет запись об авторизации
	DeleteFromDatabase("Authorizations", "id", (*auth_data)["id"])

	// Если пользователь зарегистрирован, проверяет на наличие бана
	currentUserData, err := GetFromDatabaseByValue("Users", "email", (*sortedUserData)["email"].(string))
	if err == nil {
		// Запрос на модуль тестировани для получения статуса пользователя
		user_server_status, err, message := GetUserServerStatus((*currentUserData)["id"].(string))
		if err != nil {
			http.Error(w, message, http.StatusInternalServerError)
			http.Redirect(w, r, callback_url, http.StatusFound)
			fmt.Println(message)
			return
		}
		// Проверяем статус
		if user_server_status == "Blocked" {
			http.Error(w, "Пользователь заблокирован.", 403)
			http.Redirect(w, r, callback_url, http.StatusFound)
			return
		}
	}

	// Записывает авторизированного пользователя с персональными токенами
	_, err = InsertUserToDatabase(&primitive.M{
		"status":      "WaitingAuthorizingConfirm",
		"id":          (*auth_data)["id"].(string),
		"login_token": (*auth_data)["login_token"].(string),
		"nickname":    (*sortedUserData)["nickname"],
		"email":       (*sortedUserData)["email"],
	})
	// Есть важный недостаток: при авторизации через сервис, который не выдёт почту (GitHub), каждая авторизация создаст свою запись в БД
	// P.S. Уже в прошлом, я научился получать почту с гх и без неё не будет входа - только ошибка
	if err != nil {
		log.Printf("Ошибка при создании пользователя: %v", err)
		http.Error(w, "Ошибка при создании пользователя", http.StatusInternalServerError)
	}

	/*log.Println("Сохранение кук сессии ", (*auth_data)["session_token"].(string))
	// Ставим куки
	http.SetCookie(w, &http.Cookie{
		Name:     "session_token",
		Value:    (*auth_data)["session_token"].(string),
		Path:     "/",
		HttpOnly: true,
		// Secure: true,
		Domain: "localhost",
	})*/

	result_callback_url := callback_url + "?session_token=" + (*auth_data)["session_token"].(string)
	log.Println("Перенаправление на ", result_callback_url)
	http.Redirect(w, r, result_callback_url, http.StatusFound)
}

// Регистрация нового пользователя
func VerifyRegistrationHandler(w http.ResponseWriter, r *http.Request) {
	// Получает данные из тела запроса
	requestData, err := GetDataFromBody(&r.Body)
	if err != nil {
		WriteResponse(&w, &map[string]interface{}{
			"message": "Чота не получилось прочитать тело запроса.",
		}, 400)
	}
	log.Println("Обработка запроса на регистрацию с параметром", (*requestData)["stage"])

	log.Println("Начинаем проверку кода подтверждения.")
	// Добываем state
	state, ok := (*requestData)["state"].(string)
	if !ok {
		WriteResponse(&w, &map[string]interface{}{
			"message": "Чота не получилось прочитать состояние запроса",
		}, 400)
		return
	}
	log.Println("Подтверждаем регистрацию для state", state)

	// Получаем запись
	reg_data, err := GetFromDatabaseByValue("Registrations", "state", state)
	if err != nil {
		WriteResponse(&w, &map[string]interface{}{
			"message": "Запись регистрации не найдена.",
		}, 401)
		return
	}

	// Проверка времени регистрации
	if IsExpired((*reg_data)["exp_time"].(int64)) {
		// Удаляем запись и возвращаем ответ
		log.Println("Время регистрации истекло.")
		DeleteFromDatabase("Registrations", "state", state)
		WriteResponse(&w, &map[string]interface{}{
			"message": "Время регистрации истекло.",
		}, 401)
		return
	}
	// Добываем код
	code, ok := (*requestData)["code"].(string)
	if !ok {
		WriteResponse(&w, &map[string]interface{}{
			"message": "Код в запросе отсутствует.",
		}, 400)
		return
	}
	// Сверяем код
	if code != (*reg_data)["code"].(string) {
		WriteResponse(&w, &map[string]interface{}{
			"message": "Неверный код.",
		}, 409)
		return
	}
	log.Println("Коды совпадают.")

	// Регистрирует пользователя в БД
	DeleteFromDatabase("Registrations", "state", state)
	_, err = InsertUserToDatabase(&primitive.M{
		"status":     "LogoutedGlobal",
		"nickname":   (*reg_data)["nickname"].(string),
		"last_name":  (*reg_data)["last_name"].(string),
		"first_name": (*reg_data)["first_name"].(string),
		"patronimyc": (*reg_data)["patronimyc"].(string),
		"email":      (*reg_data)["email"].(string),
		"password":   (*reg_data)["password"].(string),
	})
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

// Проверяет возможность авто авторизации по login_token
func CheckAuthHandler(w http.ResponseWriter, r *http.Request) {
	log.Println("Запрос на проверку возможности входа пользователя.")
	// Добываем логин токен из заголовков
	login_token, err := GetTokenFromHeader(r.Header.Get("Authorization"))
	if err != nil || login_token == "" {
		WriteResponse(&w, &map[string]interface{}{
			"message": "Это типа значит, что нет заголовка авторизации или токен некорректно передан.",
		}, 400)
		return
	}

	// Ищем пользователя по login_token
	userData, err := GetFromDatabaseByValue("Users", "login_token", login_token)
	// Проверяем на успех поиска. (Можно укоротить, но не стал, т.к. мб сделаю проверку на истечение логинтокена)
	if err != nil {
		log.Println("Запрос отклонён.")
		// Пользователь не найден или ошибка
		WriteResponse(&w, &map[string]interface{}{
			"message": "Пользователь не найден.",
		}, 404)
		return
	} else {
		log.Println("Запрос одобрен.")
		// Пользователь найден, не ошибка
		WriteResponse(&w, &map[string]interface{}{
			"message":  "Пользователь найден.",
			"nickname": (*userData)["nickname"],
		}, 200)
	}
}

//	Подтверждает авторизацию по login_token.
//
// (По сути, копия CheckAuth, но с отличиями - удаляет токен с БД и отправляет данные)
func ConfirmAuthHandler(w http.ResponseWriter, r *http.Request) {
	log.Println("Запрос на подтверждение входа и выдачу параметров.")
	// Добываем логин токен из заголовков
	login_token, err := GetTokenFromHeader(r.Header.Get("Authorization"))
	if err != nil || login_token == "" {
		WriteResponse(&w, &map[string]interface{}{
			"message": "Это типа значит, что нет заголовка авторизации или токен некорректно передан.",
		}, 400)
		return
	}

	// Ищем пользователя по login_token
	userData, err := GetFromDatabaseByValue("Users", "login_token", login_token)
	// Проверяем на успех поиска. (Можно укоротить, но не стал, т.к. мб сделаю проверку на истечение логинтокена)
	// + Если на конфирм перешли, то, скорее всего, запись есть, но, всё же, не помешает
	if err != nil {
		log.Println("Запрос отклонён.")
		// Пользователь не найден или ошибка
		WriteResponse(&w, &map[string]interface{}{
			"message": "Пользователь не найден.",
		}, 404)
		return
	} else {
		// Указывает, что пользователь не сможет подтвердить вход без авторизации
		(*userData)["login_token"] = ""

		// Получить последний добавленный токен обновления
		var refresh_token string = ""
		if refresh_tokens_slice, ok := (*userData)["refresh_tokens"].(primitive.A); ok && len(refresh_tokens_slice) > 0 {
			refresh_token = refresh_tokens_slice[len(refresh_tokens_slice)-1].(string)
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
				"message": "Пользователь не обновлён.",
			}, 500)
			return
		}

		// Если не удалось получить токен ранее, габелла
		if refresh_token == "" {
			log.Println("Запрос на подтверждение входа отклонён из-за отсутствия токена обновления.")
			// Пользователь не обновлён
			WriteResponse(&w, &map[string]interface{}{
				"message": "Токен обновления утерян или не был присвоен.",
			}, 401)
			return
		}

		log.Println("Возвращение подтверждения.")
		// И вернуть
		log.Println("Вход одобрен для имени", (*userData)["nickname"].(string))
		WriteResponse(&w, &map[string]interface{}{
			"message": "Вход одобрен.",

			"id":            (*userData)["id"].(string),
			"email":         (*userData)["email"].(string),
			"nickname":      (*userData)["nickname"].(string),
			"roles":         (*userData)["roles"].(primitive.A),
			"access_token":  (*userData)["access_token"].(string),
			"refresh_token": refresh_token,
		}, 200)
	}
}

// Обработчик для получения данных при наличии токена доступа
func GetSubHandler(w http.ResponseWriter, r *http.Request) {
	log.Println("Получен запрос на обновление данных.")
	// Добываем токен из заголовков
	access_token, err := GetTokenFromHeader(r.Header.Get("Authorization"))
	if err != nil || access_token == "" {
		WriteResponse(&w, &map[string]interface{}{
			"message": "Отсутствует токен.",
		}, 400)
		return
	}

	userData, err := GetFromDatabaseByValue("Users", "access_token", access_token)
	if err != nil {
		// Обработка отсутствия пользователя
		log.Println("Запрос на обновление данных провален. Пользователь не обнаружен.")
		WriteResponse(&w, &map[string]interface{}{
			"message": "Пользователь не найден.",
		}, 404)
		return
	}

	if IsOwnTokenExpired(access_token) {
		log.Println("Запрос на обновление данных провален на этапе проверки актальности токена.")
		WriteResponse(&w, &map[string]interface{}{
			"message": "Токен недействителен.",
		}, 401)
		return
	}

	log.Println("Запрос на обновление данных успешно завершён.")
	// Отправить результат обновления
	WriteResponse(&w, &map[string]interface{}{
		"id":       (*userData)["id"].(string),
		"nickname": (*userData)["nickname"].(string),
		"email":    (*userData)["email"].(string),
		"roles":    (*userData)["roles"].(primitive.A),
	}, 200)
}

// обработчик для обновления токенов
func RefreshAccessHandler(w http.ResponseWriter, r *http.Request) {
	log.Println("Получен запрос на обновление токенов.")
	// Добываем рефреш токен из заголовков
	refresh_token, err := GetTokenFromHeader(r.Header.Get("Authorization"))
	if err != nil || refresh_token == "" {
		WriteResponse(&w, &map[string]interface{}{
			"message": "Токен не передан.",
		}, 400)
		return
	}
	userData, err := GetFromDatabaseByValue("Users", "refresh_tokens", refresh_token)
	if err != nil {
		// Обработка отсутствия пользователя
		log.Println("Запрос на обновление токенов провален. Пользователь не обнаружен.")
		WriteResponse(&w, &map[string]interface{}{
			"message": "Пользователь не найден.",
		}, 404)
		return
	}
	if IsOwnTokenExpired(refresh_token) {
		log.Println("Запрос на обновление токенов провален на этапе проверки актальности токена.")
		WriteResponse(&w, &map[string]interface{}{
			"message": "Токен недействителен.",
		}, 401)
		return
	}

	// Запрос на модуль тестировани для получения статуса пользователя
	user_server_status, err, message := GetUserServerStatus((*userData)["id"].(string))
	if err != nil {
		log.Println("Не получилось проверить статус.")
		WriteResponse(&w, &map[string]interface{}{
			"message": message,
		}, 500)
		return
	}
	// Проверяем статус
	if user_server_status == "Blocked" {
		log.Println("Пользователь заблокирован.")
		WriteResponse(&w, &map[string]interface{}{
			"message": "Пользователь заблокирован.",
		}, 403)
		return
	}

	// Создаёт токены
	roles := (*userData)["roles"].(primitive.A)
	new_access_token := GenerateNewAccessToken((*userData)["id"].(string), roles, GenerateBasePermissionsListForRoles(roles))
	new_refresh_token := GenerateNewRefreshToken((*userData)["id"].(string))

	// Заменяет новым токеном обновления старый
	refresh_tokens_slice := (*userData)["refresh_tokens"].(primitive.A)
	for i := 0; i < len(refresh_tokens_slice); i++ {
		if refresh_tokens_slice[i] == refresh_token {
			refresh_tokens_slice[i] = new_refresh_token
			(*userData)["refresh_tokens"] = refresh_tokens_slice
			break
		}
	}
	// Обновляем токен доступа
	(*userData)["access_token"] = new_access_token
	// Перезаписывает
	err = ReplaceFullUserInDatabase((*userData)["id"].(string), userData)
	if err != nil {
		fmt.Println("Пользователь найден, но обновление не осуществлёно.")
		WriteResponse(&w, &map[string]interface{}{
			"message": "Ошибка обновления.",
		}, 500)
	}

	log.Println("Запрос на обновление токенов успешно завершён.")
	// Отправить результат обновления
	WriteResponse(&w, &map[string]interface{}{
		"access_token":  new_access_token,
		"refresh_token": new_refresh_token,
	}, 200)
}

// Обработчик выхода со всех устройств
func LogoutAllHandler(w http.ResponseWriter, r *http.Request) {
	// Добываем токен из заголовков
	refresh_token, err := GetTokenFromHeader(r.Header.Get("Authorization"))
	if err != nil || refresh_token == "" {
		WriteResponse(&w, &map[string]interface{}{
			"message": "Токен не передан.",
		}, 400)
		return
	}

	// Ищем пользователя по token
	userData, err := GetFromDatabaseByValue("Users", "refresh_tokens", refresh_token)
	if err != nil {
		fmt.Println("Пользователь не найден, выход не осуществлён.")
		WriteResponse(&w, &map[string]interface{}{
			"message": "Пользователь не найден.",
		}, 404)
		return
	}
	(*userData)["access_token"] = ""
	(*userData)["refresh_tokens"] = []interface{}{}
	(*userData)["status"] = "LogoutedGlobal"
	err = ReplaceFullUserInDatabase((*userData)["id"].(string), userData)
	if err != nil {
		fmt.Println("Пользователь найден, но выход не осуществлён.")
		WriteResponse(&w, &map[string]interface{}{
			"message": "Ошибка выхода.",
		}, 500)
	}
	// Успех
	WriteResponse(&w, &map[string]interface{}{
		"message": "Успешный выход.",
	}, 200)
}

// Кикает пользователя
func LogoutUserHandler(w http.ResponseWriter, r *http.Request) {
	fmt.Println("Запрос на кик пользователя.")
	// Добываем токен из заголовков
	access_token, err := GetTokenFromHeader(r.Header.Get("Authorization"))
	if err != nil || access_token == "" {
		WriteResponse(&w, &map[string]interface{}{
			"message": "Токен не передан.",
		}, 400)
		return
	}
	// Получаем данные тела
	requestData, err := GetDataFromBody(&r.Body)
	if err != nil {
		WriteResponse(&w, &map[string]interface{}{
			"message": "Чота не получилось прочитать тело запроса.",
		}, 400)
		return
	}
	// Ищем пользователя по id
	userData, err := GetFromDatabaseByValue("Users", "id", (*requestData)["id"].(string))
	if err != nil {
		fmt.Println("Пользователь не найден, выход не осуществлён.")
		WriteResponse(&w, &map[string]interface{}{
			"message": "Пользователь не найден.",
		}, 404)
		return
	}
	// Парсит токен
	claims, err := ParseToken(access_token)
	if err != nil {
		fmt.Println("Токен не действителен.")
		WriteResponse(&w, &map[string]interface{}{
			"message": "Токен не действителен.",
		}, 401)
		return
	}
	// Проверяет возможность кика пользователя у токена (либо сам себя, либо нужно разрешение на блокировку)
	if (*requestData)["user_id"].(string) != (*claims)["sub"].(string) {
		permissionsSlice := (*claims)["permissions"].(primitive.A)
		has_permission := false
		for i := 0; i < len(permissionsSlice); i++ {
			if permissionsSlice[i].(interface{}) == "user:block" {
				has_permission = true
				break
			}
		}
		if !has_permission {
			fmt.Println("Недостаточно прав для кика.")
			WriteResponse(&w, &map[string]interface{}{
				"message": "Недостаточно прав для кика пользователя.",
			}, 403)
			return

		}
	}

	(*userData)["access_token"] = ""
	(*userData)["refresh_tokens"] = []interface{}{}
	(*userData)["status"] = "LogoutedGlobal"
	err = ReplaceFullUserInDatabase((*userData)["id"].(string), userData)
	if err != nil {
		fmt.Println("Пользователь найден, но выход не осуществлён.")
		WriteResponse(&w, &map[string]interface{}{
			"message": "Ошибка выхода.",
		}, 500)
	}
	// Успех
	fmt.Println("Запрос на кик пользователя одобрен. Пользователя выкинуло на...")
	WriteResponse(&w, &map[string]interface{}{
		"message": "Пользователь разлогинен.",
	}, 200)
}

// Обработчик изменения параметров
func UserDataChangeHandler(w http.ResponseWriter, r *http.Request) {
	log.Println("Получен запрос на изменение данных.")
	// Добываем токен из заголовка
	access_token, err := GetTokenFromHeader(r.Header.Get("Authorization"))
	if err != nil || access_token == "" {
		http.Error(w, "Отсутствует access_token", http.StatusUnauthorized)
		return
	}
	// Парсит токен и проверяет актуальность
	claims, err := ParseToken(access_token)
	if err != nil || IsOwnTokenClaimsExpired(claims) {
		fmt.Println("Токен не действителен.")
		WriteResponse(&w, &map[string]interface{}{
			"message": "Токен не действителен.",
		}, 401)
		return
	}
	// Получаем новые данные
	requestData, err := GetDataFromBody(&r.Body)
	if err != nil {
		WriteResponse(&w, &map[string]interface{}{
			"message": "Чота не получилось прочитать тело запроса.",
		}, 400)
		return
	}
	// Получаем айди пользователя, которому меняем данные (если нету, меняет себе)
	token_user_id := (*claims)["sub"].(string)
	var target_user_id string
	if request_id, ok := (*requestData)["user_id"]; ok {
		target_user_id = request_id.(string)
	} else {
		target_user_id = token_user_id
	}
	// Получаем запись пользователя по айди
	userData, err := GetFromDatabaseByValue("Users", "id", target_user_id)
	if err != nil {
		// Обработка отсутствия пользователя
		log.Println("Запрос на обновление данных провален. Пользователь не обнаружен.")
		WriteResponse(&w, &map[string]interface{}{
			"message": "Пользователь не найден.",
		}, 404)
		return
	}
	// Меняем данные в записи
	// Имя
	if first_name, ok := (*requestData)["first_name"]; ok {
		// Проверка доступности действия по правам
		if target_user_id != token_user_id {
			log.Println("Нельзя менять данные об имени другого пользователя.")
			WriteResponse(&w, &map[string]interface{}{
				"message": "Нельзя менять данные об имени другого пользователя.",
			}, 403)
			return
		}
		// Проверка доступности действия по настройкам
		message, err := CheckName(first_name.(string), true)
		if err != nil {
			log.Println(message)
			WriteResponse(&w, &map[string]interface{}{
				"message": message,
			}, 406)
			return
		}
		(*userData)["first_name"] = first_name
	}
	// Фамилия
	if last_name, ok := (*requestData)["last_name"]; ok {
		// Проверка доступности действия по правам
		if target_user_id != token_user_id {
			log.Println("Нельзя менять данные об имени другого пользователя.")
			WriteResponse(&w, &map[string]interface{}{
				"message": "Нельзя менять данные об имени другого пользователя.",
			}, 403)
			return
		}
		// Проверка доступности действия по настройкам
		message, err := CheckName(last_name.(string), true)
		if err != nil {
			log.Println(message)
			WriteResponse(&w, &map[string]interface{}{
				"message": message,
			}, 406)
			return
		}
		(*userData)["last_name"] = last_name
	}
	// Отчество
	if patronimyc, ok := (*requestData)["patronimyc"]; ok {
		// Проверка доступности действия по правам
		if target_user_id != token_user_id {
			log.Println("Нельзя менять данные об имени другого пользователя.")
			WriteResponse(&w, &map[string]interface{}{
				"message": "Нельзя менять данные об имени другого пользователя.",
			}, 403)
			return
		}
		// Проверка доступности действия по настройкам
		message, err := CheckName(patronimyc.(string), true)
		if err != nil {
			log.Println(message)
			WriteResponse(&w, &map[string]interface{}{
				"message": message,
			}, 406)
			return
		}
		(*userData)["patronimyc"] = patronimyc
	}
	// Пароль
	if password, ok := (*requestData)["password"]; ok {
		// Проверка доступности действия по правам
		if target_user_id != token_user_id {
			log.Println("Нельзя менять данные о пароле другого пользователя.")
			WriteResponse(&w, &map[string]interface{}{
				"message": "Нельзя менять данные о пароле другого пользователя.",
			}, 403)
			return
		}
		// Проверка доступности действия по настройкам
		message, err := CheckPassword(password.(string))
		if err != nil {
			log.Println(message)
			WriteResponse(&w, &map[string]interface{}{
				"message": message,
			}, 406)
			return
		}
		(*userData)["password"] = password
	}
	// Роли
	if roles, ok := (*requestData)["roles"]; ok {
		// Проверка доступности действия по правам
		permissionsSlice := (*claims)["permissions"].([]interface{})
		has_roles_write_permission := false
		for i := 0; i < len(permissionsSlice); i++ {
			if permissionsSlice[i] == "user:roles:write" {
				has_roles_write_permission = true
				break
			}
		}
		if !has_roles_write_permission {
			log.Println("Нет права на изменение ролей.")
			WriteResponse(&w, &map[string]interface{}{
				"message": "Нет права на изменение ролей.",
			}, 403)
			return
		}
		// Проверка доступности действия по настройкам
		var err error
		rolesSlice := (*requestData)["roles"].([]interface{})
		for i := 0; i < len(rolesSlice); i++ {
			if rolesSlice[i] != "Student" && rolesSlice[i] != "Teacher" && rolesSlice[i] != "Admin" {
				err = fmt.Errorf("Одна или более ролей не существует(ют)")
				break
			}
		}
		if err != nil {
			log.Println("Одна или более ролей не существует(ют)")
			WriteResponse(&w, &map[string]interface{}{
				"message": "Одна или более ролей не существует(ют)",
			}, 406)
			return
		}
		// Ставит роли и удаляет токен доступа, чтобы потребовать создать новый для обновления его прав
		(*userData)["roles"] = roles
		(*userData)["access_token"] = "Deleted after roles updated."
	}

	// Обновляем запись
	err = ReplaceFullUserInDatabase((*userData)["id"].(string), userData) // Заменит старые данные
	if err != nil {
		log.Println("Запрос на изменение данных отклонён из-за ошибки обновления обновления.")
		// Пользователь не обновлён
		WriteResponse(&w, &map[string]interface{}{
			"message": "Пользователь не обновлён.",
		}, 500)
		return
	}
	// Успех
	WriteResponse(&w, &map[string]interface{}{
		"message": "Данные обновлены.",
	}, 200)
}

// Получает статус пользователя на сервере (Оффлай, онлайн, заблокирован)
func GetUserServerStatus(user string) (string, error, string) {
	statusResponse, err := DoGetRequestToService(os.Getenv("TEST_MODULE_URL")+"/users/data?user_id="+user+"&filter=status",
		"Bearer", GenerateNewAdminToken())
	defer statusResponse.Body.Close()
	if err != nil {
		return "", err, "Не удалось подключиться к серверу"
	}
	// Получаем данные из запроса
	data, err := GetDataFromDefaultResponseBody(statusResponse.Body)
	if err != nil {
		return "", err, "Не удалось обработать данные с сервера"
	}
	// Проверяем статус ответа
	if statusResponse.StatusCode != 200 {
		return "", fmt.Errorf("Ответ не получен."), (*data)["message"].(string)
	}
	// Отправляем полученный статус
	return (*data)["status"].(string), nil, "Успешное получение статуса"
}

// Система для получения данных с посторонних серверов
// Отправляем Get запрос с токеном на указанный url
func DoGetRequestToService(url string, token_prefix string, access_token string) (*http.Response, error) {
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
	req.Header.Set("Authorization", token_prefix+" "+access_token)
	// Устанавливаем Content-Type
	req.Header.Set("Content-Type", "application/json; charset=utf-8")
	req.Header.Set("Accept", "application/json")

	// Выполняем запрос
	resp, err := client.Do(req)
	if err != nil {
		return nil, fmt.Errorf("Ошибка выполнения запроса: %w", err)
	}

	// defer resp.Body.Close() не здесь
	return resp, nil
}

// Post
func DoPostRequestToService(url string, token_prefix string, access_token string, data *map[string]interface{}) (*http.Response, error) {
	// Создаём HTTP‑клиент с таймаутом
	client := &http.Client{
		Timeout: 10 * time.Second,
	}
	// Создаём json и тело запроса
	data_json, err := json.Marshal(*data)
	if err != nil {
		log.Printf("Ошибка сериализации JSON: %v", err)
		return nil, err
	}
	body := bytes.NewReader(data_json)
	// Формируем запрос
	req, err := http.NewRequest("POST", url, body)
	if err != nil {
		return nil, fmt.Errorf("ошибка создания запроса: %w", err)
	}

	// Добавляем заголовок Authorization
	// Для большинства сервисов: "Bearer <TOKEN>"
	// Для GitHub: "token <TOKEN>"
	req.Header.Set("Authorization", token_prefix+" "+access_token)

	// Устанавливаем Content-Type
	req.Header.Set("Content-Type", "application/json; charset=utf-8")

	// Дополнительно: Accept для JSON‑ответов
	req.Header.Set("Accept", "application/json")

	// Выполняем запрос
	log.Println("Выполняем Post запрос на", url, "с токеном", access_token)
	resp, err := client.Do(req)
	if err != nil {
		log.Println("Не получилось выполнить запрос.")
		return nil, fmt.Errorf("Ошибка выполнения запроса: %w", err)
	}

	log.Println("Запрос выполнен успешно.")
	// defer resp.Body.Close() не здесь — вызывается на стороне потребителя
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
func GetUserDataFromService(access_token string, service_type string) (*map[string]interface{}, error) {
	// Выбираем префикс для токена в заголовке
	var token_prefix string
	if service_type == "github" {
		token_prefix = "token"
	} else {
		token_prefix = "Bearer"
	}
	// Получаем нужный юрл
	url := GetServiceLoginURL(service_type)
	log.Println("Получение данных с клиента: 17%.")

	// Делаем запрос, полчаем ответ
	mainDataResponse, err := DoGetRequestToService(url, token_prefix, access_token)
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
	if service_type == "github" {
		log.Println("ПОЛУЧЕНИЕ ДАННЫХ С GITHUB!")
		log.Println("ДЕЛАЕМ ЗАПРОС НА /emails")
		// Делаем запрос, полчаем ответ
		emailsDataResponse, err := DoGetRequestToService(url+"/emails", token_prefix, access_token)
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
func SortServiceUserData(data *map[string]interface{}, service_type string) *primitive.M {
	log.Println("Начало сортировки полученной информации.")
	var result *primitive.M
	switch service_type {
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
		if name, ok := (*data)["nickname"]; ok && name != nil && len(name.(string)) > 0 {
			result_name = name.(string)
		} else {
			result_name = (*data)["login"].(string)
		}
		result = &primitive.M{
			"nickname": result_name,
			"email":    result_email,
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
			"nickname": (*data)["display_name"],
			"email":    result_email,
		}
	default:
		result = &primitive.M{}
	}

	log.Println("Информация отсортирована.")
	return result
}

// Ссылки на ресурсы для получения данных по токену
func GetServiceLoginURL(service_type string) string {
	switch service_type {
	case "github":
		return "https://api.github.com/user"
	case "yandex":
		return "https://login.yandex.ru/info?format=json"
	default:
		return ""
	}
}

// OAuth данные для запроса в виде config
func CreateOAuthConfig(service_type string) (*oauth2.Config, error) {
	fmt.Println("Создание конфига: " + service_type)
	switch service_type {
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
func CreateUrlValues(service_type string) (*url.Values, error) {
	fmt.Println("Создание конфига: " + service_type)
	switch service_type {
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

// Получает данные из тела
func GetDataFromBody(body *io.ReadCloser) (*map[string]interface{}, error) {
	fmt.Println("Получение данных из тела запроса.")
	var data map[string]interface{}
	err := json.NewDecoder(*body).Decode(&data)
	if err != nil {
		return nil, err
	}
	fmt.Println("Получены данные в количестве: " + fmt.Sprint(len(data)))
	return &data, nil
}

// Методы проверки титульной информации и пароля
func CheckName(name string, real bool) (string, error) {
	// Проверяем заполненность имени
	if name == "" {
		return "Никнейм и ФИО не можгут быть пустыми.", fmt.Errorf("Чо за имя?")
	}
	// Проверка начала и окончания имени
	if name[0] == ' ' {
		return "Никнейм и ФИО не можгут начинаться на пробел.", fmt.Errorf("Зачем...")
	}
	if name[len(name)-1] == ' ' {
		return "Никнейм и ФИО не можгут оканчиваться на пробел.", fmt.Errorf("Зачем...")
	}
	// Отсутствие запрещённых символов
	if real {
		availableSymbols :=
			"АаБбВвГгДдЕеЁёЖжЗзИиЙйКкЛлМмНнОоПпРрСсТтУуФфХхЦцЧчШшЩщЪъЫыЬьЭэЮюЯя-" // Пробел нельзя
		for _, char := range name {
			if !strings.ContainsRune(availableSymbols, char) {
				log.Println("Ошибка в ФИО: Недопустимый символ -", char)
				return "ФИО может состоять только из русских букв.", fmt.Errorf("Чо за имя?")
			}
		}
	} else {
		availableSymbols :=
			"АаБбВвГгДдЕеЁёЖжЗзИиЙйКкЛлМмНнОоПпРрСсТтУуФфХхЦцЧчШшЩщЪъЫыЬьЭэЮюЯяAaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz0123456789:^_-!?*()[]<> " // Пробел можно
		for _, char := range name {
			if !strings.ContainsRune(availableSymbols, char) {
				log.Println("Ошибка в никнейме: Недопустимый символ -", char)
				return "Никнейм может состоять только из русских или латинских букв, арабских цифр, символов :^_-!?*()[]<> и пробелов.", fmt.Errorf("Чо за имя?")
			}
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
		return fmt.Sprintf("Длина Никнейма и ФИО должна быть в пределах от %d до %d символов",
			min_duration, max_duration), err
	}
	return "Имя одобрено.", nil
}
func CheckPasswordAndRepeat(password string, repeat_password string) (string, error) {
	// Проверяем соответствие пароля требованиям
	message, err := CheckPassword(password)
	if err != nil {
		return message, err
	}
	// Совпадение пароля
	if password != repeat_password {
		return "Пароли не совпадают.", fmt.Errorf("Пароли не совпадают.")
	}
	return "Пароль одобрен.", nil
}
func CheckPassword(password string) (string, error) {
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
	return "Пароль одобрен.", nil
}

// Отпрвка писем на почту
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

// Генерация кода подтверждения
func GenerateCode(length int) string {
	multip := int(math.Pow(10.0, float64((length-1))) + 0.5)
	return fmt.Sprint(rand.New(rand.NewSource(time.Now().UnixNano())).Intn(9*multip) + multip)
}

// Генерация state
func GenerateState() string {
	return fmt.Sprintf("%x", time.Now().UnixNano())
}
