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
const axios = require("axios");

/* Загрузка переменных окружения из указанного .env‑файла */
// Путь жёстко задан, так как я иначе не смог чот
dotenv.config({path: "C:\\Users\\Admin\\Desktop\\WetvteGitClone\\alphavet_latter\\WebClient\\config\\client_config.env"});

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
  console.log("Подключено к Redis");
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

// Маршрут для главной страницы ("/")
app.get("/", async (req, res) => {
  console.log("Пользователь зашёл на страницу /");
  // Получаем токен сессии из cookie запроса
  const sessionToken = req.cookies["sessionToken"];
  // Переменная данных о статусе и другом пользователя
  const user = sessionToken && sessionToken.length > 0 ? (await get_redis_user(sessionToken)) : undefined;
  console.log("Получен пользователь :", user, "благодаря токену сессии", sessionToken);
  // Если пользователь авторизирован
  if (user && user["status"] === "Authorized") {
    /// Отправляем рефреш токен с целью обновить данные
    console.log("Отправлен запрос на обновление данных пользователя " + user["name"]);
    const update_response = await post_autho_update_request(user["refreshToken"]);
    // Если данные успешно проверены/обновлены/получены и т.д.
      console.log("Одобрен запрос на обновление данных пользователя " + user["name"]);
    if (update_response.data["update_result"] === "success") {
      change_redis_session(sessionToken, {
        id: update_response.data["id"],
        name: update_response.data["name"],
        email: update_response.data["email"],
        refreshToken: update_response.data["refreshToken"]
      })
      return res.sendFile("C:\\Users\\Admin\\Desktop\\WetvteGitClone\\alphavet_latter\\WebClient\\frontend\\cabinet.html");
    }
    // Если токены истекли/ошибка обновления
    else {
      console.log("Провален запрос на обновление данных пользователя " + user["name"]);
      // Очищаем куки и данные сессии, перезагружаем страницу
      res.clearCookie("sessionToken");
      delete_redis_session(sessionToken)
      return res.redirect("/")
    }
  }
  // Иначе выводит форму для входа
  else {
    res.sendFile("C:\\Users\\Admin\\Desktop\\WetvteGitClone\\alphavet_latter\\WebClient\\frontend\\entry.html");
  }
});

