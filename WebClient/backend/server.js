// Импорт библиотеки dotenv для работы с переменными окружения из .env-файла
const dotenv = require("dotenv");
// Импорт фреймворка Express для создания веб‑сервера
const express = require("express");
// Импорт ассета для чтения файлов и замены содержимого
const fs = require("fs");
// Импорт парсера для кук
const cookieParser = require("cookie-parser");
// Импорт middleware cors для обработки кросс‑доменных запросов
const cors = require("cors");
// Импорт клиента Redis для взаимодействия с сервером Redis
const redis = require("redis");
// Импорт библиотеки axios для выполнения HTTP‑запросов
const axios = require("axios").create({
    validateStatus: () => true
});

/* Загрузка переменных окружения из указанного .env‑файла */
// Путь жёстко задан, так как я иначе не смог чот
dotenv.config({ path: "C:/Users/Admin/Desktop/WetvteGitClone/alphavet_latter/WebClient/config/client_config.env" });

/* Создание экземпляра приложения Express*/
// Приложение
const app = express();

/* Middleware — промежуточное ПО для обработки запросови*/

// Разрешает кросс‑доменные запросы (CORS) без ограничений (небезопасно для продакшена)
app.use(cors());
// Позволяет парсить куки
app.use(cookieParser());
// Позволяет приложению обрабатывать JSON в теле HTTP‑запросов
app.use(express.json());

/* Подключение к серверу Redis */
// Создаём клиент Redis, используя URL из переменных окружения (REDIS_URI)
const redisClient = redis.createClient({
    url: process.env.REDIS_URI
});

/* Обработчики событий для клиента Redis */

// Событие "connect": срабатывает при установлении соединения с Redis
redisClient.on("connect", () => {
    console.log("Подключение к Redis");
});
// Событие "ready": Redis готов принимать команды
redisClient.on("ready", () => {
    console.log("Redis готов к работе");
});
// Событие "error": срабатывает при ошибке соединения/операции с Redis
redisClient.on("error", (err) => {
    console.error("Redis Error:", {
        message: err.message,           // текстовое описание ошибки
        code: err.code,               // код ошибки (например, "ECONNREFUSED")
        command: err.command,            // команда Redis, вызвавшая ошибку
        stack: err.stack                 // стек вызовов для отладки
    });
});
// Событие "end": соединение с Redis закрыто
redisClient.on("end", () => {
    console.log("Соединение с Redis закрыто");
});

/* Маршрутизация — определение обработчиков для разных URL */

app.get("/delc", (req, res) => {
    delete_session(req, res);
    res.redirect("/");
});

// Маршрут для главной страницы ("/")
app.get("/", async (req, res) => {
    console.log("Пользователь зашёл на страницу /");
    // Получаем токен сессии из cookie запроса
    const session_token = req.cookies["session_token"];
    // Переменная данных о статусе и другом 
    let user = await get_redis_user(session_token);
    if (!user && session_token) res.clearCookie("session_token");

    // Если пользователь авторизирован
    if (user && user["status"] === "Authorized") {
        console.log("Отправлен запрос на обновление данных пользователя " + user["nickname"]);
        let update_response = await get_sub_request(user["access_token"]);

        // Если запрос провален из-за истечения токена, обновляем его и пытаемся ещё раз отправить запрос
        if (update_response.status == 401) {
            let success;
            [success, user] = await refresh_user(user, session_token);
            if (success) update_response = await get_sub_request(user["access_token"]);
            else res.redirect("/");
        }

        // Если данные успешно проверены/обновлены/получены и т.д.
        if (update_response.status == 200) {
            console.log("Одобрен запрос на обновление данных пользователя " + user["nickname"]);
            // Обновляет запись пользователя
            user = await change_redis_session(session_token, {
                id: update_response.data["id"],
                nickname: update_response.data["nickname"],
                email: update_response.data["email"],
                roles: update_response.data["roles"]
            })
            // Редирект
            res.redirect(`cabinet?user_id=${user["id"]}`);
            return;
        }
        // Если токены истекли/ошибка обновления
        else {
            console.log("Провален запрос на обновление данных пользователя " + user["nickname"]);
            // Очищаем куки и данные сессии, перезагружаем страницу
            await delete_session(req, res);
            return res.redirect("/");
        }
    }
    // Иначе выводит форму для входа
    else {
        res.status(200).send(generate_entry_page());
    }
});

