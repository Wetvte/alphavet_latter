package main

import (
	"context"
	"fmt"
	"log"
	"os"

	"go.mongodb.org/mongo-driver/bson"
	"go.mongodb.org/mongo-driver/bson/primitive"
	"go.mongodb.org/mongo-driver/mongo"
	"go.mongodb.org/mongo-driver/mongo/options"
)

// Памятка статусаов входа
/*type UserStatus int
Не получилось заюзать
const (
	Registring = iota 			   // Регистрируется сейчас
	AuthorizingWithService         // Входит через сервис
	AuthorizingWithCode            // Входит через код
	WaitingAuthorizingConfirm      // Авторизировался в БД, но не получил данные для входа
	Authorized                     // Вошёл
	LogoutedGlobal				   // Запись есть, но нигде не авторизован
)*/

var mongoClient *mongo.Client

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

// Вставляет пользователя в базу данных, или заменяет данные, если уже есть
// * ВАЖНО!!! Ипользовать ТОЛЬКО для добавления новых записей, а не с расчётом на втозамену старой записи с парочкой изменённых значений
// Если помещает запись об авторизации через сервис - добавляет без проверок и не смешивает (она потом удаляется кодом извне без замены/смешивания)
// Если добавляет запись о регистрации, она смешивается с уже имеющейся авторизацией через сервис (если есть), либо создаёт новую
// Если добавляет пользователя, его запись смешивается с существующей
func InsertUserToDatabase(data *primitive.M) (*primitive.M, error) {
	log.Println("Попытка добавить пользователя.")
	collection := mongoClient.Database("LatterProject").Collection("Users")
	log.Println("Добавление пользователя: 10%.")

	// Проверка наличия почты
	email, has_old_data := (*data)["email"].(string)
	if !has_old_data || email == "Not linked" {
		return nil, fmt.Errorf("Не получилось создать пользователя, так как не получена почта.")
	}

	// Пробиваем по емэйл
	old_data, old_data_search_err := GetFromDatabaseByValue("Users", "email", email)

	log.Println("Добавление пользователя: 25%.")
	if old_data_search_err != nil {
		// Если это 100% новая запись для такого пользователя
		log.Println("Добавление пользователя: 55%.")
		// Если присвоен пользователю айди заранее
		if id, ok := (*data)["id"]; ok && len(id.(string)) > 0 {
			// Проверяет на занятость и, если нужно, присваивает новый
			_, err := GetFromDatabaseByValue("Users", "id", id.(string))
			if err == nil {
				(*data)["id"] = GenerateID()
			}
		} else {
			(*data)["id"] = GenerateID()
		}
		// Пока генерируется уже существующий айди, регенерируем
		for true {
			_, err := GetFromDatabaseByValue("Users", "id", (*data)["id"].(string))
			if err == nil {
				(*data)["id"] = GenerateID()
			} else {
				break
			}
		}
		// Инициализирует неназначенные изначальные данные // ini
		roles := primitive.A{"Student"}
		permissions := GenerateBasePermissionsListForRole("Student")
		(*data)["roles"] = roles
		(*data)["permissions"] = permissions
		// Записывает новый токен обновления
		new_refresh_token := GenerateNewRefreshToken(email)
		(*data)["refresh_tokens"] = primitive.A{new_refresh_token}
		// Записывает новый токен доступа
		new_access_token := GenerateNewAccessToken((*data)["id"].(string), roles, permissions)
		(*data)["access_token"] = new_access_token
		log.Println("Добавление пользователя: 79%.")
		// Добавляет в основной модуль | Сначала записать на сервер теста, а потом уже в БД
		note_url := os.Getenv("TEST_MODULE_URL") + "/users/note" // url для запроса
		note_token := GenerateNewAdminToken()                    // Создаём токен доступа админа
		note_data := &map[string]interface{}{
			"id":         (*data)["id"],
			"nickname":   (*data)["nickname"],
			"first_name": (*data)["first_name"],
			"last_name":  (*data)["last_name"],
			"patronimyc": (*data)["patronimyc"],
			"email":      (*data)["email"],
			"roles":      (*data)["roles"],
		}
		resp, err := DoPostRequestToService(note_url, "Bearer", note_token, note_data)
		defer resp.Body.Close()
		log.Println("Статус вставки пользователя в БД", resp.StatusCode)
		if err != nil || resp.StatusCode != 200 {
			return data, fmt.Errorf("Не удалось записать пользователя на серверную БД.")
		}
		log.Println("Добавление пользователя: 85%.")
		// Добавляет в свою базу данных
		_, err = collection.InsertOne(context.Background(), data)
		log.Println("Добавление пользователя: 100% (полностью новая запись).")
		return data, err
	} else { // Если уже есть такой емэил
		// перезаписываем данные, оставляя айди (и в моментах пароль)
		log.Println("Добавление пользователя: 75%.")
		// Меняет данные, которые нужно и те, которые не помешало бы
		(*old_data)["status"] = (*data)["status"]
		(*old_data)["login_token"] = (*data)["login_token"]
		// Роль остаётся // токены создаются
		roles := (*old_data)["roles"].(primitive.A)
		new_access_token := GenerateNewAccessToken((*old_data)["id"].(string), roles, GenerateBasePermissionsListForRoles(roles))
		new_refresh_token := GenerateNewRefreshToken(email)
		// Добавляет новый токен обновления к существующим. Если не было других - оздаём первым
		if refresh_tokens_slice, ok := (*old_data)["refresh_tokens"].(primitive.A); ok {
			refresh_tokens_slice = append(refresh_tokens_slice, new_refresh_token)
			(*old_data)["refresh_tokens"] = refresh_tokens_slice
		} else {
			(*old_data)["refresh_tokens"] = primitive.A{new_refresh_token}
		}
		// Записывает токен доступа
		(*old_data)["access_token"] = new_access_token
		// Заменяет в БД
		_, err := collection.ReplaceOne(context.Background(),
			bson.M{"id": (*old_data)["id"]}, old_data)
		log.Println("Добавление пользователя: 100% (замена старой записи email с новыми параметрами).")
		return old_data, err
	}
}

