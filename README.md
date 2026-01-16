Проект основан на: https://github.com/VladimirChabanov/alg_and_prog_2025/blob/main/02_practice/03_group_projects/details/

Для настройки проекта нужно провернуть следующие дейтвия следующие действия:
1) Настроить модуль Авторизациия:
- Зарегистрировать OAuth аккаунты Github и Yandex
- Создать кластер данных на сайте MongoDB
- Создать почту и открыть усправление через smtp, получить специальный пароль для управления ею изве
- Создать файл .env с данными с указать его путь в коде файла Authorization/environment.go.
- Настроить содержимое .env файла:
    SERVER_PORT=8080 # Порт сервера авторизации
    MONGODB_URI=mongodb+srv://(Пользователь):(Пароль)@(Ссылка на кластер)
    GITHUB_CLIENT_ID=
    GITHUB_CLIENT_SECRET=
    YANDEX_CLIENT_ID=
    YANDEX_CLIENT_SECRET=
    CALLBACK_URL=http://localhost:(Порт, см. выше)/auth_callback
    OWN_TOKEN_GENERATE_KEY= # Ключ для генерации токенов
    PASSWORD_MIN_DURATION=
    PASSWORD_MAX_DURATION=
    NAME_MIN_DURATION=
    NAME_MAX_DURATION=
    MAIL=
    MAIL_PASSWORD=
    MAIL_PASSWORD_OUTSIDE= # Спец. пароль
    TEST_MODULE_URL=http://localhost:4000 # Адрес основного модуля
- Запускается командой go run . в директиве Authorization

2) Настроить модуль Веб клиента
- Изменить WebClient/frontend/markup/footer.html (по желанию)
- В WebClient/frontend/scripts/quests.js Изменить переменные с URL адресом модуля Клиента, если планируется менять
- Создать базу данных на сайте Redis
- Создать файл .env с данными с указать его путь в коде файла WebClient/backend/server.js.
- Настроить содержимое .env файла:
    SERVER_DOMAIN=localhost
    SERVER_PORT=3000 # Порт сервера клиента
    SERVER_URL=http://localhost/web
    SERVERS_COUNT=
    # Ссылка на редис, как она дана на сайте
    REDIS_URI=redis://(Ссылка)
    AUTHORIZATION_MODULE_URL=http://localhost:8080 # Адрес подуля авторизации
    TEST_MODULE_URL=http://localhost:4000 # Адрес Основного подуля
- Настроить переменную persistent_path в методах generate_site_page и generate_entry_page в WebClient/backend/server.js
- Запускается командой node server.js в директиве WebClient/backend

3) Настроить модуль Telegram бота
- Создать бота и сохранить его токен
- Создать файл .env с данными с указать его путь в коде файла ...
- Настроить содержимое .env файла:
    SERVER_HOST=localhost
    SERVER_PORT=2000
    SERVER_URL=http://localhost/tbot
    SERVERS_COUNT=
    # Для редис нужна строка вида
    REDIS_URI=домен:порт,password=пароль,abortConnect=false,connectTimeout=10000,syncTimeout=10000
    # Адрес Основного модуля
    AUTHORIZATION_MODULE_URL=http://localhost:8080
    # Адрес модуля Авторизации
    TEST_MODULE_URL=http://127.0.0.1:4000
    # Токен бота
    BOT_TOKEN=8291005932:AAHK4-Tru9Kye-w-Javu3xjIgwXMHlms5qk
- Запускается командой dotnet run в директиве TelegramBot

4) Настроить основной модуль
- Скачать PostgreSQL
- Создать файл .env с данными с указать его путь в коде файла TestProcessing/src/db_connector.cpp.
- Настроить содержимое .env файла:
    # Для PostgreSQL
    DB_HOST=localhost
    DB_PORT=5432
    DB_NAME=LatterProject
    DB_USER=postgres
    DB_PASS= # Пароль от БД
    # Хост модуля (localhost не работает)
    API_HOST=127.0.0.1
    # Порт модуля
    API_PORT=4000
    # Ключ генерации токенов из модуля Авторизации
    OWN_TOKEN_GENERATE_KEY=ABOBA
    # Адрес Основного подуля Авторизации
    AUTHORIZATION_MODULE_URL=http://localhost:8080
    AUTHORIZATION_MODULE_HOST=localhost
    AUTHORIZATION_MODULE_PORT=8080
- Создать следующие таблицы: users, disciplines, tests, questions, tries, answers, news.
- Настроить таблицы кодом ниже.
- Установить jwt-cpp в vcpkg, расположенном на локальной машине и указать полный путь к нему в TestProcessing/CMakeLists.txt
- В файле .vscode/settings.json указать свои пути для папок build и CMakeLists.txt
- Запускается командой latter.exe в директиве TestProcessing/build/Debug