// Маршрут для входа ("/login") post обрабатывает любой вид входа, а get только атоматический
app.get("/login", async (req, res) => {
    console.log("Попытка автовхода.");
    // P.S.2 Теперь понял: пользователь после авторизации через OAuth возвращается на /login веб клиента и,
    // так как ему нужно автоматом войти, и он сохранился анонимным, обрабатываем автовход (ещё нужно для default типа логирования)

    // Получаем параметр "type" из query‑строки URL (например, ?type=github)
    const { type } = req.query;
    let { target } = req.query;
    console.log("Пользователь обратился к get странице /login");
    // Для процессов авторизации есть пост запрос 
    if (type && type.length > 0) {
        return res.redirect("/");
    }
    // Получаем токен сессии из cookie запроса
    let session_token = req.cookies["session_token"];
    console.log("Полученные куки сессии: " + session_token);
    // Ищем пользователя по куки
    let user = await get_redis_user(session_token);
    if (!user || user["status"] === "Unknown") {
        // Авто-входу не быть
        await delete_session(req, res);
        return res.redirect("/");
    }

    console.log("Начальная цель автовхода -", target);
    // Если цель - отмена (Пользователь нажал "нет" в подтверждении)
    if (target === "cancel") {
        // Удаляем и уходим ^_^
        console.log("Подтверждение входа отклонено.");
        await delete_redis_session
        return res.redirect("/");
    }
    // Если цели нет, ею выбирается Проверка
    if (!target || (target != "confirm" && target != "check_and_confirm")) {
        target = "check";
    }
    console.log("После обработки -", target);

    // Если цель - проверка
    if (target == "check" || target == "check_and_confirm") {
        // Делаем запрос о проверке, есть ли пользователь в базе данных
        const check_response = user["login_token"] ? (await post_service_check_auth_request(type, user["login_token"])) : undefined;
        // Проверяет соответствие токена
        if (!check_response || user["login_token"] != check_response.data["login_token"]) {
            console.error("Ошибка проверки входа (токены не соответствуют или отсутствуют)");
            // Пользователя не знаем, -куки
            await delete_session(req, res);
            return res.redirect("/");
        }
        // Если пользователя нет в базе данных - нет входу
        if (check_response.status != 200) {
            console.log("Вход отклонён.", check_response.data["message"]);
            await delete_session(req, res);
            return res.redirect("/");
        }
        console.log("Вход одобрен и ждёт подтверждения для имени " + check_response.data["nickname"]);
        // Если пользователь проверяет и подтверждает, сразу переходит на подтверждение
        if (target == "check_and_confirm") {
            return res.redirect("/login?target=confirm");
        }
        // Иначе отправляем ему страницу входа с настройками для подтверждения
        else {
            // Отправляем файл страницы с подтверждением
            res.status(200).send(generate_entry_page("Подтверждение входа", check_response.data["nickname"]));
            return;
        }
    }
    // Если цель - подтверждение
    if (target == "confirm" || target == "check_and_confirm") {
        // Отправляем запрос на подтверждение входа
        const confirm_response = await post_service_confirm_auth_request(type, user["login_token"]);
        // Проверяем соответствие токена входа
        if (user["login_token"] != confirm_response.data["login_token"]) {
            console.error("Ошибка подтверждения входа (токены не соответствуют)");
            await delete_session(req, res);
            return res.redirect("/");
        }
        // Проверяем статус подтверждения
        if (confirm_response.status != 200) {
            console.log("Вход отклонён.", confirm_response.data["message"]);
            await delete_session(req, res);
            return res.redirect("/");
        }
        // Проверяем наличие необходимых токенов
        if (!confirm_response.data["access_token"] || !confirm_response.data["refresh_token"]) {
            console.error("Э, а где токены, йоу? Ты вернул "
                + confirm_response.data["access_token"] + " ИиИ " + confirm_response.data["refresh_token"]);
            await delete_session(req, res);
            return res.redirect("/");
        }
        // Обновляем сессию в Redis — пользователь авторизован
        user = await create_redis_session(session_token, {
            status: "Authorized",
            id: confirm_response.data["id"],
            email: confirm_response.data["email"],
            name: confirm_response.data["nickname"],
            roles: confirm_response.data["roles"],
            access_token: confirm_response.data["access_token"],
            refresh_token: confirm_response.data["refresh_token"]
        });
        return res.redirect("/");
    }
});
app.post("/login", async (req, res) => {
    // Получаем параметр "type" из query‑строки URL (например, ?type=github)
    const { type } = req.query;
    console.log("Пользователь обратился к post странице /login?type=" + type);
    // Если тип входа некорректен, возвращаем ошибку (Автовход для анонима обрабатывается в гет запросе выше)
    if (type !== "default" && type !== "code" && type !== "github" && type !== "yandex") {
        console.log("Неверный тип входа.")
        return res.status(400).send(JSON.stringify({
            message: "Неверный тип входа."
        }));
    }

    // Получаем токен сессии из cookie запроса
    let session_token = req.cookies["session_token"];
    console.log("Полуенные куки сессии:" + session_token);
    // Ищем пользователя по куки
    let user = await get_redis_user(session_token);
    if (!user && session_token) res.clearCookie("session_token");

    // Если пользователь авторизован, возвращаем ошибку
    if (user && user["status"] === "Authorized") {
        console.log("Пользователь уже авторизован.")
        return res.status(409).send(JSON.stringify({
            message: "Пользователь уже авторизован."
        }));
    }
    // Если пользователь неизвестный
    if (!user || user["status"] === "Unknown") {
        console.log("Проходит запись неизвестного пользователя в анонимы.")
        // Генерируем случайный токен сессии (для идентификации пользователя)
        session_token = generate_random_token();
        console.log("Session token created", session_token);
        user = await create_redis_session(session_token, {
            status: "Anonymous",
            nickname: "none"
        });
    }
    console.log("Проходит авторизация анонимного пользователя.")
    // Остались только Анонимы с конкретным type входа
    // Генерируем случайный токен входа (для процесса авторизации)
    const login_token = generate_random_token();
    console.log("Login token created", login_token);
    // Сохраняем токен входа
    user = await change_redis_session(session_token, {
        login_token: login_token
    });
    // Отправляем запрос на авторизацию соответствующим type образом
    let auth_response;
    if (type == "default") {
        const { email, password } = req.body;
        auth_response = await post_default_auth_request(email, password, login_token);
        // Если успешно, сохраняем в сессию
        if (auth_response.status == 200) {
            console.log("Авторизация обычная успешна.");
            res.cookie("session_token", session_token);
        }
        // Возвращаем результат авторизации кодом, чтобы челик знал, что он не так сделал (или так)
        res.status(auth_response.status).send(JSON.stringify({
            message: auth_response.data["message"]
        }));
    }
    // Отправляем запрос на авторизацию code
    else if (type == "code") {
        const { state, code } = req.body;
        auth_response = await post_code_auth_verify_request(state, code, login_token);
        // Если успешно, сохраняем в сессию
        if (auth_response.status == 200) {
            console.log("Авторизация обычная успешна.");
            res.cookie("session_token", session_token);
        }
        // Возвращаем результат авторизации кодом, чтобы челик знал, что он не так сделал (или так)
        res.status(auth_response.status).send(JSON.stringify({
            message: auth_response.data["message"]
        }));
    }
    else { // авторизация сервисом
        auth_response = await post_service_auth_request(type, session_token, login_token);
        // Обработка ошибки
        if (auth_response.status != 200) {
            return res.status(auth_response.status).send(JSON.stringify({
                message: auth_response.data["message"]
            }));
        }
        // Если нет ошибки, редирект на полученный oauth (ВАЖНО !!! Нужно передать юрл обратно юзеру на страницу, и там уже открывать)
        console.log("Полученный url для редикта на oauth: " + auth_response.data["oauth_redirect_url"] + ".");
        if (auth_response.data["oauth_redirect_url"] && auth_response.data["oauth_redirect_url"].length > 0) {
            console.log("Передаём пользователю.")
            // Если успешно, сохранение кук будет через Auth module
            res.status(200).send(JSON.stringify({
                oauth_redirect_url: auth_response.data["oauth_redirect_url"]
            }));
        }
    }
});