// Заменяет запись пользователя
func ReplaceFullUserInDatabase(id string, new_data *primitive.M) error {
	log.Println("Замена пользователя полностью по айди", id)
	collection := mongoClient.Database("LatterProject").Collection("Users")
	_, ok := (*new_data)["id"]
	if !ok {
		log.Println("Корректировка айди перед заменой.")
		(*new_data)["id"] = id
	}
	for key, value := range *new_data {
		// Проверяем, что значение — строка и она пустая
		if str, ok := value.(string); ok && str == "" {
			delete((*new_data), key)
		}
	}
	_, err := collection.ReplaceOne(context.Background(), bson.M{"id": id}, new_data)
	return err
}

// Запись авторизации
func InsertAuthorizationNoteToDatabase(data *primitive.M) (string, error) {
	// Входя через сервис, повтора не будет, так что просто присваиваем айди и вставляем.
	collection := mongoClient.Database("LatterProject").Collection("Authorizations")
	(*data)["id"] = GenerateID()
	_, err := collection.InsertOne(context.Background(), data)
	log.Println("Добавление записи входа по", (*data)["id"].(string))
	return (*data)["id"].(string), err
}

// Запись регистрации
func InsertRegistrationNoteToDatabase(data *primitive.M) (string, error) {
	// Входя через сервис, повтора не будет, так что просто присваиваем айди и вставляем.
	collection := mongoClient.Database("LatterProject").Collection("Registrations")
	(*data)["id"] = GenerateID()
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
	log.Println("Получение", db_name, ": 65%")
	// Делает фильтр
	filter := primitive.M{key_name: key}
	var user primitive.M
	err := collection.FindOne(
		context.Background(),
		filter,
	).Decode(&user)
	// Запасной вариант
	if err != nil {
		log.Println("Провал поиска по значениям. Начинаем посик в массивах")
		filter = primitive.M {key_name: primitive.M{"$in": []interface{}{key}},}
		err = collection.FindOne(
			context.Background(),
			filter,
		).Decode(&user)
	}
	// Принимает переменную и возвращает первого встреченного пользователя
	
	log.Println("Получение", db_name, ": 100%.", "Ошибка -", err)
	return &user, err
}

// Удаляет, если есть, из нужной таблицы БД
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

// отключается от БД
func DisconnectFromMongoDatabase() {
	fmt.Println("Отключение от БД.")
	err := mongoClient.Disconnect(context.Background())
	if err != nil {
		log.Fatal(err)
	}
}

// Генерация айди для записей
func GenerateID() string {
	/*// Берём наносекунды с эпохи Unix
	timestamp := time.Now().UnixMilli()
	// Добавляем случайное число от 0 до 999
	random := rand.Intn(1000)
	// Собираем в одну строку
	result := fmt.Sprintf("%d%03d", timestamp, random)*/
	return GenerateCode(9)
}