// Маршрут для входа ("/login") post обрабатывает любой вид входа, а get только атоматический
app.get("/login", async (req, res) => {
  console.log("Попытка автовхода.");
  // P.S.2 Теперь понял: пользователь после авторизации через OAuth возвращается на /login веб клиента и,
  // так как ему нужно автоматом войти, и он сохранился анонимным, обрабатываем автовход (ещё нужно для default типа логирования)

  // Получаем параметр "type" из query‑строки URL (например, ?type=github)
  const { type } = req.query;
  console.log("Пользователь обратился к get странице /login");
  // Для процессов авторизации есть пост запрос 
  if (type && type.length > 0) {
    return res.redirect("/");
  }
  // Получаем токен сессии из cookie запроса
  let sessionToken = req.cookies["sessionToken"];
  console.log("Полуенные куки сессии:" + sessionToken);
  // Ищем пользователя по куки
  let user = sessionToken && sessionToken.length > 0 ? (await get_redis_user(sessionToken)) : undefined;
  if (!user || user["status"] === "Unknown") {
    // Авто-входу не быть
    return res.redirect("/");
  }

  // Получает цель визита. 
  let target = req.query.target;
  console.log("Начальная цель -", target);
  // Если цель - отмена (Пользователь нажал "нет" в подтверждении)
  if (target === "cancel") {
    // Удаляем и уходим ^_^
    console.log("Подтверждение входа отклонено.");
    delete_redis_session(sessionToken);
    res.clearCookie("sessionToken");
    return res.redirect("/");
  }
  // Если цели нет, ею выбирается Проверка
  if (!target || target != "confirm") {
    target = "check";
  }

  // Если цель - проверка
  if (target == "check") {
    // Делаем запрос о проверке, есть ли пользователь в базе данных
    const check_response = user["loginToken"] ? (await post_service_check_auth_request(type, user["loginToken"])) : undefined;
    // Проверяет соответствие токена
    if (!check_response || user["loginToken"] != check_response.data["loginToken"]) {
      console.error("Ошибка проверки входа (токены не соответствуют или отсутствуют)");    
      // Пользователя не знаем, -куки
      delete_redis_session(sessionToken);
      res.clearCookie("sessionToken");
      return res.redirect("/");
    }
    // Если пользователя нет в базе данных /*check_response.data["access_state"] === "denied"*/
    if (check_response.data["access_state"] !== "allowed") {
      console.log("Вход отклонён.", check_response.data["message"]);
      delete_redis_session(sessionToken);
      res.clearCookie("sessionToken");
      return res.redirect("/");
    }
    console.log("Вход одобрен и ждёт подтверждения для имени " + check_response.data["name"]);
    // Если пользователь пошёл через код, сразу переходит на подтверждение
    if (user["loginType"] === "default") {
      return res.redirect("/login?target=confirm");
    }
    // Иначе отправляем ему страницу входа с настройками для подтверждения
    else {
      // Сохраняем имя
      user = await change_redis_session(sessionToken, {
        check_name: check_response.data["name"],
      })
      // Читаем файл страницы
      fs.readFile("C:\\Users\\Admin\\Desktop\\WetvteGitClone\\alphavet_latter\\WebClient\\frontend\\entry.html", 'utf8', (err, page) => {
        if (err) {
          return res.status(500).send('Ошибка чтения файла: ' + err.message);
        }
        // Заменяем строку имени
        page = page.replace(" <!--Name--> ", check_response.data["name"]);
        // Отправляем изменённый контент клиенту
        res.setHeader("Content-Type", "text/html; charset=utf-8");
        return res.send(page);
      });
    }
  }
  // Если цель - подтверждение
  else {
    // Отправляем запрос на подтверждение входа
    const confirm_response = await post_service_confirm_auth_request(type, user["loginToken"]);
    // Проверяем соответствие токена входа
    if (user["loginToken"] != confirm_response.data["loginToken"]) {
      console.error("Ошибка подтверждения входа (токены не соответствуют)");
      delete_redis_session(sessionToken);
      res.clearCookie("sessionToken");
      return res.redirect("/");
    }
    // Проверяем статус подтверждения
    if (confirm_response.data["confirm_state"] !== "success") {
      console.log("Вход отклонён.", confirm_response.data["message"]);
      delete_redis_session(sessionToken);
      res.clearCookie("sessionToken");
      return res.redirect("/");
    }
    // Проверяем наличие необходимых токенов
    if (!confirm_response.data["accessToken"] || !confirm_response.data["refreshToken"]) {
      console.error("Э, а где токены, йоу? Ты вернул "
        + confirm_response.data["accessToken"] + " ИиИ " + confirm_response.data["refreshToken"]);
      delete_redis_session(sessionToken);
      res.clearCookie("sessionToken");
      return res.redirect("/");
    }
    // Обновляем сессию в Redis — пользователь авторизован
    user = await create_redis_session(sessionToken, {
      status: "Authorized",
      loginType: confirm_response.data["loginType"],
      id: confirm_response.data["id"],
      email: confirm_response.data["email"],
      name: confirm_response.data["name"],
      refreshToken: confirm_response.data["refreshToken"]
    });
    return res.redirect("/");
  }
});
app.post("/login", async (req, res) => {
  // Получаем параметр "type" из query‑строки URL (например, ?type=github)
  const { type } = req.query;
  console.log("Пользователь обратился к post странице /login?type=" + type);
  // Если тип входа некорректен, возвращаем ошибку (Автовход для анонима обрабатывается в гет запросе выше)
  if (type !== "default" && type !== "github" && type !== "yandex") {
    console.log("Неверный тип входа.")
    return res.send(JSON.stringify({
      success_state: "fail",
      message: "Неверный тип входа."
    }));
  }

  // Получаем токен сессии из cookie запроса
  let sessionToken = req.cookies["sessionToken"];
  console.log("Полуенные куки сессии:" + sessionToken);
  // Ищем пользователя по куки
  let user = sessionToken && sessionToken.length > 0 ? (await get_redis_user(sessionToken)) : undefined;

  // Если пользователь авторизован, возвращаем ошибку
  if (user && user["status"] === "Authorized") {
    console.log("Пользователь уже авторизован.")
    return res.send(JSON.stringify({
      success_state: "fail",
      message: "Пользователь уже авторизован."
    }));
  }
  // Если пользователь неизвестный
  if (!user || user["status"] === "Unknown") {
    console.log("Проходит запись неизвестного пользователя в анонимы.")
    // Генерируем случайный токен сессии (для идентификации пользователя)
    sessionToken = generate_random_token();
    console.log("Session token created", sessionToken);
    user = await create_redis_session(sessionToken, {
      status: "Anonymous",
      loginType: type,
      name: "none"
    });
  }
  console.log("Проходит авторизация анонимного пользователя.")
  // Остались только Анонимы с конкретным type входа
  // Генерируем случайный токен входа (для процесса авторизации)
  const loginToken = generate_random_token();
  console.log("Login token created", loginToken);
  // Сохраняем токен входа
  user = await change_redis_session(sessionToken, {
    loginToken: loginToken
  });
  // Отправляем запрос на авторизацию соответствующим type образом
  let auth_response;
  if (type == "default") {
    const {email, password} = req.body;
    auth_response = await post_default_auth_request(email, password, loginToken);
    // Если успешно, сохраняем в сессию
    if (auth_response.data["success_state"] === "success") {
      res.cookie("sessionToken", sessionToken)
    }
    // Возвращаем результат авторизации кодом, чтобы челик знал, что он не так сделал (или так)
    res.send(JSON.stringify({
      success_state: auth_response.data["success_state"],
      message: auth_response.data["message"]
    }));
  } else {
    auth_response = await post_service_auth_request(type, sessionToken, loginToken);
    // Если нет ошибки, редирект на полученный oauth (ВАЖНО !!! Нужно передать юрл обратно юзеру на страницу, и там уже открывать)
    console.log("Полученный url для редикта на oauth: " + auth_response.data["oauth_redirect_url"] + ".");
    if (auth_response.data["oauth_redirect_url"] && auth_response.data["oauth_redirect_url"].length > 0) {
      console.log("Передаём пользователю.")
      // Если успешно, сохранение кук будет через Auth module
      res.send(JSON.stringify({
        oauth_connect_success_state: "success",
        oauth_redirect_url: auth_response.data["oauth_redirect_url"]
      }));
    }
  }
});