// Для запроса отправки на аккаунт кода для авторизации/регистрации (пока только авторизации, т.к. регистрация вынесена отдельно)
app.post("/init/authorization", async (req, res) => {
    console.log("Проверяем доступность авторизации кодом.");
    const { email } = req.body;
    const authorization_response = await post_send_authorization_code_request(email);
    console.log("Результат: " + authorization_response.status);
    // Обработка успеха
    if (authorization_response.status == 200) {
        return res.status(200).send(JSON.stringify({
            message: authorization_response.data["message"],
            state: authorization_response.data["state"]
        }));
    }
    else {
        return res.status(authorization_response.status).send(JSON.stringify({
            message: authorization_response.data["message"],
        }));
    }
});
app.post("/init/registration", async (req, res) => {
    console.log("Проверяем доступность регистрации.");
    const { email, nickname, first_name, last_name, patronimyc, password, repeat_password } = req.body;
    const registration_response =
        await post_send_registration_code_request(email, nickname, first_name, last_name, patronimyc, password, repeat_password);
    console.log("Результат: " + registration_response.status);
    // Обработка успеха
    if (registration_response.status == 200) {
        return res.status(200).send(JSON.stringify({
            message: registration_response.data["message"],
            state: registration_response.data["state"]
        }));
    }
    else {
        return res.status(registration_response.status).send(JSON.stringify({
            message: registration_response.data["message"],
        }));
    }
});

