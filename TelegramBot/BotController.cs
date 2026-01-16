using System;
using System.IO;
using System.Text;
using System.Collections;
using System.Collections.Generic;
using Telegram.Bot;
using Telegram.Bot.Types;
using Telegram.Bot.Types.Enums;
using Telegram.Bot.Types.ReplyMarkups;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Builder;
using Microsoft.Extensions.Hosting;
using System.Threading.Tasks;
using Newtonsoft.Json.Linq;

namespace TelegramBot {
    public class BotController
    {
        private RedisStorage redisStorage;
        private ApiClient authClient;
        private ApiClient testClient;
        private ITelegramBotClient botClient;

        public BotController(ITelegramBotClient botClient, RedisStorage redis)
        {
            // Инициализируем клиент бота
            this.botClient = botClient;
            // Подключается к редис
            redisStorage = redis;
            // Создаёт клиенты Основного модуля и Авторизации
            authClient = new ApiClient(ConfigLoader.Get("AUTHORIZATION_MODULE_URL"));
            testClient = new ApiClient(ConfigLoader.Get("TEST_MODULE_URL"));
            Console.WriteLine("Контроллер команд создан.");
        }
        // await botClient.SendTextMessageAsync(chatId, message);

        // Добывает данные из тела
        public static async Task<JObject> GetDataFromBody(Stream body)
        {
            // Читаем в строку
            StreamReader reader = new StreamReader(body, Encoding.UTF8);
            string jsonString = await reader.ReadToEndAsync();
            Console.WriteLine("Тело прочтено: " + jsonString);
            reader.Dispose();
            // Преобразуем в JObject
            return JObject.Parse(string.IsNullOrEmpty(jsonString) ? "{}" : jsonString);
        }
        // Делает ответ
        public static Task WriteResponseAsync(HttpContext context, int statusCode, string message)
        {
            context.Response.StatusCode = statusCode;
            context.Response.ContentType = "application/json";
            JObject result = new JObject();
            result["message"] = message;
            return context.Response.WriteAsync(result.ToString());
        }