// Маршрут для регистрации ("/registration")
app.post("/registration", async (req, res) => {
  // Получаем токен регистрации
  const token = get_token_from_header(req.headers.authorization);
  // Если регистрация на этапе обычного запроса
  if (!token/*stage === "filling"*/) {
    const {email, name, password, repeat_password} = req.body;
    const registration_response = await post_registration_request(email, name, password, repeat_password);
    if (registration_response.data["access_state"] === "allowed") {
      return res.send(JSON.stringify({
        access_state: "allowed",
        message: registration_response.data["message"],
        registrationToken: registration_response.data["registrationToken"]
      }));
    }
    else {
      return res.send(JSON.stringify({
      access_state: "denied",
      message: registration_response.data["message"],
    }));
    }
  }
  // Если регистрация на этапе подтверждения
  else /*stage === "verifing"*/ {
    const { verifyCode } = req.body;
    const verify_response = await post_registration_verify_request(verifyCode, token);
    return res.send(JSON.stringify({
      access_state: verify_response.data["success_state"],
      message: verify_response.data["message"]
    }));
  }
});

// Получает токен из заголовка авторизации
function get_token_from_header(header) // header authorization
{
  if (!header || header === "") {
    return;
  }
  const parts = header.split(" ");
  if (parts.legth !== 2 || parts[1].legth === 0) {
    return;
  }
  return parts[1];
}

// Маршрут для выхода из системы ("/logout")
app.get("/logout", async (req, res) => {
  // Получаем токен сессии из cookie
  const token = req.cookies["sessionToken"];

  // Переменная данных о статусе и другом пользователя
  let user;

  // Если неизвестен
  if (!token || (((user = await get_redis_user(token)) && user["status"] === "Unknown"))) {
    console.error("Куда лезешь, если не авторизирован даже");
    // Перенаправляем на главную страницу
    return res.redirect("/");
  }

  // Если при проверке пользователь не был получен из Redis, он получается здесь
  if (!user) {
    user = await get_redis_user(token);
    // Выкидывает анонимов тоже
    if (user["status"] === "Anonimous") {
      console.error("Куда лезешь, если не авторизирован даже");
      // Перенаправляем на главную страницу
      return res.redirect("/");
    }
  }

  // Получение параметра all из url
  const { all } = req.query;
  // Выход со всех устройств
  if (all === "true") {
    await post_logout_request(user["refreshToken"])
  }
  
  // Удаляем запись по ключу token
  await redisClient.del(token);
   // Очищаем cookie на клиенте
  res.clearCookie("sessionToken");
  // Перенаправляем на главную страницу
  return res.redirect("/");
});

/* Создание токенов сессии и входа */
// Сделать рандомный токен (строку)
function generate_random_token() {
  return Math.random().toString(36).substring(2);
}

/* Работа с Redis */