// Маршрут для регистрации ("/verify/registration")
app.post("/verify/registration", async (req, res) => {
    console.log("Пользователь обратился к странице /verify/registration");
    // Получаем state и код регистрации
    const { code, state } = req.body;
    if (!state) return res.status(400).send(JSON.stringify({
        message: "Отсутствует параметр состояния."
    }));

    console.log("Проверяем код подтверждения.");
    const verify_response = await post_registration_verify_request(code, state);
    console.log("Результат: " + verify_response.status);
    return res.status(verify_response.status).send(JSON.stringify({
        message: verify_response.data["message"]
    }));
});

// Открытие главной страницы
app.get("/primary", async (req, res) => {
    // Можно зайти незалогиненным, но тогда ничего нельзя будет сделать, т.к. всё перекинет на "/"
    res.status(200).send(generate_site_page("primary", "Главная", {}));
});
// Открытие личного кабинета
app.get("/cabinet", async (req, res) => {
    let { user_id } = req.query;
    console.log("Пользователь обратился к /cabinet?user_id=" + user_id);

    // Получаем токен сессии из cookie запроса
    let session_token = req.cookies["session_token"];
    console.log("Полуенные куки сессии:" + session_token);
    // Ищем пользователя по куки
    let user = await get_redis_user(session_token);
    if (!user || user["status"] !== "Authorized") {
        // Сначала авторизоваться
        if (!user) res.clearCookie("session_token");
        return res.redirect("/");
    }

    let title;
    // Если не был передан айди, переходит в свой кабинет
    if (!user_id) {
        user_id = user["id"];
        title = "Личный кабинет";
    }
    else {
        title = "Профиль пользователя";
    }

    // Отправляем файл страницы ЛК
    res.status(200).send(generate_site_page("cabinet", title, { user_id: user_id }));
});
// Просмотр дисциплины
app.get("/discipline", async (req, res) => {
    const { discipline_id } = req.query;
    console.log("Пользователь обратился к /discipline?discipline_id=" + discipline_id);
    // Если не указана - на главную
    if (!discipline_id) return res.redirect("/primary");

    // Получаем токен сессии из cookie запроса
    let session_token = req.cookies["session_token"];
    console.log("Полуенные куки сессии:" + session_token);
    // Ищем пользователя по куки
    let user = await get_redis_user(session_token);
    if (!user || user["status"] !== "Authorized") {
        // Сначала авторизоваться
        if (!user) res.clearCookie("session_token");
        return res.redirect("/");
    }

    // Отправляем файл страницы 
    res.status(200).send(generate_site_page("discipline", "Дисциплина", { discipline_id: discipline_id }));
});
// Просмотр теста
app.get("/test", async (req, res) => {
    const { test_id, try_id } = req.query;
    console.log("Пользователь обратился к /test?test_id=" + test_id + "&try_id=" + try_id);

    if (!test_id) return res.redirect("/primary");

    // Получаем токен сессии из cookie запроса
    let session_token = req.cookies["session_token"];
    console.log("Полуенные куки сессии:" + session_token);
    // Ищем пользователя по куки
    let user = await get_redis_user(session_token);
    if (!user || user["status"] !== "Authorized") {
        // Сначала авторизоваться
        if (!user) res.clearCookie("session_token");
        return res.redirect("/");
    }

    // Отправляем файл страницы 
    res.status(200).send(generate_site_page("test", try_id ? "Попытка" : "Тест", { ...req.query }));
});