5) Каждый из модулей запускается в отдельной cmd.
- Перед запуском модулей стоит включить VPN.
- Для запуска модуля авторизации нужно попасть в папку Authorization и испольнить команду "go run ."
- Для запуска Веб клиента необходимо в папке WebClient вызвать команду "node backend/server.js"
- Для запуска Основного модуля необходимо в папке TestProcessing ...
- Для запуска модуля Telegram бота необходимо ...

# Чтобы объединить клиенты Веб и Телеграм, нужно установить Nginx и в его папке conf изменить файл nginx.conf:
upstream web {
    server http://localhost:3001;
    server http://localhost:3002;
    server http://localhost:3003;
}

upstream tbot {
    server http://localhost:2001;
    server http://localhost:2002;
    server http://localhost:2003;
}

server {
    listen 80;
    server_name localhost;

    # Прокси для веб‑сервисов
    location /web/ {
        proxy_pass http://web/;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    }

    # Прокси для ботов
    location /bot/ {
        proxy_pass http://tbot/;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    }

    # Здоровье
    location /health/ {
        return 200 "OK\n";
    }
}


# SQL код для настройки созданных таблиц
ALTER TABLE answers
ADD COLUMN id INT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
ADD COLUMN question_signature INT[2] NOT NULL,
ADD COLUMN options INT[] DEFAULT '{}',
ADD COLUMN points INT DEFAULT 0,
ADD COLUMN max_points INT,
ADD COLUMN author VARCHAR(20) NOT NULL;
-- Функция установки макс очков
CREATE OR REPLACE FUNCTION set_max_points()
RETURNS TRIGGER AS $$
DECLARE
    max_options INT;  -- количество элементов для суммирования
BEGIN
     -- Получаем max_options из questions
    SELECT COALESCE(q.max_options_in_answer, 1) INTO max_options
    FROM questions q
    WHERE q.id = NEW.question_signature[1] 
      AND q.version = NEW.question_signature[2];

    -- Вычисляем сумму топ‑N элементов
    SELECT COALESCE(SUM(sub.points), 0) INTO NEW.max_points
    FROM (
        SELECT unnest(q.points) AS points
        FROM questions q
        WHERE q.id = NEW.question_signature[1]
          AND q.version = NEW.question_signature[2]
        ORDER BY points DESC
        LIMIT max_options  -- Сначала LIMIT, потом SUM
    ) sub;
    
	RETURN NEW;  -- возвращаем изменённую строку для вставки
END;
$$ LANGUAGE plpgsql;
CREATE TRIGGER trigger_set_max_points
BEFORE INSERT ON answers
FOR EACH ROW
EXECUTE FUNCTION set_max_points();
-- Функция установки текущих очков
CREATE OR REPLACE FUNCTION set_points()
RETURNS TRIGGER AS $$
DECLARE
    current_option_number INT;      -- текущий индекс из Options
    option_points INT;       -- значение из Points по этому индексу
    total_points INT := 0; -- итоговая сумма
    question_points INT[]; -- массив Points из Questions
BEGIN
    -- Получаем массив Points из соответствующей строки Questions
    SELECT points INTO question_points FROM questions
    WHERE id = NEW.question_signature[1] AND version = NEW.question_signature[2];
    -- Если Questions не найдена или Points пуст — обнуляем результат
    IF question_points IS NULL OR array_length(question_points, 1) IS NULL THEN
        NEW.points := 0;
        RETURN NEW;
    END IF;

    -- Проходим по всем элементам массива Options
    FOREACH current_option_number IN ARRAY NEW.options LOOP
        -- Пропускаем неположительные индексы
        IF current_option_number <= 0 THEN
            CONTINUE;
        END IF;
        -- Проверяем, существует ли элемент с таким индексом в Points
        IF current_option_number > array_length(question_points, 1) THEN
            CONTINUE;
        END IF;

        -- Берём значение по индексу (PostgreSQL: индексация с 1!)
        option_points := question_points[current_option_number];
        -- Добавляем к сумме, если значение определено
        IF option_points IS NOT NULL THEN
            total_points := total_points + option_points;
        END IF;
    END LOOP;

    -- Записываем итоговую сумму
    NEW.points := total_points;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;
CREATE TRIGGER trigger_set_points
BEFORE UPDATE OF options ON answers  -- срабатывает при обновлении Options
FOR EACH ROW
EXECUTE FUNCTION set_points();

ALTER TABLE tries
ADD COLUMN id INT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
ADD COLUMN status VARCHAR(12) DEFAULT 'Solving',
ADD COLUMN author VARCHAR(20) NOT NULL,
ADD COLUMN answers INT[] NOT NULL,
ADD COLUMN points INT,
ADD COLUMN max_points INT,
ADD COLUMN score_percent INT,
ADD COLUMN test INT NOT NULL;
-- Функция контроля показателей points при изменении ответов
CREATE OR REPLACE FUNCTION calculate_points()
RETURNS TRIGGER AS $$
DECLARE
    total_points INT := 0;
    total_max_points INT := 0;
    answer_number INT;
    row_points INT;
    row_max_points INT;
BEGIN
    -- Если массив answers пуст или NULL — обнуляем значения
    IF NEW.answers IS NULL OR array_length(NEW.answers, 1) = 0 THEN
        NEW.points := 0;
        NEW.max_points := 0;
        NEW.score_percent := 0;
        RETURN NEW;
    END IF;

    -- Проходим по всем ID из массива answers
    FOREACH answer_number IN ARRAY NEW.answers LOOP
        -- Получаем points и max_points для текущего answer_number
        SELECT points, max_points INTO row_points, row_max_points FROM answers WHERE id=answer_number;

        -- Накапливаем суммы
        total_points := total_points + row_points;
        total_max_points := total_max_points + row_max_points;
    END LOOP;

    -- Записываем результаты в строку tries
    NEW.points := total_points;
    NEW.max_points := total_max_points;

    -- Рассчитываем процент (избегаем деления на 0)
    IF total_max_points > 0 THEN
        NEW.score_percent := (total_points * 100) / total_max_points;
    ELSE
        NEW.score_percent := 0;
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;
CREATE TRIGGER trigger_calculate_tries_scores
BEFORE UPDATE OF status, answers ON tries
FOR EACH ROW
EXECUTE FUNCTION calculate_points();
ALTER TABLE questions
ADD COLUMN id INT,
ADD COLUMN version INT,
ADD COLUMN status VARCHAR(20) DEFAULT 'Exists',
ADD COLUMN author VARCHAR(20) NOT NULL,
ADD COLUMN title VARCHAR(100) NOT NULL,
ADD COLUMN options TEXT[] DEFAULT '{}',
ADD COLUMN points INT[] DEFAULT '{}',
ADD COLUMN max_options_in_answer INT DEFAULT 1;
-- Установка ключа
ALTER TABLE questions 
ADD CONSTRAINT questions_pkey PRIMARY KEY (id, version);
-- Функция настройки версии и айди
CREATE OR REPLACE FUNCTION public.auto_increment_question()
RETURNS TRIGGER AS $$
BEGIN
    -- Если Id не указан ИЛИ не существует — берём следующий свободный Id
    IF NEW.id IS NULL OR NOT EXISTS (SELECT 1 FROM questions WHERE id=NEW.id) THEN
        SELECT COALESCE(MAX(id), 0) + 1 INTO NEW.id FROM questions;
		SELECT 1 INTO NEW.version;
	ELSE
		-- Находим максимальную Version для этого Id и устанавливаем: если записей нет — 1, иначе max + 1
		SELECT COALESCE(MAX(version), 0) + 1 INTO NEW.version FROM questions WHERE id=NEW.id;
    END IF;
	
	RETURN NEW;
END;
$$ LANGUAGE plpgsql;
CREATE TRIGGER trigger_auto_increment_question
BEFORE INSERT ON questions
FOR EACH ROW
EXECUTE FUNCTION public.auto_increment_question();


ALTER TABLE users
ADD COLUMN id VARCHAR(20) NOT NULL PRIMARY KEY,
ADD COLUMN status VARCHAR(20) DEFAULT 'Offline',
ADD COLUMN first_name VARCHAR(20) NOT NULL,
ADD COLUMN last_name VARCHAR(20) DEFAULT '',
ADD COLUMN patronimyc VARCHAR(20) DEFAULT '',
ADD COLUMN roles TEXT[] DEFAULT '{"Student"}';
ADD COLUMN nickname VARCHAR(20) NOT NULL,
ADD COLUMN email VARCHAR(100) NOT NULL;

ALTER TABLE disciplines
ADD COLUMN id INT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
ADD COLUMN title VARCHAR(100),
ADD COLUMN describtion VARCHAR(500),
ADD COLUMN status VARCHAR(20) DEFAULT 'Exists',
ADD COLUMN students TEXT[] DEFAULT '{}',
ADD COLUMN teachers TEXT[] DEFAULT '{}',
ADD COLUMN tests INT[] DEFAULT '{}';

ALTER TABLE tests
ADD COLUMN id INT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
ADD COLUMN title VARCHAR(50) NOT NULL,
ADD COLUMN describtion VARCHAR(250) NOT NULL,
ADD COLUMN status VARCHAR(20) DEFAULT 'Deactivated',
ADD COLUMN author VARCHAR(20) NOT NULL,
ADD COLUMN max_tries_for_student INT DEFAULT 1,
ADD COLUMN questions_signatures INT[][2] DEFAULT '{}',
ADD COLUMN tries INT[] DEFAULT '{}';

ALTER TABLE news
ADD COLUMN id INT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
ADD COLUMN status VARCHAR(20) DEFAULT 'Sended', -- Sended, Viewed, Deleted
ADD COLUMN sender VARCHAR(20) NOT NULL,
ADD COLUMN recipient VARCHAR(20) NOT NULL,
ADD COLUMN text VARCHAR(200) DEFAULT '';


