// Импорт библиотеки dotenv для работы с переменными окружения из .env-файла
const dotenv = require('dotenv');
// Импорт фреймворка Express для создания веб‑сервера
const express = require('express');
// Импорт парсера для кук
const cookieParser = require('cookie-parser');
// Импорт middleware cors для обработки кросс‑доменных запросов
const cors = require('cors');
// Импорт клиента Redis для взаимодействия с сервером Redis
const redis = require('redis');
// Импорт библиотеки axios для выполнения HTTP‑запросов (на будущее, а то мало ли)
const axios = require('axios');

// Загрузка переменных окружения из указанного .env‑файла
// Путь жёстко задан, так как я иначе не смог чот
dotenv.config({path: 'C:\\Users\\Admin\\Desktop\\WetvteGitClone\\alphavet_latter\\WebClient\\config\\client_config.env'});

// Создание экземпляра приложения Express
const app = express();

// Middleware — промежуточное ПО для обработки запросов

// Разрешает кросс‑доменные запросы (CORS) без ограничений (небезопасно для продакшена)
app.use(cors());
// Позволяет парсить куки
app.use(cookieParser());
// Позволяет приложению обрабатывать JSON в теле HTTP‑запросов
app.use(express.json());

// Подключение к серверу Redis
// Создаём клиент Redis, используя URL из переменных окружения (REDIS_URI)
const redisClient = redis.createClient({
  url: process.env.REDIS_URI
});

// Обработчики событий для клиента Redis

// Событие 'connect': срабатывает при установлении соединения с Redis
redisClient.on('connect', () => {
  console.log('Подключено к Redis');
});
// Событие 'ready': Redis готов принимать команды
redisClient.on('ready', () => {
  console.log('Redis готов к работе');
});
// Событие 'error': срабатывает при ошибке соединения/операции с Redis
redisClient.on('error', (err) => {
  console.error('Redis Error:', {
    message: err.message,           // текстовое описание ошибки
    code: err.code,               // код ошибки (например, 'ECONNREFUSED')
    command: err.command,            // команда Redis, вызвавшая ошибку
    stack: err.stack                 // стек вызовов для отладки
  });
});
// Событие 'end': соединение с Redis закрыто
redisClient.on('end', () => {
  console.log('Соединение с Redis закрыто');
});

// Асинхронное подключение к Redis
// Используем async/await для ожидания установления соединения
(async () => {
  await redisClient.connect();
})();

// Маршрутизация — определение обработчиков для разных URL

// Маршрут для главной страницы ('/')
app.get('/', async (req, res) => {
  // Получаем токен сессии из cookie запроса
  const token = req.cookies['session_token'];

  // Если токена нет — показываем форму входа с ссылками на GitHub и Яндекс
  if (!token) {
    // Пользователь Unknow
    return res.send(`
      <h1>Добро пожаловать!</h1>
      <p><a href="/login?type=github">Войти через GitHub</a></p>
      <p><a href="/login?type=yandex">Войти через Яндекс ID</a></p>
    `);
  }

  try {
    // Получаем токен из редис
    console.log('Getting user data by token', token);
    const userData = await redisClient.get(token);
    // Если данных нет
    if (!userData) {
      // Удаляем куку и перезаходим на страницу
      console.log('Нет данных в Redis для токена', token);
      res.clearCookie('session_token');
      return res.redirect('/');
    }

    // Иначе пишем, что данные есть
    console.log('Got user data', userData, 'by token', token);

    try {
      // Пытаемся запарсить до json файла данные пользователя
      const user = JSON.parse(userData);
      console.log('JSON parsed user data:', user);
      // Если пользователь авторизован, меняем страницу
      if (user.status === 'Authorized') {
        res.send(`
          <h1>Личный кабинет</h1>
          <p>Вы авторизованы как ${user.email}</p>
          <p><a href="/logout">Выйти</a></p>
        `);
      } else {
        // Если не авторизован, перекидываем на страницу авторизации
        res.redirect('/login');
      }
    } catch (parseErr) {
      // Если не получилось запарсить данные, выдаём ошибку
      console.error('Ошибка парсинга JSON:', parseErr);
      res.status(500).send('Ошибка обработки данных пользователя');
    }
  } catch (redisErr) {
    // Если не получилось получить данные от редис, выдаём ошибку
    console.error('Ошибка Redis:', redisErr);
    res.status(500).send('Ошибка сервера Redis');
  }
});

// Маршрут для страницы входа ('/login')
app.get('/login', (req, res) => {
  // Получаем параметр 'type' из query‑строки URL (например, ?type=github)
  const { type } = req.query;
  // Если параметр type не указан — перенаправляем на главную
  if (!type) {
    return res.redirect('/');
  }

  // Генерируем случайный токен сессии (для идентификации пользователя)
  const sessionToken = Math.random().toString(36).substring(2);
  console.log('Session token created', sessionToken);
  // Генерируем случайный токен входа (для процесса авторизации)
  const loginToken = Math.random().toString(36).substring(2);
  console.log('Login token created', loginToken);

  // Сохраняем в Redis запись: ключ — sessionToken, значение — JSON с данными
  // status: 'Anonymous' — пользователь ещё не авторизован
  // loginToken — токен для процесса входа
  // type — тип провайдера (none/github/yandex)
  redisClient.set(sessionToken, JSON.stringify({
    status: 'Anonymous',
    loginToken,
    type,
  }));

  // Устанавливаем cookie 'session_token' со значением sessionToken
  // httpOnly: true — cookie недоступен через JavaScript (защита от XSS)
  res.cookie('session_token', sessionToken, { httpOnly: true });

  // Здесь нужно как-то сделать запрос к модулю авторизации

  // Перенаправляем пользователя на главную страницу
  res.redirect('/');
});

// Маршрут для выхода из системы ('/logout')
app.get('/logout', async (req, res) => {
  // Получаем токен сессии из cookie
  const token = req.cookies['session_token'];

  // Если токен есть — удаляем запись из Redis и очищаем cookie
  if (token) {
    await redisClient.del(token);               // Удаляем запись по ключу token
    res.clearCookie('session_token');        // Очищаем cookie на клиенте
  }

  // Перенаправляем на главную страницу
  res.redirect('/');
});

// Определение порта для сервера
// Порт берётся из переменных окружения (SERVER_PORT)
const SERVER_PORT = process.env.SERVER_PORT;

// Запуск сервера на указанном порту
// После запуска выводится сообщение в консоль
app.listen(SERVER_PORT, () => {
  console.log(`Server running on port ${SERVER_PORT}`);
});