// Маршрут для выхода из системы ("/logout")
app.get("/logout", async (req, res) => {
    // Получаем токен сессии из cookie
    const session_token = req.cookies["session_token"];

    // Переменная данных о статусе и другом пользователя
    const user = await get_redis_user(session_token);

    // Если неизвестен
    if (!user || user["status"] !== "Authorized") {
        console.error("Куда лезешь, если не авторизирован даже");
        // Перенаправляем на главную страницу
        if (!user) res.clearCookie("session_token");
        return res.redirect("/");
    }

    // Получение параметра all из url
    const { all } = req.query;
    // Выход со всех устройств
    if (all === "true") {
        await post_logout_request(user["access_token"])
    }

    // Удаляем запись по ключу token
    await redisClient.del(session_token);
    // Очищаем cookie на клиенте
    res.clearCookie("session_token");
    // Перенаправляем на главную страницу
    return res.redirect("/");
});

// Получение данных с redis (нужно для айди, в основном)
app.get("/sub", async (req, res) => {
    const session_token = req.cookies["session_token"];
    console.log("Потытка отдать данные по токену", session_token);
    if (!session_token) return res.status(401);
    const user = await get_redis_user(session_token);
    if (!user) return res.redirect("/");
    res.status(200).send(JSON.stringify(user));
});

// Получение данных с основного модуля
app.get("/data/read", async (req, res) => {
    console.log("Потытка получить данные с основного модуля.");
    // Проверяем аторизацию автора запроса
    const session_token = req.cookies["session_token"];
    if (!session_token) {
        console.log("Но нет куки");
        return res.redirect("/");
    }
    let user = await get_redis_user(session_token);
    if (!user) {
        console.log("Но пользователь неизвестен");
        return res.redirect("/");
    }
    // Получаем данные для запроса
    const { source, ...params } = req.query;
    console.log("GET: source -", source, "params -", params);
    if (!source) {
        console.log("Но не указан source");
        return res.status(400);
    }

    // Делаем запрос
    console.log("Делаем запрос:");
    let response = await get_testmodule_request(source, params, user["access_token"]);
    console.log("Результат -", response.status);
    // Если провален из-за токена, пытаемся поменять токены и отправляем заново
    if (response.status == 401) {
        let success;
        [success, user] = await refresh_user(user, session_token);
        console.log("Результат обновления токена -", success);
        if (success) {
            console.log("Делаем запрос повторно:");
            response = await get_testmodule_request(source, params, user["access_token"]);
            console.log("Результат -", response.status);
        }
        else res.status(response.status);
    }

    // Отправляем статус и данные ответа
    res.status(response.status).json({ ...response.data });
});
// Запись данных в основной модуль
app.post("/data/write", async (req, res) => {
    console.log("Потытка отправить данные на основной модуль.");
    // Проверяем аторизацию автора запроса
    const session_token = req.cookies["session_token"];
    if (!session_token) {
        console.log("Но нет куки");
        return res.redirect("/");
    }
    let user = await get_redis_user(session_token);
    if (!user) {
        console.log("Но пользователь неизвестен");
        return res.redirect("/");
    }
    // Получаем данные для запроса
    const { source, ...params } = req.query;
    console.log("POST: source -", source, "params -", params, "body -", req.body);
    if (!source) {
        console.log("Но не указан source");
        return res.status(400);
    }

    // Делаем запрос
    console.log("Делаем запрос:");
    let response = await post_testmodule_request(source, params, req.body, user["access_token"]);
    console.log("Результат -", response.status);
    // Если провален из-за токена, пытаемся поменять токены и отправляем заново
    if (response.status == 401) {
        let success;
        [success, user] = await refresh_user(user, session_token);
        console.log("Результат обновления токена -", success);
        if (success) {
            console.log("Делаем запрос повторно:");
            response = await post_testmodule_request(source, params, req.body, user["access_token"]);
            console.log("Результат -", response.status);
        }
        else res.status(response.status);
    }

    // Отправляем статус и данные ответа
    res.status(response.status).json({ ...response.data });
});

// Получает токен из заголовка авторизации
function get_token_from_header(header) // header authorization
{
    if (!header || header === "") {
        console.log("Токен не получен из-за некорректности заголовка (1).");
        return;
    }
    const parts = header.split(" ");
    if (parts.length !== 2 || parts[1].length === 0) {
        console.log("Токен не получен из-за некорректности заголовка (2).");
        return;
    }
    console.log("Получен токен регистрации: " + parts[1] + " из заголовка " + header);
    return parts[1];
}

