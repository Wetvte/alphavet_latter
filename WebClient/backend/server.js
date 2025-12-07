const express = require('express');
const dotenv = require('dotenv');

// Загружаем настройки из .env
dotenv.config("../config/client_config.env");

const app = express();
const SERVER_PORT = process.env.SERVER_PORT || 3000;

// Маршрут для главной страницы
app.get('/', (req, res) => {
    res.send('<h1>Добро пожаловать!</h1><p><a href="/login">Войти</a></p>');
});

// Маршрут для страницы входа
app.get('/login', (req, res) => {
    res.send('<h1>Страница входа</h1><p>Здесь будут кнопки GitHub/Яндекс</p>');
});

// Запускаем сервер
app.listen(SERVER_PORT, () => {
    console.log(`Сервер запущен на порту ${SERVER_PORT}`);
});