        public void StartAsync(string host, int port)
        {
            // Формируем url
            string url = $"http://{host}:{port}";
            // Создаём билдер
            var builder = WebApplication.CreateBuilder();
            // Создаём сервер
            var app = builder.Build();

            // Связываем обработчики с маршрутами
            // Неизвестная команда
            app.MapPost("/unknown", async (HttpContext context) =>
            {
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                await botClient.SendMessage(chatId,
                @"Неизвестная команда (или нарушен синтаксис существующей). Введите /help для получения списка команд");
                
                await WriteResponseAsync(context, 200, "Информация отправлена.");
            });
            app.MapPost("/start", async (HttpContext context) =>
            {
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                await botClient.SendMessage(chatId,
                string.Join(Environment.NewLine, new[] {
                "Вас приветствует проект Latter.",
                "Введите /help для получения списка команд."
                }));
                
                await WriteResponseAsync(context, 200, "Информация отправлена.");
            });
            // Список команд
            app.MapPost("/help", async (HttpContext context) =>
            {
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;
                string filter = body["filter"].ToString();
                if (filter == "-all") filter = "-reg -login -user -news -discipline -test -question -try -answer";

                if (filter == "-commands") {
                    await botClient.SendMessage(chatId, 
                    string.Join(Environment.NewLine, new[] {
                    "Выберите тип команд, который хотите получить:",
                    "/help -all - Все команды.",
                    "/help -reg - Команды для регистрации аккаунта.",
                    "/help -login - Команды для авторизации аккаунта.",
                    "/help -news - Команды для просмотра сообщений аккаунта.",
                    "/help -discipline - Команды для работы с дисциплинами.",
                    "/help -test - Команды для работы с тестами.",
                    "/help -question - Команды для работы с вопросами.",
                    "/help -try - Команды для для работы с попытками.",
                    "/help -answer - Команды для для работы с ответами.",
                    }));
                }
                else {
                    if (filter.Contains("-reg")) {
                        await botClient.SendMessage(chatId, 
                        string.Join(Environment.NewLine, new[] {
                        "/reg (тип) (параметры) - Регистрация аккаунта. Тип регистрации и запрашиваемые параметры:",
                        "-init (Проверить возможность регистрации аккаунта и получить код подтверждения): почта, никнейм, имя, фамилия, отчество, пароль, повтор пароля;",
                        "-verify (Подтверждение регистрации): код с почты",
                        }));
                    }
                    if (filter.Contains("-login")) {
                        await botClient.SendMessage(chatId, 
                        string.Join(Environment.NewLine, new[] {
                        "/login (тип) (параметры) - Авторизоваться выбранным способом. Тип авторизации и запрашиваемые параметры:",
                        "-default (Обычная авторизация с паролем): почта, пароль;",
                        "-code (Авторизация с отправкой кода на авторизированный аккаунт): почта, код (если не будет введён код, он сгенерируется и его можно будет увидеть в разделе news на авторизированном устройстве)",
                        "-yandex (Регистрация через сервисы Яндекс): без параметров (будет отправлена ссылка на Яндекс OAuth авторизацию)",
                        "-github (Регистрация через сервисы GitHub): без параметров (будет отправлена ссылка на Github OAuth авторизацию)",
                        "/logout - Выход из аккаунта.",
                        "/logout -all - Выход из аккаунта со всех устройств.",
                        }));
                    }
                    if (filter.Contains("-news")) {
                        await botClient.SendMessage(chatId, 
                        string.Join(Environment.NewLine, new[] {
                        "/news - Список всех ваших сообщений",
                        "/news -status (айди) Deleted - Удаляет сообщение из списка"
                        }));
                    }
                    if (filter.Contains("-user")) {
                        await botClient.SendMessage(chatId, 
                        string.Join(Environment.NewLine, new[] {
                        "/users - Список всех пользователей",
                        "/user (операция) (айди) (параметр) - Операция с пользователем с ID. Наличие/Отсутствие параметра определяет его Чтение/Запись",
                        "/me (операция) (параметр) - то же самое, но вместо айди автоматически подставляется ваш",
                        "Существующие операции:",
                        "-nickname (Никнейм пользователя)",
                        "-fullname (ФИО пользователя. При установке перечисляются 3 параметра: имя, фамилия, отчество)",
                        "-roles (Роли пользователя)",
                        "-status (Статус пользователя)",
                        "-profile (Всё вышеперечисленное)",
                        "-disciplines (Дисциплины пользователя. Не доступно для записи)",
                        "-tests (Пройденные тесты пользователя. Не доступно для записи)",
                        "-grades (Оценки за тесты пользователя. Не доступно для записи)"
                        }));
                    }
                    if (filter.Contains("-discipline")) {
                        await botClient.SendMessage(chatId, 
                        string.Join(Environment.NewLine, new[] {
                        "/disciplines - Список всех дисциплин",
                        "/discipline -create (название) (описание) (айди преподавателя) - Создание новой дисциплины",
                        "/discipline -status (айди) Deleted - Удаление дисциплины",
                        "/discipline -info (айди) (название) (описание) - Чтение/Установка названия и описания дисциплины",
                        "/discipline -teachers - Преподаватели дисциплины",
                        "/discipline -tests - Тесты дисциплины",
                        "/discipline -students - Студенты дисциплины",
                        "/discipline -addteacher (айди дисциплины) (айди пользователя) - Добавляет преподавателя в дисциплину",
                        "/discipline -addtest (айди дисциплины) (название) - Добавляет новый тест в дисциплину",
                        "/discipline -sub (айди дисциплины) - Записаться на дисциплину",
                        "/discipline -remuser (айди дисциплины) (айди пользователя) - Удаляет пользователя из дисциплины"
                        }));
                    }
                    if (filter.Contains("-test")) {
                        await botClient.SendMessage(chatId, 
                        string.Join(Environment.NewLine, new[] {
                        "/test -info (айди теста) (название) (описание) - Чтение/Установка названия и описания теста",
                        "/test -status (айди) Activated/Deactivated/Deleted - Активация/Деактивация/Удаление теста",
                        "/test -tries (айди теста) - Попытки прохождения теста",
                        "/test -tries (айди теста) (айди пользователя) - Попытки прохождения теста определённого пользователя",
                        "/test -modify (айди теста) - Возвращает всю информацию о вопросах, которые присутствуют в тесте",
                        "/test -addques (айди теста) (айди вопроса) (версия вопроса) - Добавляет в тест с ID вопрос с ID и указанной версией (если версия не указана, добавится последняя)",
                        "/test -remques (айди теста) (номер вопроса) - Удаляем вопрос под определённы номером",
                        "/test -quesseq (айди теста) -q (айди) (версия) - Устанавливает тесту последовательность переданных через -q вопросов (айди) (версия). Если версия не будет указана, используется последняя",
                        }));
                    }
                    if (filter.Contains("-question")) {
                        await botClient.SendMessage(chatId, 
                        string.Join(Environment.NewLine, new[] {
                        "/questions - Список всех вопросов",
                        "/question -create (название) (кол-во вариантов ответа) -o (текст ответа) (кол-во очков) - Создание вопроса (через -opinfo передаются параметры отдельного ответа)",
                        "/question -update (айди) (название) (кол-во вариантов ответа) -o (текст ответа) (кол-во очков) - Создаёт новую версию существующего вопроса (аналогично созданию вопроса, но добавляется параметр ID в операции update)",
                        "/question -status (айди) Deleted - Удаление вопроса (он будет недоступен для получения, но это не коснётся тестов, которые его уже используют, его)",
                        "/question -info (айди) (версия) - Посмотреть всю информацию о вопросе (если не ввести версию, будет использована последняя)"
                        }));
                    }
                    if (filter.Contains("-tries")) {
                        await botClient.SendMessage(chatId, 
                        string.Join(Environment.NewLine, new[] {
                            "/try -start (айди теста) - Выводит все вопросы теста и начинает попытку прохождения",
                            "/try -stop (айди попытки) - Заканчивает попытку прохождения",
                            "/try -view (айди попытки) - Посмотреть ответы и результаты попытки прохождения",
                            "/answer -save (айди) (варианты ответа) - Сохраняет ответы на вопрос",
                            "/answer -delete (айди) - Обнуляет ответ на вопрос",
                            "Команда /answer поддерживает более одного действия. Например, можно сохранить несколько вопросов путём указания более одного -save с параметрами"
                        }));
                    }

                    await botClient.SendMessage(chatId, 
                    string.Join(Environment.NewLine, new[] {
                    "Примечания:",
                    "1) Параметры передаются через пробел.",
                    "2) Чтобы передать в качестве параметра текст, содержащий пробелы, его необходимо заключить в двойные кавычки. Если они присутствуют в тексте - их следует дублировать.",
                    "3) Заключение параметра в двойные кавычки также необходимо, если он начинается с символа '-' или '/'",
                    "4) Поддерживается обработка нескольких команд в одном сообщении. Для примера можете ввести: /help -reg /help -login -user"
                    }));
                }

                await WriteResponseAsync(context, 200, "Информация отправлена.");
            });
            // Регистрация
            app.MapPost("/init/registration", async (HttpContext context) =>
            {
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                await botClient.SendMessage(chatId, 
                string.Join(Environment.NewLine, new[] {
                "Отправляем запрос на регистрацию пользователя:",
                $"Почта: {body["email"].ToString()}",
                $"Никнейм: {body["nickname"].ToString()}",
                $"Имя: {body["first_name"].ToString()}",
                $"Фамилия: {body["last_name"].ToString()}",
                $"Отчество: {body["patronimyc"].ToString()}",
                }));

                // Создаём тело запроса
                JObject request_body = new JObject();
                request_body["email"] = body["email"].ToString();
                request_body["nickname"] = body["nickname"].ToString();
                request_body["first_name"] = body["first_name"].ToString();
                request_body["last_name"] = body["last_name"].ToString();
                request_body["patronimyc"] = body["patronimyc"].ToString();
                request_body["password"] = body["password"].ToString();
                request_body["repeat_password"] = body["repeat_password"].ToString();
                // Отправляем запрос на регистрацию
                ApiClient.Response response = await authClient.PostAsync("init/registration", request_body);

                // Если получилось
                if (response.Status == 200) {
                    // Сохраняем в redis
                    JObject newUser = new JObject();
                    newUser["status"] = "Unknown";
                    newUser["registration_state"] = response.Body["state"].ToString();
                    await redisStorage.SetAsync(chatId, newUser);
                    // Сообщаем, что получилось
                    await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                        response.Body["message"].ToString() : "Ошибка выполнения запроса");
                }
                // Иначе сообщаем о неудаче
                else await botClient.SendMessage(chatId,
                    response.Body.ContainsKey("message") ? response.Body["message"].ToString() : "Ошибка сервера.");
            });
            app.MapPost("/verify/registration", async (HttpContext context) =>
            {
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                // Проверяем на наличие регистрации
                JObject note = await redisStorage.GetAsync(chatId);
                if (!note.ContainsKey("registration_state")) {
                    await botClient.SendMessage(chatId, "Нет данных о регистрации для подтверждения.");
                    return;
                }

                // Создаём тело запроса
                JObject request_body = new JObject();
                request_body["code"] = body["code"].ToString();
                request_body["state"] = note["registration_state"].ToString();
                // Отправляем запрос на регистрацию
                ApiClient.Response response = await authClient.PostAsync("verify/registration", request_body);

                // Если получилось
                if (response.Status == 200) {
                    // Удаляем в redis
                    await redisStorage.RemoveAsync(chatId);
                    // Сообщаем, что получилось
                    await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                        response.Body["message"].ToString() : "Ошибка выполнения запроса");
                }
                // Иначе сообщаем о неудаче
                else await botClient.SendMessage(chatId,
                    response.Body.ContainsKey("message") ? response.Body["message"].ToString() : "Ошибка сервера.");
            });
            // Авторизация
            app.MapPost("/init/authorization", async (HttpContext context) =>
            {
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                await botClient.SendMessage(chatId, $"Отправляем запрос на регистрацию пользователя {body["email"].ToString()}");

                // Создаём тело запроса
                JObject request_body = new JObject();
                request_body["email"] = body["email"].ToString();
                // Отправляем запрос на регистрацию
                ApiClient.Response response = await authClient.PostAsync("init/authorization", request_body);

                // Если получилось
                if (response.Status == 200) {
                    // Сохраняем в redis
                    JObject newUser = new JObject();
                    newUser["status"] = "Unknown";
                    newUser["authorization_state"] = response.Body["state"].ToString();
                    await redisStorage.SetAsync(chatId, newUser);
                    // Сообщаем, что получилось
                    await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                        response.Body["message"].ToString() : "Ошибка выполнения запроса");
                }
                // Иначе сообщаем о неудаче
                else await botClient.SendMessage(chatId,
                    response.Body.ContainsKey("message") ? response.Body["message"].ToString() : "Ошибка сервера.");
            });
            app.MapPost("/login", async (HttpContext context) =>
            {
                // Получение тела, чата айди, 
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                // Получение типа входа
                if (!context.Request.Query.ContainsKey("type")) {
                    await botClient.SendMessage(chatId, "Неверный вызов обработчика.");
                    return;
                }
                string type = context.Request.Query["type"];

                // и записи из хранилища
                JObject user = await redisStorage.GetAsync(chatId);

                // Обработка уже авторизированного пользователя
                if (user.ContainsKey("status") && user["status"].ToString() == "Authorized") {
                    await botClient.SendMessage(chatId, "Чтобы провести авторизацию, необходимо выйти из аккаунта.");
                    return;
                }
                
                // Создаём тело запроса
                JObject request_body = new JObject();
                // Создаём рандомный токен входа
                string loginToken = GenerateRandomToken(12);
                request_body["login_token"] = loginToken;
                
                // Сохраняем токен и статус Анонима
                user["status"] = "Anonimous";
                user["login_token"] = loginToken;
                await redisStorage.SetAsync(chatId, user);

                // Далее в зависимости от параметра
                if (type == "default" || type == "code") {
                    // Настраиваем тело запроса и отправляем его в зависимости от типа
                    ApiClient.Response auth_response;
                    if (type == "default") {
                        request_body["email"] = body["email"].ToString();
                        request_body["password"] = body["password"].ToString();
                        // Отправляем запрос
                        await botClient.SendMessage(chatId, "Запрос на авторизацию паролем отправлен.");
                        auth_response = await authClient.PostAsync("auth/default", request_body, null, new ApiClient.Token("Bearer", loginToken)); 
                    }
                    else /*code*/ {
                        // Проверяем на иниализацию
                        if (!user.ContainsKey("authorization_state")) {
                            await botClient.SendMessage(chatId, "Нет данных об авторизации для подтверждения.");
                            return;
                        }

                        // Настраиваем тело запроса
                        request_body["code"] = body["code"];
                        request_body["state"] = user["authorization_state"];
                        // Отправляем запрос
                        await botClient.SendMessage(chatId, "Запрос на авторизацию кодом отправлен.");
                        auth_response = await authClient.PostAsync("verify/authorization", request_body, null, new ApiClient.Token("Bearer", loginToken)); 
                    }

                    // Если получилось авторизоваться
                    if (auth_response.Status == 200) {
                        // Редирект на обработчик запроса
                        context.Response.Redirect("/login/confirm?chat_id="+chatId);
                    }
                    // Иначе сообщаем о неудаче
                    else await botClient.SendMessage(chatId,
                        auth_response.Body.ContainsKey("message") ? auth_response.Body["message"].ToString() : "Ошибка авторизации.");  
                }
                else if (type == "yandex" || type == "github") {
                    // Настраиваем тело запроса
                    request_body["session_token"] = chatId.ToString();
                    request_body["service_type"] = type;
                    request_body["callback_url"] = url + "/service_auth_callback";

                    // Отправляем запрос
                    await botClient.SendMessage(chatId, "Запрос на получение ссылки для авторизации отправлен.");
                    ApiClient.Response service_response = await authClient.PostAsync("auth/service", request_body, null, new ApiClient.Token("Bearer", loginToken));
                    // Если получилось авторизоваться
                    if (service_response.Status == 200) {
                        // Отправляем пользователю ссылку
                        await botClient.SendMessage(chatId,
                            "Перейдите по ссылке для дальнейшей авторизации через выбранный сервис: " +
                                service_response.Body["oauth_redirect_url"].ToString());
                    }
                    // Иначе сообщаем о неудаче
                    else await botClient.SendMessage(chatId,
                        service_response.Body.ContainsKey("message") ? service_response.Body["message"].ToString() : "Ошибка авторизации.");
                }
            });
            app.MapGet("/service_auth_callback", async (HttpContext context) =>
            {
                // Если не передан параметр - хана
                Console.WriteLine(context.Request.Query["session_token"]);
                if (!context.Request.Query.ContainsKey("session_token"))
                {
                    return;
                }
                
                string cahtIdString = context.Request.Query["session_token"];
                // Пишем
                await botClient.SendMessage(long.Parse(cahtIdString), "Получен ответ.");
                // Редирект на обработчик запроса
                context.Response.Redirect("/login/confirm?chat_id="+cahtIdString);
            });
            app.MapGet("/login/confirm", async (HttpContext context) =>
            {
                // Айди чата хранится в вопросе
                if (!context.Request.Query.ContainsKey("chat_id") || !long.TryParse(context.Request.Query["chat_id"], out long chatId)) {
                    Console.WriteLine("Не получилось запарсить id строку в long");
                    return;
                }
                bool contains = await redisStorage.ExistsAsync(chatId);
                if (!contains) {
                    await botClient.SendMessage(chatId, "Запись для подтверждения не найдена.");
                    return;
                }

                // Получаем запись из хранилища
                JObject user = await redisStorage.GetAsync(chatId);

                /* Из-за того, что в Веб Клиенте куки не сохраняется до успешого входа, нет ключа, чтобы получить запись из редис
                   и вызвать подтверждение, а в боте у нас всегда известен ключ - айди чата, поэтому можно в любой момент вызвать подтверждение,
                   даже, если нет записи в модуле Авторизации, поэтому, чтобы лишний раз не делать запрос, сделаем проверку хотя бы на это: */
                // Если логин токена нет и статус не аноним - нечего подтверждать
                if (!user.ContainsKey("login_token") || !user.ContainsKey("status") || user["status"].ToString() != "Anonimous") {
                    await botClient.SendMessage(chatId, "Запись уже подтверждена, либо не обработана.");
                    return;
                }

                // Делаем запрос на вход
                ApiClient.Response confirm_response = await authClient.GetAsync("auth/confirm", null, new ApiClient.Token("Bearer", user["login_token"].ToString())); 
                // Если получилось
                if (confirm_response.Status == 200) {
                    // Сохраняем в redis
                    JObject newUser = new JObject();
                    newUser["status"] = "Authorized";
                    newUser["id"] = confirm_response.Body["id"].ToString();
                    newUser["email"] = confirm_response.Body["email"].ToString();
                    newUser["nickname"] = confirm_response.Body["nickname"].ToString();
                    newUser["access_token"] = confirm_response.Body["access_token"].ToString();
                    newUser["refresh_token"] = confirm_response.Body["refresh_token"].ToString();
                    await redisStorage.SetAsync(chatId, newUser);
                    // Сообщаем, что получилось
                    await botClient.SendMessage(chatId, confirm_response.Body["message"].ToString());
                }
                else
                {
                    // Иначе сообщаем о неудаче
                    await botClient.SendMessage(chatId,
                        confirm_response.Body.ContainsKey("message") ? confirm_response.Body["message"].ToString() : "Ошибка входа.");
                    // Удаляем запись
                    await redisStorage.RemoveAsync(chatId);
                }
            });
            app.MapPost("/logout", async (HttpContext context) =>
            {
                // Получение тела, чата айди
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                // Проверка на запись
                bool exists = await redisStorage.ExistsAsync(chatId);
                if (!exists)
                {
                    botClient.SendMessage(chatId, "Сначала необходимо авторизоваться.");
                    return;
                }
                // Запись из хранилища
                JObject user = await redisStorage.GetAsync(chatId);
                // Проверка на авторизацию
                if (user["status"].ToString() != "Authorized")
                {
                    await botClient.SendMessage(chatId, "Выход недоступен из неавторизованной записи.");
                    return;
                }

                // Удаление записи
                await redisStorage.RemoveAsync(chatId);

                // Удаление со всех устройств, если нужно
                bool all = bool.Parse(body["all"].ToString());
                if (all) await authClient.GetAsync("logout/all", null, new ApiClient.Token("Bearer", user["refresh_token"].ToString()));
                await botClient.SendMessage(chatId, $"Выход {(all ? "со всех устройств" : "")} осуществлён.");
            });
            // Пользователи
            app.MapPost("/users", async (HttpContext context) =>
            {
                // Получение тела, чата айди
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                JObject user = await redisStorage.GetAuthorizeCheckedAsync(chatId);
                if (user["authorized"].ToString() == "false")
                {
                    await botClient.SendMessage(chatId, "Сначала необходимо авторизоваться.");
                    return;
                }

                // Делаем запрос
                ApiClient.Response response = await testClient.GetAsync("users/list", null,
                    new ApiClient.Token("Bearer", user["access_token"].ToString(),
                        new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));
                        
                // Обработка отказа
                if (response.Status != 200)
                {
                    await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                        response.Body["message"].ToString() : "Ошибка выполнения запроса");
                    return;
                }

                // Выводим в зависимости от содержимого
                List<string> info = new List<string>();
                info.Add("Список пользователей:");
                foreach (JToken item in (JArray)response.Body["users"])
                {
                    info.Add($"{item["user_id"].ToString()}: {item["fullname"].ToString()}");
                }
                
                // Выводит
                await botClient.SendMessage(chatId, string.Join(Environment.NewLine, info));
            });
            app.MapPost("/user/info/read", async (HttpContext context) =>
            {
                // Получение тела, чата айди
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                JObject user = await redisStorage.GetAuthorizeCheckedAsync(chatId);
                if (user["authorized"].ToString() == "false")
                {
                    await botClient.SendMessage(chatId, "Сначала необходимо авторизоваться.");
                    return;
                }

                // Заполняем query для запроса
                Dictionary<string, string> request_query = new Dictionary<string, string>();
                request_query["user_id"] = body.ContainsKey("user_id") ? body["user_id"].ToString() : user["id"].ToString();
                request_query["filter"] = body["filter"].ToString();

                // Делаем запрос
                ApiClient.Response response = await testClient.GetAsync("users/data", request_query,
                    new ApiClient.Token("Bearer", user["access_token"].ToString(),
                        new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));

                // Обработка отказа
                if (response.Status != 200)
                {
                    await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                        response.Body["message"].ToString() : "Ошибка выполнения запроса");
                    return;
                }

                // Выводим в зависимости от содержимого
                List<string> info = new List<string>();
                info.Add($"Информация о пользователе {request_query["user_id"]}:");
                if (response.Body.ContainsKey("email")) info.Add("Почта: " + response.Body["email"].ToString());
                if (response.Body.ContainsKey("nickname")) info.Add("Никнейм: " + response.Body["nickname"].ToString());
                if (response.Body.ContainsKey("fullname")) info.Add("ФИО: " + response.Body["fullname"].ToString());
                if (response.Body.ContainsKey("status")) info.Add("Статус: " + response.Body["status"].ToString());
                if (response.Body.ContainsKey("roles"))
                {
                    int roles_count = 0;
                    string roles = "";
                    foreach (JToken item in (JArray)response.Body["roles"])
                    {
                        roles += " " + item.ToString();
                        roles_count++;
                    }

                    info.Add((roles_count > 1 ? "Роли:" : "Роль:") + roles);
                }
                if (response.Body.ContainsKey("disciplines"))
                {
                    info.Add("Дисциплины пользвователя:");
                    foreach (JToken item in (JArray)response.Body["disciplines"])
                    {
                        info.Add($"{item["discipline_id"].ToString()}: {item["title"].ToString()} {(item["status"].ToString() == "Deleted" ? "(Удалена)" : "")}");
                    }
                }
                if (response.Body.ContainsKey("tests"))
                {
                    info.Add("Тесты пользвователя:");
                    foreach (JToken item in (JArray)response.Body["tries"])
                    {
                        info.Add($"{item["test_id"].ToString()}: {item["test_title"].ToString()} - {item["test_status"].ToString()}");
                    }
                }
                if (response.Body.ContainsKey("tries"))
                {
                    info.Add("Оценки пользвователя:");
                    foreach (JToken item in (JArray)response.Body["tries"])
                    {
                        info.Add($"{item["try_id"].ToString()}: {item["test_title"].ToString()} ({item["test_id"].ToString()}) - " +
                        $"{item["points"].ToString()}/{item["max_points"].ToString()} ({item["score_percent"].ToString()}%)");
                    }
                }
                // Выводит
                await botClient.SendMessage(chatId, string.Join(Environment.NewLine, info));
            });
            app.MapPost("/user/info/write", async (HttpContext context) =>
            {
                // Получение тела, чата айди
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                JObject user = await redisStorage.GetAuthorizeCheckedAsync(chatId);
                if (user["authorized"].ToString() == "false")
                {
                    await botClient.SendMessage(chatId, "Сначала необходимо авторизоваться.");
                    return;
                }

                // Заполняем тело для запроса
                JObject request_body = new JObject();
                request_body["user_id"] = body.ContainsKey("user_id") ? body["user_id"].ToString() : user["id"].ToString();

                // В зависимости от операции настраиваем тело запроса
                string endpoint = "";
                JArray values = (JArray)body["values"];
                switch (body["operation"].ToString())
                {
                    case "nickname":
                        endpoint = "users/nickname";
                        request_body["nickname"] = values[0].ToString();
                        break;
                    case "fullname":
                        endpoint = "users/fullname";
                        request_body["last_name"] = values[0].ToString();
                        request_body["first_name"] = values[1].ToString();
                        request_body["patronimyc"] = values[2].ToString();
                        break;
                    case "roles":
                        endpoint = "users/nickname";
                        request_body["roles"] = values;
                        break;
                    case "status":
                        endpoint = "users/status";
                        request_body["status"] = values[0].ToString();
                        break;
                }

                if (endpoint == "")
                {
                    Console.WriteLine("Нет обработчика для установки данного сообщения");
                    return;
                }

                // Делаем запрос
                ApiClient.Response response = await testClient.PostAsync(endpoint, request_body, null,
                    new ApiClient.Token("Bearer", user["access_token"].ToString(),
                        new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));

                // Выводит
                await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                        response.Body["message"].ToString() : "Ошибка выполнения запроса");
            });
            // Дисциплины
            app.MapPost("/disciplines", async (HttpContext context) =>
            {
                // Получение тела, чата айди
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                JObject user = await redisStorage.GetAuthorizeCheckedAsync(chatId);
                if (user["authorized"].ToString() == "false")
                {
                    await botClient.SendMessage(chatId, "Сначала необходимо авторизоваться.");
                    return;
                }

                // Делаем запрос
                ApiClient.Response response = await testClient.GetAsync("disciplines/list", null,
                    new ApiClient.Token("Bearer", user["access_token"].ToString(),
                        new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));
                        
                // Обработка отказа
                if (response.Status != 200)
                {
                    await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                        response.Body["message"].ToString() : "Ошибка выполнения запроса");
                    return;
                }

                // Выводим в зависимости от содержимого
                List<string> info = new List<string>();
                info.Add("Список дисциплин:");
                foreach (JToken item in (JArray)response.Body["disciplines"])
                {
                    info.Add($"{item["discipline_id"].ToString()}: {item["title"].ToString()}");
                }
                
                // Выводит
                await botClient.SendMessage(chatId, string.Join(Environment.NewLine, info));
            });
            app.MapPost("/discipline/create", async (HttpContext context) =>
            {
                // Получение тела, чата айди
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                JObject user = await redisStorage.GetAuthorizeCheckedAsync(chatId);
                if (user["authorized"].ToString() == "false")
                {
                    await botClient.SendMessage(chatId, "Сначала необходимо авторизоваться.");
                    return;
                }

                // Заполняем тело для запроса
                JObject request_body = new JObject();
                request_body["title"] = body["title"].ToString();
                request_body["describtion"] = body["describtion"].ToString();
                request_body["teacher_id"] = body["teacher_id"].ToString();
                
                // Делаем запрос
                ApiClient.Response response = await testClient.PostAsync("disciplines/create", request_body, null,
                    new ApiClient.Token("Bearer", user["access_token"].ToString(),
                        new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));
                
                // Обработка отказа
                if (response.Status != 200)
                {
                    await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                        response.Body["message"].ToString() : "Ошибка выполнения запроса");
                    return;
                }

                // Отправляем уведомление
                await botClient.SendMessage(chatId,
                    $"Дисциплина {body["title"].ToString()} ({response.Body["discipline_id"].ToString()}) успешно создана.");
            });
            app.MapPost("/discipline/status", async (HttpContext context) =>
            {
                // Получение тела, чата айди
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                JObject user = await redisStorage.GetAuthorizeCheckedAsync(chatId);
                if (user["authorized"].ToString() == "false")
                {
                    await botClient.SendMessage(chatId, "Сначала необходимо авторизоваться.");
                    return;
                }

                // Если передан параметр для присваивания
                if (body.ContainsKey("status"))
                {
                    // Заполняем тело для запроса
                    JObject request_body = new JObject();
                    request_body["status"] = body["status"].ToString();
                    request_body["discipline_id"] = body["discipline_id"].ToString();

                    // Делаем запрос
                    ApiClient.Response response = await testClient.PostAsync("disciplines/status", request_body, null,
                        new ApiClient.Token("Bearer", user["access_token"].ToString(),
                            new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));

                    // Обработка отказа
                    if (response.Status != 200)
                    {
                        await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                            response.Body["message"].ToString() : "Ошибка выполнения запроса");
                        return;
                    }

                    // Отправляем уведомление
                    await botClient.SendMessage(chatId,
                        $"Статус дисциплины {response.Body["title"].ToString()} ({body["discipline_id"].ToString()}) успешно обновлён.");
                }
                else
                {
                    // Заполняем query для запроса
                    Dictionary<string, string> request_query = new Dictionary<string, string>();
                    request_query["discipline_id"] = body["discipline_id"].ToString();
                    request_query["filter"] = "text status";

                    // Делаем запрос
                    ApiClient.Response response = await testClient.GetAsync("disciplines/data", request_query,
                        new ApiClient.Token("Bearer", user["access_token"].ToString(),
                            new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));

                    // Обработка отказа
                    if (response.Status != 200)
                    {
                        await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                            response.Body["message"].ToString() : "Ошибка выполнения запроса");
                        return;
                    }

                    // Отправляем уведомление
                    await botClient.SendMessage(chatId,
                        $"Статус дисциплины {response.Body["title"].ToString()} ({request_query["discipline_id"]}) - {response.Body["status"].ToString()}");
                }
            });
            app.MapPost("/discipline/info", async (HttpContext context) =>
            {
                // Получение тела, чата айди
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                JObject user = await redisStorage.GetAuthorizeCheckedAsync(chatId);
                if (user["authorized"].ToString() == "false")
                {
                    await botClient.SendMessage(chatId, "Сначала необходимо авторизоваться.");
                    return;
                }

                // Если передан параметр для присваивания
                if (body.ContainsKey("title") && body.ContainsKey("describtion")) {
                    // Заполняем тело для запроса
                    JObject request_body = new JObject();
                    request_body["title"] = body["title"].ToString();
                    request_body["describtion"] = body["describtion"].ToString();
                    request_body["discipline_id"] = body["discipline_id"].ToString();

                    // Делаем запрос
                    ApiClient.Response response = await testClient.PostAsync("disciplines/text", request_body, null,
                        new ApiClient.Token("Bearer", user["access_token"].ToString(),
                            new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));
                    
                    // Обработка отказа
                    if (response.Status != 200)
                    {
                        await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                            response.Body["message"].ToString() : "Ошибка выполнения запроса");
                        return;
                    }

                    // Отправляем уведомление
                    await botClient.SendMessage(chatId,
                        $"Информация о дисциплине {response.Body["title"].ToString()} ({body["discipline_id"].ToString()}) успешно обновлена.");
                }
                else
                {
                    // Заполняем query для запроса
                    Dictionary<string, string> request_query = new Dictionary<string, string>();
                    request_query["discipline_id"] = body["discipline_id"].ToString();
                    request_query["filter"] = "text";

                    // Делаем запрос
                    ApiClient.Response response = await testClient.GetAsync("disciplines/data", request_query,
                        new ApiClient.Token("Bearer", user["access_token"].ToString(),
                            new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));
                    
                    // Обработка отказа
                    if (response.Status != 200)
                    {
                        await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                            response.Body["message"].ToString() : "Ошибка выполнения запроса");
                        return;
                    }

                    // Отправляем уведомление
                    await botClient.SendMessage(chatId, string.Join(Environment.NewLine, new string[] {
                        $"Информация о дисциплине {request_query["discipline_id"]}:",
                        $"Название: {response.Body["title"].ToString()}",
                        $"Описание: {response.Body["describtion"].ToString()}"
                    }));
                }
            });
            app.MapPost("/discipline/teachers", async (HttpContext context) =>
            {
                // Получение тела, чата айди
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                JObject user = await redisStorage.GetAuthorizeCheckedAsync(chatId);
                if (user["authorized"].ToString() == "false")
                {
                    await botClient.SendMessage(chatId, "Сначала необходимо авторизоваться.");
                    return;
                }

                // Заполняем query для запроса
                Dictionary<string, string> request_query = new Dictionary<string, string>();
                request_query["filter"] = "teacherslist text";
                request_query["discipline_id"] = body["discipline_id"].ToString();

                // Делаем запрос
                ApiClient.Response response = await testClient.GetAsync("disciplines/data", request_query,
                    new ApiClient.Token("Bearer", user["access_token"].ToString(),
                        new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));
                
                // Обработка отказа
                if (response.Status != 200)
                {
                    await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                        response.Body["message"].ToString() : "Ошибка выполнения запроса");
                    return;
                }

                // Выводим в зависимости от содержимого
                List<string> info = new List<string>();
                info.Add($"Список преподавателей дисциплины {response.Body["title"].ToString()}:");
                foreach (JToken item in (JArray)response.Body["teachers"])
                {
                    info.Add($"{item["user_id"].ToString()}: {item["fullname"].ToString()}");
                }
                // Отправляем уведомление
                await botClient.SendMessage(chatId, string.Join(Environment.NewLine, info));
            });
            app.MapPost("/discipline/tests", async (HttpContext context) =>
            {
                // Получение тела, чата айди
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                JObject user = await redisStorage.GetAuthorizeCheckedAsync(chatId);
                if (user["authorized"].ToString() == "false")
                {
                    await botClient.SendMessage(chatId, "Сначала необходимо авторизоваться.");
                    return;
                }

                // Заполняем query для запроса
                Dictionary<string, string> request_query = new Dictionary<string, string>();
                request_query["filter"] = "testslist text";
                request_query["discipline_id"] = body["discipline_id"].ToString();

                // Делаем запрос
                ApiClient.Response response = await testClient.GetAsync("disciplines/data", request_query,
                    new ApiClient.Token("Bearer", user["access_token"].ToString(),
                        new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));
                
                // Обработка отказа
                if (response.Status != 200)
                {
                    await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                        response.Body["message"].ToString() : "Ошибка выполнения запроса");
                    return;
                }

                // Выводим в зависимости от содержимого
                List<string> info = new List<string>();
                info.Add($"Список тестов дисциплины {response.Body["title"].ToString()}:");
                foreach (JToken item in (JArray)response.Body["tests"])
                {
                    info.Add($"{item["test_id"].ToString()}: {item["title"].ToString()} ({item["status"].ToString()})");
                }
                // Отправляем уведомление
                await botClient.SendMessage(chatId, string.Join(Environment.NewLine, info));
                
            });
            app.MapPost("/discipline/students", async (HttpContext context) =>
            {
                // Получение тела, чата айди
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                JObject user = await redisStorage.GetAuthorizeCheckedAsync(chatId);
                if (user["authorized"].ToString() == "false")
                {
                    await botClient.SendMessage(chatId, "Сначала необходимо авторизоваться.");
                    return;
                }

                // Заполняем query для запроса
                Dictionary<string, string> request_query = new Dictionary<string, string>();
                request_query["filter"] = "studentslist text";
                request_query["discipline_id"] = body["discipline_id"].ToString();

                // Делаем запрос
                ApiClient.Response response = await testClient.GetAsync("disciplines/data", request_query,
                    new ApiClient.Token("Bearer", user["access_token"].ToString(),
                        new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));
                
                // Обработка отказа
                if (response.Status != 200)
                {
                    await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                        response.Body["message"].ToString() : "Ошибка выполнения запроса");
                    return;
                }

                // Выводим в зависимости от содержимого
                List<string> info = new List<string>();
                info.Add($"Список студентов дисциплины {response.Body["title"].ToString()}:");
                foreach (JToken item in (JArray)response.Body["students"])
                {
                    info.Add($"{item["user_id"].ToString()}: {item["fullname"].ToString()}");
                }
                // Отправляем уведомление
                await botClient.SendMessage(chatId, string.Join(Environment.NewLine, info));
            });
            app.MapPost("/discipline/test/add", async (HttpContext context) =>
            {
                // Получение тела, чата айди
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                JObject user = await redisStorage.GetAuthorizeCheckedAsync(chatId);
                if (user["authorized"].ToString() == "false")
                {
                    await botClient.SendMessage(chatId, "Сначала необходимо авторизоваться.");
                    return;
                }

                // Заполняем тело для запроса
                JObject request_body = new JObject();
                request_body["title"] = body["title"].ToString();
                request_body["discipline_id"] = body["discipline_id"].ToString();
                
                // Делаем запрос
                ApiClient.Response response = await testClient.PostAsync("disciplines/tests/add", request_body, null,
                    new ApiClient.Token("Bearer", user["access_token"].ToString(),
                        new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));
                    
                // Обработка отказа
                if (response.Status != 200)
                {
                    await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                        response.Body["message"].ToString() : "Ошибка выполнения запроса");
                    return;
                }

                // Отправляем уведомление
                await botClient.SendMessage(chatId,
                    $"Тест {request_body["title"]} ({response.Body["test_id"].ToString()}) успешно создан. Начальный статус - {response.Body["status"].ToString()}.");       
            });
            app.MapPost("/discipline/user/add", async (HttpContext context) =>
            {
                // Получение тела, чата айди
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                JObject user = await redisStorage.GetAuthorizeCheckedAsync(chatId);
                if (user["authorized"].ToString() == "false")
                {
                    await botClient.SendMessage(chatId, "Сначала необходимо авторизоваться.");
                    return;
                }

                // Заполняем тело для запроса
                JObject request_body = new JObject();
                if (body.ContainsKey("user_id")) request_body["user_id"] = body["user_id"].ToString();
                request_body["user_type"] = body["user_type"].ToString();
                request_body["discipline_id"] = body["discipline_id"].ToString();

                // Делаем запрос
                ApiClient.Response response = await testClient.PostAsync("disciplines/users/add", request_body, null,
                    new ApiClient.Token("Bearer", user["access_token"].ToString(),
                        new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));
                    
                // Обработка отказа
                if (response.Status != 200)
                {
                    await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                        response.Body["message"].ToString() : "Ошибка выполнения запроса");
                    return;
                }

                // Отправляем уведомление
                await botClient.SendMessage(chatId, $"Пользователь добавлен в дисциплину.");
            });
            app.MapPost("/discipline/user/remove", async (HttpContext context) =>
            {
                // Получение тела, чата айди
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                JObject user = await redisStorage.GetAuthorizeCheckedAsync(chatId);
                if (user["authorized"].ToString() == "false")
                {
                    await botClient.SendMessage(chatId, "Сначала необходимо авторизоваться.");
                    return;
                }

                // Заполняем тело для запроса
                JObject request_body = new JObject();
                request_body["user_id"] = body["user_id"].ToString();
                request_body["discipline_id"] = body["discipline_id"].ToString();

                // Делаем запрос
                ApiClient.Response response = await testClient.PostAsync("disciplines/users/remove", request_body, null,
                    new ApiClient.Token("Bearer", user["access_token"].ToString(),
                        new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));
                    
                // Обработка отказа
                if (response.Status != 200)
                {
                    await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                        response.Body["message"].ToString() : "Ошибка выполнения запроса");
                    return;
                }

                // Отправляем уведомление
                await botClient.SendMessage(chatId, $"Пользователь удалён из дисциплины.");
            });
            app.MapPost("/test/info", async (HttpContext context) =>
            {
                // Получение тела, чата айди
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                JObject user = await redisStorage.GetAuthorizeCheckedAsync(chatId);
                if (user["authorized"].ToString() == "false")
                {
                    await botClient.SendMessage(chatId, "Сначала необходимо авторизоваться.");
                    return;
                }

                // Если передан параметр для присваивания
                if (body.ContainsKey("title") && body.ContainsKey("describtion")) {
                    // Заполняем тело для запроса
                    JObject request_body = new JObject();
                    request_body["title"] = body["title"].ToString();
                    request_body["describtion"] = body["describtion"].ToString();
                    request_body["test_id"] = body["test_id"].ToString();

                    // Делаем запрос
                    ApiClient.Response response = await testClient.PostAsync("tests/text", request_body, null,
                        new ApiClient.Token("Bearer", user["access_token"].ToString(),
                            new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));
                    
                    // Обработка отказа
                    if (response.Status != 200)
                    {
                        await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                            response.Body["message"].ToString() : "Ошибка выполнения запроса");
                        return;
                    }

                    // Отправляем уведомление
                    await botClient.SendMessage(chatId,
                        $"Информация о тесте {response.Body["title"].ToString()} ({body["test_id"].ToString()}) успешно обновлена.");
               
                }
                else
                {
                    // Заполняем query для запроса
                    Dictionary<string, string> request_query = new Dictionary<string, string>();
                    request_query["test_id"] = body["test_id"].ToString();
                    request_query["filter"] = "text max_tries";

                    // Делаем запрос
                    ApiClient.Response response = await testClient.GetAsync("tests/data", request_query,
                        new ApiClient.Token("Bearer", user["access_token"].ToString(),
                            new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));
                    
                    // Обработка отказа
                    if (response.Status != 200)
                    {
                        await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                            response.Body["message"].ToString() : "Ошибка выполнения запроса");
                        return;
                    }

                    // Отправляем уведомление
                    await botClient.SendMessage(chatId, string.Join(Environment.NewLine, new string[] {
                        $"Информация о тесте {request_query["test_id"]}:",
                        $"Название: {response.Body["title"].ToString()}",
                        $"Описание: {response.Body["describtion"].ToString()}",
                        $"Максимальное кол-во попыток: {response.Body["max_tries"].ToString()}"
                    }));
                }
            });
            app.MapPost("/test/status", async (HttpContext context) =>
            {
                // Получение тела, чата айди
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                JObject user = await redisStorage.GetAuthorizeCheckedAsync(chatId);
                if (user["authorized"].ToString() == "false")
                {
                    await botClient.SendMessage(chatId, "Сначала необходимо авторизоваться.");
                    return;
                }

                // Если передан параметр для присваивания
                if (body.ContainsKey("status")) {
                    // Заполняем тело для запроса
                    JObject request_body = new JObject();
                    request_body["status"] = body["status"].ToString();
                    request_body["test_id"] = body["test_id"].ToString();

                    // Делаем запрос
                    ApiClient.Response response = await testClient.PostAsync("tests/status", request_body, null,
                        new ApiClient.Token("Bearer", user["access_token"].ToString(),
                            new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));
                    
                    // Обработка отказа
                    if (response.Status != 200)
                    {
                        await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                            response.Body["message"].ToString() : "Ошибка выполнения запроса");
                        return;
                    }

                    // Отправляем уведомление
                    await botClient.SendMessage(chatId,
                        $"Статус теста {response.Body["title"].ToString()} ({body["test_id"].ToString()}) успешно обновлён.");
                }
                else
                {
                    // Заполняем query для запроса
                    Dictionary<string, string> request_query = new Dictionary<string, string>();
                    request_query["test_id"] = body["test_id"].ToString();
                    request_query["filter"] = "text status";

                    // Делаем запрос
                    ApiClient.Response response = await testClient.GetAsync("tests/data", request_query,
                        new ApiClient.Token("Bearer", user["access_token"].ToString(),
                            new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));
                    
                    // Обработка отказа
                    if (response.Status != 200)
                    {
                        await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                            response.Body["message"].ToString() : "Ошибка выполнения запроса");
                        return;
                    }

                    // Отправляем уведомление
                    await botClient.SendMessage(chatId,
                        $"Статус дисциплины {response.Body["title"].ToString()} ({request_query["test_id"].ToString()}) - {response.Body["status"].ToString()}");
                }
            });
            app.MapPost("/test/tries", async (HttpContext context) =>
            {
                // Получение тела, чата айди
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                JObject user = await redisStorage.GetAuthorizeCheckedAsync(chatId);
                if (user["authorized"].ToString() == "false")
                {
                    await botClient.SendMessage(chatId, "Сначала необходимо авторизоваться.");
                    return;
                }

                if (body.ContainsKey("user_id"))
                {
                    // Заполняем query для запроса
                    Dictionary<string, string> request_query = new Dictionary<string, string>();
                    request_query["test_id"] = body["test_id"].ToString();
                    request_query["user_id"] = body["user_id"].ToString();
                
                    // Делаем запрос
                    ApiClient.Response response = await testClient.GetAsync("tests/tries/user", request_query,
                        new ApiClient.Token("Bearer", user["access_token"].ToString(),
                            new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));
                    
                    // Обработка отказа
                    if (response.Status != 200)
                    {
                        await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                            response.Body["message"].ToString() : "Ошибка выполнения запроса");
                        return;
                    }

                    List<string> info = new List<string>();
                    info.Add($"Список попыток прохождения теста {request_query["test_id"]} пользователем {request_query["user_id"]}:");
                    foreach (JToken item in (JArray)response.Body["tries"])
                    {
                        info.Add($"{item["try_id"].ToString()}: {item["points"].ToString()}/{item["max_points"].ToString()} ({item["score_percent"].ToString()}%)");
                    }
                    // Отправляем уведомление
                    await botClient.SendMessage(chatId, string.Join(Environment.NewLine, info));
                }
                else
                {
                    // Заполняем query для запроса
                    Dictionary<string, string> request_query = new Dictionary<string, string>();
                    request_query["test_id"] = body["test_id"].ToString();
                    request_query["filter"] = "trieslist";
                
                    // Делаем запрос
                    ApiClient.Response response = await testClient.GetAsync("tests/data", request_query,
                        new ApiClient.Token("Bearer", user["access_token"].ToString(),
                            new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));
                    
                    // Обработка отказа
                    if (response.Status != 200)
                    {
                        await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                            response.Body["message"].ToString() : "Ошибка выполнения запроса");
                        return;
                    }
                
                    List<string> info = new List<string>();
                    info.Add("Список попыток прохождения теста:");
                    foreach (JToken item in (JArray)response.Body["tries"])
                    {
                        info.Add($"{item["try_id"].ToString()}: {item["author_fullname"].ToString()} ({item["author_id"].ToString()}) - " +
                            $"{item["points"].ToString()}/{item["max_points"].ToString()} ({item["score_percent"].ToString()}%)");
                    }
                    // Отправляем уведомление
                    await botClient.SendMessage(chatId, string.Join(Environment.NewLine, info));
                }
            });
            app.MapPost("/test/modify", async (HttpContext context) =>
            {
                // Получение тела, чата айди
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                JObject user = await redisStorage.GetAuthorizeCheckedAsync(chatId);
                if (user["authorized"].ToString() == "false")
                {
                    await botClient.SendMessage(chatId, "Сначала необходимо авторизоваться.");
                    return;
                }
                
                // Заполняем query для запроса
                Dictionary<string, string> request_query = new Dictionary<string, string>();
                request_query["test_id"] = body["test_id"].ToString();
                request_query["filter"] = "questions";

                // Делаем запрос
                ApiClient.Response response = await testClient.GetAsync("tests/data", request_query,
                    new ApiClient.Token("Bearer", user["access_token"].ToString(),
                        new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));
                
                // Обработка отказа
                if (response.Status != 200)
                {
                    await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                        response.Body["message"].ToString() : "Ошибка выполнения запроса");
                    return;
                }
            
                List<string> info = new List<string>();
                info.Add($"Список вопросов теста {request_query["test_id"]}:"); int number = 1;
                foreach (JToken item in (JArray)response.Body["questions"])
                {
                    info.Add($"{number}) {item["title"].ToString()} (Вариантов ответа: {item["max_options"].ToString()}).");
                    info.Add($"Айди - {item["question_id"].ToString()}, версия - {item["question_version"].ToString()}.");
                    JArray options = (JArray)item["options"];
                    JArray points = (JArray)item["points"];
                    for (int i = 0; i < options.Count; i++) 
                    {
                        info.Add($"- {options[i].ToString()} (очков: {points[i].ToString()})");
                    }
                    number++;
                }
                // Отправляем уведомление
                await botClient.SendMessage(chatId, string.Join(Environment.NewLine, info));
            });
            app.MapPost("/test/question/add", async (HttpContext context) =>
            {
                // Получение тела, чата айди
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                JObject user = await redisStorage.GetAuthorizeCheckedAsync(chatId);
                if (user["authorized"].ToString() == "false")
                {
                    await botClient.SendMessage(chatId, "Сначала необходимо авторизоваться.");
                    return;
                }
                
                // Заполняем тело для запроса
                JObject request_body = new JObject();
                request_body["question_id"] = body["question_id"].ToString();
                if (body.ContainsKey("question_version")) request_body["question_version"] = body["question_version"].ToString();
                request_body["test_id"] = body["test_id"].ToString();

                // Делаем запрос
                ApiClient.Response response = await testClient.PostAsync("tests/questions/add", request_body, null,
                    new ApiClient.Token("Bearer", user["access_token"].ToString(),
                        new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));
                    
                // Обработка отказа
                if (response.Status != 200)
                {
                    await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                        response.Body["message"].ToString() : "Ошибка выполнения запроса");
                    return;
                }

                // Отправляем уведомление
                await botClient.SendMessage(chatId, $"Вопрос успешно добавлен в тест.");
            });
            app.MapPost("/test/question/remove", async (HttpContext context) =>
            {
                // Получение тела, чата айди
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                JObject user = await redisStorage.GetAuthorizeCheckedAsync(chatId);
                if (user["authorized"].ToString() == "false")
                {
                    await botClient.SendMessage(chatId, "Сначала необходимо авторизоваться.");
                    return;
                }

                // Заполняем тело для запроса
                JObject request_body = new JObject();
                request_body["question_number"] = body["question_number"].ToString();
                request_body["test_id"] = body["test_id"].ToString();

                // Делаем запрос
                ApiClient.Response response = await testClient.PostAsync("tests/questions/remove", request_body, null,
                    new ApiClient.Token("Bearer", user["access_token"].ToString(),
                        new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));
                    
                // Обработка отказа
                if (response.Status != 200)
                {
                    await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                        response.Body["message"].ToString() : "Ошибка выполнения запроса");
                    return;
                }

                // Отправляем уведомление
                await botClient.SendMessage(chatId, $"Вопрос успешно удалён из теста.");
            });
            app.MapPost("/test/questions/sequence", async (HttpContext context) =>
            {
                // Получение тела, чата айди
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                JObject user = await redisStorage.GetAuthorizeCheckedAsync(chatId);
                if (user["authorized"].ToString() == "false")
                {
                    await botClient.SendMessage(chatId, "Сначала необходимо авторизоваться.");
                    return;
                }

                // Заполняем тело для запроса
                JObject request_body = new JObject();
                request_body["questions_signatures"] = body["questions_signatures"];
                request_body["test_id"] = body["test_id"].ToString();

                // Делаем запрос
                ApiClient.Response response = await testClient.PostAsync("tests/questions/update", request_body, null,
                    new ApiClient.Token("Bearer", user["access_token"].ToString(),
                        new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));
                    
                // Обработка отказа
                if (response.Status != 200)
                {
                    await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                        response.Body["message"].ToString() : "Ошибка выполнения запроса");
                    return;
                }

                // Отправляем уведомление
                await botClient.SendMessage(chatId, $"Последовательность вопросов успешно изменена.");
            });
            // Вопросы
            app.MapPost("/questions", async (HttpContext context) =>
            {
                // Получение тела, чата айди
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                JObject user = await redisStorage.GetAuthorizeCheckedAsync(chatId);
                if (user["authorized"].ToString() == "false")
                {
                    await botClient.SendMessage(chatId, "Сначала необходимо авторизоваться.");
                    return;
                }

                // Делаем запрос
                ApiClient.Response response = await testClient.GetAsync("questions/list", null,
                    new ApiClient.Token("Bearer", user["access_token"].ToString(),
                        new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));
                        
                // Обработка отказа
                if (response.Status != 200)
                {
                    await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                        response.Body["message"].ToString() : "Ошибка выполнения запроса");
                    return;
                }

                List<string> info = new List<string>();
                info.Add($"Список всех вопросов:");
                foreach (JToken item in (JArray)response.Body["questions"])
                {
                    info.Add($"Айди - {item["question_id"].ToString()}, версия - {item["question_version"].ToString()} (Вариантов ответа: {item["max_options"].ToString()}).");
                    JArray options = (JArray)item["options"];
                    for (int i = 0; i < options.Count; i++) 
                    {
                        info.Add($"- {options[i].ToString()}");
                    }
                }
                // Отправляем уведомление
                await botClient.SendMessage(chatId, string.Join(Environment.NewLine, info));
            });
            app.MapPost("/question/create", async (HttpContext context) =>
            {
                // Получение тела, чата айди
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                JObject user = await redisStorage.GetAuthorizeCheckedAsync(chatId);
                if (user["authorized"].ToString() == "false")
                {
                    await botClient.SendMessage(chatId, "Сначала необходимо авторизоваться.");
                    return;
                }

                // Заполняем тело для запроса
                JObject request_body = new JObject();
                request_body["title"] = body["title"].ToString();
                request_body["options"] = body["options"];
                request_body["points"] = body["points"];
                request_body["max_options"] = body["max_options"].ToString();

                // Делаем запрос
                ApiClient.Response response = await testClient.PostAsync("questions/create", request_body, null,
                    new ApiClient.Token("Bearer", user["access_token"].ToString(),
                        new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));
                    
                // Обработка отказа
                if (response.Status != 200)
                {
                    await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                        response.Body["message"].ToString() : "Ошибка выполнения запроса");
                    return;
                }

                // Отправляем уведомление
                await botClient.SendMessage(chatId, $"Вопрос {response.Body["question_id"].ToString()} успешно создан.");
            });
            app.MapPost("/question/update", async (HttpContext context) =>
            {
                // Получение тела, чата айди
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                JObject user = await redisStorage.GetAuthorizeCheckedAsync(chatId);
                if (user["authorized"].ToString() == "false")
                {
                    await botClient.SendMessage(chatId, "Сначала необходимо авторизоваться.");
                    return;
                }
                
                JObject request_body = new JObject();
                request_body["question_id"] = body["question_id"].ToString();
                request_body["title"] = body["title"].ToString();
                request_body["options"] = body["options"];
                request_body["points"] = body["points"];
                request_body["max_options"] = body["max_options"].ToString();

                // Делаем запрос
                ApiClient.Response response = await testClient.PostAsync("questions/update", request_body, null,
                    new ApiClient.Token("Bearer", user["access_token"].ToString(),
                        new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));
                    
                // Обработка отказа
                if (response.Status != 200)
                {
                    await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                        response.Body["message"].ToString() : "Ошибка выполнения запроса");
                    return;
                }

                // Отправляем уведомление
                await botClient.SendMessage(chatId, $"Вопрос успешно обновлён.");
            });
            app.MapPost("/question/status", async (HttpContext context) =>
            {
                // Получение тела, чата айди
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                JObject user = await redisStorage.GetAuthorizeCheckedAsync(chatId);
                if (user["authorized"].ToString() == "false")
                {
                    await botClient.SendMessage(chatId, "Сначала необходимо авторизоваться.");
                    return;
                }

                // Если передан параметр для присваивания
                if (body.ContainsKey("status")) {
                    // Заполняем тело для запроса
                    JObject request_body = new JObject();
                    request_body["status"] = body["status"].ToString();
                    request_body["question_id"] = body["question_id"].ToString();
                    request_body["question_version"] = body["question_version"].ToString();

                    // Делаем запрос
                    ApiClient.Response response = await testClient.PostAsync("questions/status", request_body, null,
                        new ApiClient.Token("Bearer", user["access_token"].ToString(),
                            new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));
                    
                    // Обработка отказа
                    if (response.Status != 200)
                    {
                        await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                            response.Body["message"].ToString() : "Ошибка выполнения запроса");
                        return;
                    }

                    // Отправляем уведомление
                    await botClient.SendMessage(chatId,
                        $"Статус вопроса успешно обновлён.");
                }
                else
                {
                    // Заполняем query для запроса
                    Dictionary<string, string> request_query = new Dictionary<string, string>();
                    request_query["question_id"] = body["question_id"].ToString();
                    if (body.ContainsKey("question_version")) request_query["question_version"] = body["question_version"].ToString();
                    request_query["filter"] = "status";
                    
                    // Делаем запрос
                    ApiClient.Response response = await testClient.GetAsync("questions/data", request_query,
                        new ApiClient.Token("Bearer", user["access_token"].ToString(),
                            new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));
                    
                    // Обработка отказа
                    if (response.Status != 200)
                    {
                        await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                            response.Body["message"].ToString() : "Ошибка выполнения запроса");
                        return;
                    }

                    // Отправляем уведомление
                    await botClient.SendMessage(chatId,
                        $"Статус вопроса {request_query["question_id"]}.{response.Body["version"].ToString()} - {response.Body["status"].ToString()}");
                }
            });
            app.MapPost("/question/info", async (HttpContext context) =>
            {
                // Получение тела, чата айди
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                JObject user = await redisStorage.GetAuthorizeCheckedAsync(chatId);
                if (user["authorized"].ToString() == "false")
                {
                    await botClient.SendMessage(chatId, "Сначала необходимо авторизоваться.");
                    return;
                }

                // Заполняем query для запроса
                Dictionary<string, string> request_query = new Dictionary<string, string>();
                request_query["question_id"] = body["question_id"].ToString();
                if (body.ContainsKey("question_version")) request_query["question_version"] = body["question_version"].ToString();
                request_query["filter"] = "all";
                
                // Делаем запрос
                ApiClient.Response response = await testClient.GetAsync("questions/data", request_query,
                    new ApiClient.Token("Bearer", user["access_token"].ToString(),
                        new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));
                
                // Обработка отказа
                if (response.Status != 200)
                {
                    await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                        response.Body["message"].ToString() : "Ошибка выполнения запроса");
                    return;
                }

                List<string> info = new List<string>();
                info.Add($"Информация о вопросе {request_query["question_id"]}.{response.Body["version"].ToString()}:");
                info.Add($"Вариантов ответа: {response.Body["max_options"].ToString()}.");
                info.Add("Опции ответов:");
                JArray options = (JArray)response.Body["options"];
                JArray points = (JArray)response.Body["points"];
                for (int i = 0; i < options.Count; i++) 
                {
                    info.Add($"- {options[i].ToString()} (очков: {points[i].ToString()})");
                }
                // Отправляем уведомление
                await botClient.SendMessage(chatId, string.Join(Environment.NewLine, info));
            });
            // Попытки и ответы
            app.MapPost("/try/start", async (HttpContext context) =>
            {
                // Получение тела, чата айди
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                JObject user = await redisStorage.GetAuthorizeCheckedAsync(chatId);
                if (user["authorized"].ToString() == "false")
                {
                    await botClient.SendMessage(chatId, "Сначала необходимо авторизоваться.");
                    return;
                }

                // Заполняем тело для запроса
                JObject request_body = new JObject();
                request_body["test_id"] = body["test_id"].ToString();
                
                // Делаем запрос
                ApiClient.Response response = await testClient.PostAsync("tries/start", request_body, null,
                    new ApiClient.Token("Bearer", user["access_token"].ToString(),
                        new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));
                    
                // Обработка отказа
                if (response.Status != 200)
                {
                    await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                        response.Body["message"].ToString() : "Ошибка выполнения запроса");
                    return;
                }

                // Формирует и отправляет ответ
                List<string> info = new List<string>();
                info.Add($"Попытка успешно начата!");
                info.Add($"Айди попытки: {response.Body["try_id"].ToString()}.");
                info.Add($"Список вопросов:"); int number = 1;
                foreach (JToken item in (JArray)response.Body["questions"])
                {
                    info.Add($"Вопрос {number} (Вариантов ответа: {item["max_options"].ToString()}).");
                    info.Add(item["title"].ToString());
                    JArray options = (JArray)item["options"];
                    for (int i = 0; i < options.Count; i++) 
                    {
                        info.Add($"- {options[i].ToString()}");
                    }
                    info.Add($"Айди ответа: {item["answer_id"].ToString()}.");
                    number++;
                }
                await botClient.SendMessage(chatId, string.Join(Environment.NewLine, info));
            });
            app.MapPost("/try/stop", async (HttpContext context) =>
            {
                // Получение тела, чата айди
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                JObject user = await redisStorage.GetAuthorizeCheckedAsync(chatId);
                if (user["authorized"].ToString() == "false")
                {
                    await botClient.SendMessage(chatId, "Сначала необходимо авторизоваться.");
                    return;
                }

                // Заполняем тело для запроса
                JObject request_body = new JObject();
                request_body["try_id"] = body["try_id"].ToString();
                
                // Делаем запрос
                ApiClient.Response response = await testClient.PostAsync("tries/stop", request_body, null,
                    new ApiClient.Token("Bearer", user["access_token"].ToString(),
                        new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));
                    
                // Обработка отказа
                if (response.Status != 200)
                {
                    await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                        response.Body["message"].ToString() : "Ошибка выполнения запроса");
                    return;
                }

                await botClient.SendMessage(chatId, "Попытка успешно окончена.");
            });
            app.MapPost("/answer/change", async (HttpContext context) =>
            {
                // Получение тела, чата айди
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                JObject user = await redisStorage.GetAuthorizeCheckedAsync(chatId);
                if (user["authorized"].ToString() == "false")
                {
                    await botClient.SendMessage(chatId, "Сначала необходимо авторизоваться.");
                    return;
                }

                // Заполняем тело для запроса
                JObject request_body = new JObject();
                request_body["answer_id"] = body["answer_id"].ToString();
                request_body["options"] = body["options"];

                // Делаем запрос
                ApiClient.Response response = await testClient.PostAsync("tries/answer/change", request_body, null,
                    new ApiClient.Token("Bearer", user["access_token"].ToString(),
                        new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));

                // Обработка отказа
                if (response.Status != 200)
                {
                    await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                        response.Body["message"].ToString() : "Ошибка выполнения запроса");
                    return;
                }

                int count = ((JArray)body["options"]).Count;
                await botClient.SendMessage(chatId, $"Ответ успешно {(count == 0 ? "обнулён" : "изменён")}.");
            });
            app.MapPost("/test/view", async (HttpContext context) =>
            {
                // Получение тела, чата айди
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                JObject user = await redisStorage.GetAuthorizeCheckedAsync(chatId);
                if (user["authorized"].ToString() == "false")
                {
                    await botClient.SendMessage(chatId, "Сначала необходимо авторизоваться.");
                    return;
                }
                
                // Заполняем query для запроса
                Dictionary<string, string> request_query = new Dictionary<string, string>();
                request_query["try_id"] = body["try_id"].ToString();

                // Делаем запрос
                ApiClient.Response response = await testClient.GetAsync("tries/view", request_query,
                    new ApiClient.Token("Bearer", user["access_token"].ToString(),
                        new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));
                
                // Обработка отказа
                if (response.Status != 200)
                {
                    await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                        response.Body["message"].ToString() : "Ошибка выполнения запроса");
                    return;
                }
            
                List<string> info = new List<string>();
                info.Add($"Информация о попытке {request_query["try_id"]}:");
                info.Add($"Автор: {response.Body["author_id"].ToString()}.");
                info.Add($"Статус: {response.Body["status"].ToString()}.");
                int number = 1;
                foreach (JToken item in (JArray)response.Body["answers"])
                {
                    info.Add($"{number}) {item["question_title"].ToString()} (Баллов: {item["points"].ToString()}/{item["max_points"].ToString()}).");
                    JArray options = (JArray)item["question_options"];
                    JArray selected_options = (JArray)item["selected_options"];
                    for (int i = 0; i < options.Count; i++) 
                    {
                        info.Add($"- {options[i].ToString()}{(selected_options.Contains(number) ? " (Выбрано)" : "")}");
                    }
                    number++;
                }
                // Отправляем уведомление
                await botClient.SendMessage(chatId, string.Join(Environment.NewLine, info));
            });
            // Новости
            app.MapPost("/news", async (HttpContext context) =>
            {
                // Получение тела, чата айди
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                JObject user = await redisStorage.GetAuthorizeCheckedAsync(chatId);
                if (user["authorized"].ToString() == "false")
                {
                    await botClient.SendMessage(chatId, "Сначала необходимо авторизоваться.");
                    return;
                }

                // Заполняем query для запроса
                Dictionary<string, string> request_query = new Dictionary<string, string>();
                request_query["user_id"] = user["id"].ToString();

                // Делаем запрос
                ApiClient.Response response = await testClient.GetAsync("news/list/recipient", request_query,
                    new ApiClient.Token("Bearer", user["access_token"].ToString(),
                        new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));
                        
                // Обработка отказа
                if (response.Status != 200)
                {
                    await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                        response.Body["message"].ToString() : "Ошибка выполнения запроса");
                    return;
                }

                // Выводим в зависимости от содержимого
                List<string> info = new List<string>();
                info.Add("Список новостей пользователя:");
                foreach (JToken item in (JArray)response.Body["news"])
                {
                    info.Add($"{item["news_id"].ToString()}: от {item["sender_fullname"].ToString()}. {item["text"].ToString()}");
                }
                
                // Выводит
                await botClient.SendMessage(chatId, string.Join(Environment.NewLine, info));
            });
            app.MapPost("/news/status", async (HttpContext context) =>
            {
                // Получение тела, чата айди
                JObject body = await GetDataFromBody(context.Request.Body);
                if (!long.TryParse(body["chat_id"].ToString(), out long chatId)) return;

                JObject user = await redisStorage.GetAuthorizeCheckedAsync(chatId);
                if (user["authorized"].ToString() == "false")
                {
                    await botClient.SendMessage(chatId, "Сначала необходимо авторизоваться.");
                    return;
                }

                // Если передан параметр для присваивания
                if (body.ContainsKey("status")) {
                    // Заполняем тело для запроса
                    JObject request_body = new JObject();
                    request_body["status"] = body["status"].ToString();
                    request_body["news_id"] = body["news_id"].ToString();

                    // Делаем запрос
                    ApiClient.Response response = await testClient.PostAsync("news/status", request_body, null,
                        new ApiClient.Token("Bearer", user["access_token"].ToString(),
                            new ApiClient.Token.RefreshData(user["refresh_token"].ToString(), redisStorage, chatId, true)));
                    
                    // Обработка отказа
                    if (response.Status != 200)
                    {
                        await botClient.SendMessage(chatId, response.Body.ContainsKey("message") ?
                            response.Body["message"].ToString() : "Ошибка выполнения запроса");
                        return;
                    }

                    // Отправляем уведомление
                    await botClient.SendMessage(chatId, "Статус новости успешно обновлён.");
                }
                else
                {
                    // Отправляем уведомление
                    await botClient.SendMessage(chatId, "Нельзя получить статус определённой новости отдельно.");
                }
            });

            // Запускаем сервер для обработки команд
            Console.WriteLine($"Запускаем сервер контроллера на {url}.");
            app.Run(url);
            // Сообщаем о прекрашении работы сервера
            Console.WriteLine("Сервер контроллера остановлен.");
        }

        public static string GenerateRandomToken(int length = 10)
        {
            const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
            var token = new char[length];

            Random random = new Random();
            for (int i = 0; i < length; i++)
                token[i] = chars[random.Next(chars.Length)];

            return new string(token);
        }
    }
}