// Обновляет токены для redis пользователя, сохранённого под токеном
async function refresh_user(user, session_token) {
    const refresh_request = await post_refresh_request(user["refresh_token"]);
    // Если обновить не получилось, удаляем запись и перезаходим
    if (refresh_request.status != 200) {
        await delete_redis_session(session_token);
        return [false, user];
    }
    // Если обновление токенов успешно, сохраняем новые и переделываем запрос
    user = await change_redis_session(session_token, {
        access_token: refresh_request.data["access_token"],
        refresh_token: refresh_request.data["refresh_token"]
    })
    return [true, user];
}

/* Создание токенов сессии и входа */
// Сделать рандомный токен (строку)
function generate_random_token() {
    return Math.random().toString(36).substring(2);
}

// Удаляет сессию полностью для запроса
async function delete_session(req, res) {
    const session_token = req.cookies["session_token"];
    console.log("Удаляем сессию " + session_token);
    if (!session_token) {
        console.log("Сессия отсутствует.")
        return;
    }
    await delete_redis_session(session_token);
    console.log("Сессия удалена из БД.");
    res.clearCookie("session_token");
    console.log("Сессия удалена из куки браузера.")
}

/* Работа с Redis */

// Возвращает пользователя редис
async function get_redis_user(key_token) {
    if (!key_token) return;

    const userJSON = await redisClient.get(key_token);
    // Если данных нет
    if (!userJSON) {
        console.log("Нет данных в Redis для токена", key_token);
        return undefined;
    }

    // Иначе пишем, что данные есть
    console.log("Got user", userJSON, "by token", key_token);
    try {
        // Пытаемся запарсить данные пользователя
        const user = await JSON.parse(userJSON);
        console.log("JSON parsed user data:", user);
        return user;
    } catch (parseErr) {
        // Если не получилось запарсить данные, выдаём ошибку
        console.error("Ошибка парсинга JSON:", parseErr);
        return undefined;
    }
}
// Создание сессии в redis и изменение её параметров
async function create_redis_session(key_token, data) {
    const standart_data = {
        status: "Unknown",
        id: "none",
        email: "not found",
        nickname: "unknown",
        login_token: undefined, // Для операций по входу
        registration_token: undefined, // Для операций по регистрации
        refresh_token: undefined, // Для автоавторизации
    };
    const user = { ...standart_data, ...data };
    const userJSON = JSON.stringify(user);
    console.log("Created user", userJSON, "for token", key_token);
    await redisClient.set(key_token, userJSON);
    return user;
}
async function change_redis_session(key_token, new_data) {
    const userJSON = await redisClient.get(key_token);
    // Если данных нет
    if (!userJSON) {
        return await create_redis_session(key_token, new_data);
    }

    try {
        // Пытаемся запарсить до json файла данные пользователя
        const user = await JSON.parse(userJSON);
        // Меняем
        const new_user = { ...user, ...new_data };
        await redisClient.set(key_token, JSON.stringify(new_user));
        return new_user;
    } catch (parseErr) {
        // Если не получилось запарсить данные, выдаём ошибку
        console.error("Ошибка парсинга JSON:", parseErr);
        return await create_redis_session(key_token, new_data);
    }
}
async function delete_redis_session(key_token) {
    await redisClient.del(key_token);
}