// Возвращает пользователя редис
async function get_redis_user(key_token) {
  const userJSON = await redisClient.get(key_token);
  // Если данных нет
  if (!userJSON) {
    console.log("Нет данных в Redis для токена", key_token);
    return;
  }

  // Иначе пишем, что данные есть
  console.log("Got user", userJSON, "by token", key_token);
  try {
    // Пытаемся запарсить до json файла данные пользователя
    const user = JSON.parse(userJSON);
    console.log("JSON parsed user data:", user);
    return user;
  } catch (parseErr) {
    // Если не получилось запарсить данные, выдаём ошибку
    console.error("Ошибка парсинга JSON:", parseErr);
  }
}
// Создание сессии в redis и изменение её параметров
async function create_redis_session(key_token, data) {
  const standart_data = {
    status: "Unknown",
    loginType: "none",
    id: "none",
    email: "not found",
    name: "unknown",
    loginToken: undefined, // Для операций по входу
    registrationToken: undefined, // Для операций по регистрации
    refreshToken: undefined, // Для автоавторизации
  };
  const user = {...standart_data, ...data};
  const userJSON = JSON.stringify(user);
  console.log("Created user", userJSON, "for token", key_token);
  await redisClient.set(key_token, userJSON);
  return user;
}
async function change_redis_session(key_token, new_data) {
  const userJSON = await redisClient.get(key_token);
  // Если данных нет
  if (!userJSON) {
    console.log("Нет данных в Redis для токена", key_token);
    return;
  }
  try {
    // Пытаемся запарсить до json файла данные пользователя
    const user = JSON.parse(userJSON);
    // Меняем
    const new_user = {...user, ...new_data};
    await redisClient.set(key_token, JSON.stringify(new_user));
    return new_user;
  } catch (parseErr) {
    // Если не получилось запарсить данные, выдаём ошибку
    console.error("Ошибка парсинга JSON:", parseErr);
  }
}
async function delete_redis_session(key_token) {
  await redisClient.del(key_token);
}

/* Создание запросов на модуль авторизации */

// Отправить запрос на модуль Авторизации, связанный со входом через сервис
async function post_service_auth_request(serviceType, sessionToken, loginToken) {
  return await axios.post(process.env.AUTHORIZATION_MODULE_URL + "/service_auth", // юрл локалхотса, на котором щас этот сервер для коллбека
    {
      serviceType,
      sessionToken,
      callback_url: "http://" + process.env.SERVER_DOMAIN + ":" + process.env.SERVER_PORT + "/login",
    },
    {
      headers: {
        "Authorization": `Bearer ${loginToken}`,
        "Content-Type": "application/json"
      }
    }
  );
}
// Отправить запрос на модуль Авторизации, связанный с проверкой только что залогиненного пользователя
async function post_service_check_auth_request(type, loginToken) {
  return await axios.post(process.env.AUTHORIZATION_MODULE_URL + "/check_auth",
    {
      type
    },
    {
      headers: {
        "Authorization": `Bearer ${loginToken}`,
        "Content-Type": "application/json"
      }
    }
  );
}
// Отправить запрос на модуль Авторизации, связанный с подтверждением входа при логировании 
async function post_service_confirm_auth_request(type, loginToken) {
  return await axios.post(process.env.AUTHORIZATION_MODULE_URL + "/confirm_auth",
    {
      type
    },
    {
      headers: {
        "Authorization": `Bearer ${loginToken}`,
        "Content-Type": "application/json"
      }
    }
  );
}
// Отправить запрос на модуль Авторизации, связанный с регистрацией пользователя кодом
async function post_registration_request(email, name, password, repeat_password) {
  return await axios.post(process.env.AUTHORIZATION_MODULE_URL + "/registration",
    {
      stage: "filling",
      name,
      email,
      password,
      repeat_password
    }
  );
}
// Отправить запрос на модуль Авторизации, связанный с регистрацией пользователя кодом
async function post_registration_verify_request(verifyCode, registrationToken) {
  return await axios.post(process.env.AUTHORIZATION_MODULE_URL + "/registration",
    {
      stage: "verifing",
      verifyCode
    },
    {
      headers: {
        "Authorization": `Bearer ${registrationToken}`,
        "Content-Type": "application/json"
      }
    }
  );
}
// Отправить запрос на модуль Авторизации, связанный со входом через код
async function post_default_auth_request(email, password, loginToken) {
  console.log("Авторизация логин токена " + loginToken + " по данным: " + email + " | " + password);
  return await axios.post(process.env.AUTHORIZATION_MODULE_URL + "/default_auth",
    {
      email,
      password,
      callback_url: "https://" + process.env.SERVER_DOMAIN + ":" + process.env.SERVER_PORT + "/login",
    },
    {
      headers: {
        "Authorization": `Bearer ${loginToken}`,
        "Content-Type": "application/json"
      }
    }
  );
}
// Отправить запрос на модуль Авторизации, связанный получением актуальной информации
async function post_autho_update_request(refreshToken) {
  return await axios.post(process.env.AUTHORIZATION_MODULE_URL + "/update_auth", {},
    {
      headers: {
        "Authorization": `Bearer ${refreshToken}`,
        "Content-Type": "application/json"
      }
    }
  );
}
// Отправить запрос на модуль Авторизации, связанный с полным выходом
async function post_logout_request(refreshToken) { 
  return await axios.post(process.env.AUTHORIZATION_MODULE_URL + "/logout_all", {},
  {
    headers: {
      "Authorization": `Bearer ${refreshToken}`,
      "Content-Type": "application/json"
    }
  }
);
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