/* Создание запросов на модуль авторизации */
// Отправить запрос на модуль Авторизации, связанный со входом через сервис
async function post_service_auth_request(service_type, session_token, login_token) {
    return await axios.post(process.env.AUTHORIZATION_MODULE_URL + "/auth/service", // юрл локалхотса, на котором щас этот сервер для коллбека
        {
            service_type,
            session_token,
            callback_url: "http://" + process.env.SERVER_DOMAIN + ":" + process.env.SERVER_PORT + "/login",
        },
        {
            headers: {
                "Authorization": `Bearer ${login_token}`,
                "Content-Type": "application/json"
            }
        }
    );
}
// Отправить запрос на модуль Авторизации, связанный с проверкой только что залогиненного пользователя
async function post_service_check_auth_request(type, login_token) {
    return await axios.post(process.env.AUTHORIZATION_MODULE_URL + "/auth/check",
        {
            type
        },
        {
            headers: {
                "Authorization": `Bearer ${login_token}`,
                "Content-Type": "application/json"
            }
        }
    );
}
// Отправить запрос на модуль Авторизации, связанный с подтверждением входа при логировании 
async function post_service_confirm_auth_request(type, login_token) {
    return await axios.post(process.env.AUTHORIZATION_MODULE_URL + "/auth/confirm",
        {
            type
        },
        {
            headers: {
                "Authorization": `Bearer ${login_token}`,
                "Content-Type": "application/json"
            }
        }
    );
}
// Отправить запрос на модуль Авторизации, связанный с регистрацией пользователя кодом
async function post_registration_verify_request(code, state) {
    return await axios.post(process.env.AUTHORIZATION_MODULE_URL + "/verify/registration",
        {
            state,
            code
        },
        {
            headers: {
                "Content-Type": "application/json"
            }
        }
    );
}
// Отправить запрос на модуль Авторизации, связанный с регистрацией пользователя кодом
async function post_send_registration_code_request(email, nickname, first_name, last_name, patronimyc, password, repeat_password) {
    return await axios.post(process.env.AUTHORIZATION_MODULE_URL + "/init/registration",
        {
            nickname,
            first_name,
            last_name,
            patronimyc,
            email,
            password,
            repeat_password
        },
        {
            headers: {
                "Content-Type": "application/json"
            }
        }
    );
}
// Отправить запрос на модуль Авторизации, связанный со входом обычной
async function post_default_auth_request(email, password, login_token) {
    console.log("Авторизация логин токена " + login_token + " по данным: " + email + " | " + password);
    return await axios.post(process.env.AUTHORIZATION_MODULE_URL + "/auth/default",
        {
            email,
            password,
            callback_url: "https://" + process.env.SERVER_DOMAIN + ":" + process.env.SERVER_PORT + "/login",
        },
        {
            headers: {
                "Authorization": `Bearer ${login_token}`,
                "Content-Type": "application/json"
            }
        }
    );
}
async function post_send_authorization_code_request(email) {
    console.log("Создание кода входа для почты " + email);
    return await axios.post(process.env.AUTHORIZATION_MODULE_URL + "/init/authorization",
        {
            email
        },
        {
            headers: {
                "Content-Type": "application/json"
            }
        }
    );
}
async function post_code_auth_verify_request(state, code, login_token) {
    console.log("Авторизация токена " + login_token + " по данным: " + state + " | " + code);
    return await axios.post(process.env.AUTHORIZATION_MODULE_URL + "/verify/authorization",
        {
            state,
            code,
        },
        {
            headers: {
                "Authorization": `Bearer ${login_token}`,
                "Content-Type": "application/json"
            }
        }
    );
}
// Отправить запрос на модуль Авторизации, связанный получением актуальной информации
async function get_sub_request(access_token) {
    return await axios.get(process.env.AUTHORIZATION_MODULE_URL + "/sub",
        {
            headers: {
                "Authorization": `Bearer ${access_token}`,
                "Content-Type": "application/json"
            }
        }
    );
}
// Отправить запрос на обновление токенов
async function post_refresh_request(refresh_token) {
    return await axios.post(process.env.AUTHORIZATION_MODULE_URL + "/refresh", {},
        {
            headers: {
                "Authorization": `Bearer ${refresh_token}`,
                "Content-Type": "application/json"
            }
        });
}
// Отправить запрос на модуль Авторизации, связанный с полным выходом
async function post_logout_request(access_token) {
    return await axios.post(process.env.AUTHORIZATION_MODULE_URL + "/logout/all", {},
        {
            headers: {
                "Authorization": `Bearer ${access_token}`,
                "Content-Type": "application/json"
            }
        });
}

/* Создание запросов на модуль Тестирования */
// Получение информации
async function get_testmodule_request(page, params, access_token) {
    return await axios.get(process.env.TEST_MODULE_URL + "/" + page,
        {
            params: params,
            headers: {
                "Authorization": `Bearer ${access_token}`,
                "Content-Type": "application/json"
            }
        });
}
// Отправка информации
async function post_testmodule_request(page, params, body, access_token) {
    return await axios.post(process.env.TEST_MODULE_URL + "/" + page,
        {
            ...body
        },
        {
            params: params,
            headers: {
                "Authorization": `Bearer ${access_token}`,
                "Content-Type": "application/json"
            }
        });
}

/* Конструктор страниц */
// Входа
function generate_entry_page(title = "Вход", confirm_name = " <!--Name--> ") {
    const persistent_path = "C:/Users/Admin/Desktop/WetvteGitClone/alphavet_latter/WebClient/frontend";

    const styles = fs.readFileSync(`${persistent_path}/styles/entry.css`, "utf8");

    const markup = fs.readFileSync(`${persistent_path}/markup/entry.html`, "utf8");

    const questsJS = fs.readFileSync(`${persistent_path}/scripts/quests.js`, "utf8");
    const scripts = fs.readFileSync(`${persistent_path}/scripts/entry.js`, "utf8");
    const smoothonloadJS = fs.readFileSync(`${persistent_path}/scripts/smoothonload.js`, "utf8");
    const inputfilteringJS = fs.readFileSync(`${persistent_path}/scripts/inputfiltering.js`, "utf8");

    const page = `
  <!DOCTYPE html>
  <html lang="ru">
  <head>
    <meta charset="UTF-8">
    <title>${title}</title>
    <style>${styles}</style>
  </head>
  <body>
  ${markup}
  <script>
  ${questsJS}
  const user_name = "${confirm_name}";
  ${scripts}
  ${smoothonloadJS}
  ${inputfilteringJS}
  </script>
  </body>
  </html>`;

    return page;
}
// Остальных
function generate_site_page(name, title, settings) {
    const persistent_path = "C:/Users/Admin/Desktop/WetvteGitClone/alphavet_latter/WebClient/frontend";

    const styles = fs.readFileSync(`${persistent_path}/styles/${name}.css`, "utf8");
    const bodyCSS = fs.readFileSync(`${persistent_path}/styles/body.css`, "utf8");
    const containerCSS = fs.readFileSync(`${persistent_path}/styles/container.css`, "utf8");
    const headerCSS = fs.readFileSync(`${persistent_path}/styles/header.css`, "utf8");
    const navigationCSS = fs.readFileSync(`${persistent_path}/styles/navigation.css`, "utf8");
    const footerCSS = fs.readFileSync(`${persistent_path}/styles/footer.css`, "utf8");
    const notificationCSS = fs.readFileSync(`${persistent_path}/styles/notification.css`, "utf8");
    const buttonsCSS = fs.readFileSync(`${persistent_path}/styles/buttons.css`, "utf8");
    const sectionsCSS = fs.readFileSync(`${persistent_path}/styles/sections.css`, "utf8");
    const modalCSS = fs.readFileSync(`${persistent_path}/styles/modal.css`, "utf8");

    const markup = fs.readFileSync(`${persistent_path}/markup/${name}.html`, "utf8");
    const footerHTML = fs.readFileSync(`${persistent_path}/markup/footer.html`, "utf8");
    const notificationHTML = fs.readFileSync(`${persistent_path}/markup/notification.html`, "utf8");

    const scripts = fs.readFileSync(`${persistent_path}/scripts/${name}.js`, "utf8");
    const questsJS = fs.readFileSync(`${persistent_path}/scripts/quests.js`, "utf8");
    const notificationJS = fs.readFileSync(`${persistent_path}/scripts/notification.js`, "utf8");
    const navigationJS = fs.readFileSync(`${persistent_path}/scripts/navigation.js`, "utf8");
    const smoothonloadJS = fs.readFileSync(`${persistent_path}/scripts/smoothonload.js`, "utf8");
    const inputfilteringJS = fs.readFileSync(`${persistent_path}/scripts/inputfiltering.js`, "utf8");
    const sectionsJS = fs.readFileSync(`${persistent_path}/scripts/sections.js`, "utf8");

    const page = `
  <!DOCTYPE html>
  <html lang="ru">
  <head>
    <meta charset="UTF-8">
    <title>${title}</title>
    <style>
    ${bodyCSS}
    ${containerCSS}
    ${headerCSS}
    ${navigationCSS}
    ${footerCSS}
    ${notificationCSS}
    ${sectionsCSS}
    ${buttonsCSS}
    ${styles}
    ${modalCSS}
    </style>
  </head>
  <body>
  <div class="container">
  ${markup}
  ${footerHTML}
  </div>
  ${notificationHTML}
  <script>
  load_data = ${JSON.stringify(settings)};
  ${questsJS}
  ${scripts}
  ${notificationJS}
  ${smoothonloadJS}
  ${navigationJS}
  ${inputfilteringJS}
  ${sectionsJS}
  </script>
  </body>
  </html>`;

    // settings - данные для загрузки страницы (например, айди просматриваемого ресурса), передающееся как ключ-значение в {}

    return page;
}

/* Создание сервера и подключение к Redis */

// Асинхронное подключение к Redis
// Используем async/await для ожидания установления соединения
(async () => {
    await redisClient.connect();
})();

// Определение порта для сервера
// Порт берётся из переменных окружения (SERVER_PORT)
const SERVER_PORT = process.env.SERVER_PORT;

// Запуск сервера на указанном порту
// После запуска выводится сообщение в консоль
app.listen(SERVER_PORT, () => {
    console.log(`Server running on port ${SERVER_PORT}`);
});