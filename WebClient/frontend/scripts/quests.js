const web_server_url = "http://localhost:3000";

// Для открытия страниц
function open_ref(ref) {
    window.location.href = ref;
}
function open_webclient_ref(ref) {
    window.location.href = `${web_server_url}/${ref}`;
}

// В целом общий метод отправки запросов
async function send_request(url, method, params = {}, body = {}) {
    // Формируем строку параметров
    const query = new URLSearchParams(params).toString();
    // Полная строка ссылки
    const full_url = url + ((query == "") ? "" : ("?" + query));
    // Части запроса
    const headers = {
        "Content-Type": "application/json"
    };
    const bodyJSON = JSON.stringify(body);
    const credentials = "include"; // для передачи куки

    // Выбираем способ в зависимости от наличии тела
    const request_data = bodyJSON == "{}" ? {
        method: method,
        headers: headers,
        credentials: credentials
    } : {
        method: method,
        headers: headers,
        credentials: credentials,
        body: bodyJSON
    };

    // Отправка и обработка
    console.log("Отправка данных по URL: " + full_url + "...");
    try {
        // Отправляем запрос
        const response = await fetch(full_url, request_data);

        // Сохраняем HTTP-статус
        const response_status = response.status;

        let response_json;
        try {
            // Парсим JSON
            response_json = await response.json();
            console.log("Нормальный.");
        } catch (error_json) {
            // Ошибка парсинга JSON
            console.log("Тела ответа не будет, оно приняло ислам.");
            response_json = {};
        }

        // Возвращаем массив: [статус, JSON-данные]
        return [response_status, response_json];

    } catch (error) {
        // Сетевые ошибки (нет интернета, неверный URL...)
        console.error('Сеть недоступна:', error.message);
        send_notification("error", "Ошибка сети.");
        return [undefined, undefined];
    }
}

// Общий метод отправки post
async function post_to(url, params = {}, body = {}) {
    const [status, response] = await send_request(url, "POST", params, body);
    return [status, response];
}
// Отправка пост запросов к Web Client"у (возвращает response)
async function post_to_webclient(page, params = {}, body = {}) {
    const target_url = `${web_server_url}/${page}`;
    const [status, response] = await post_to(target_url, params, body);
    return [status, response];
}

// Общий метод отправки get
async function get_from(url, params = {}) {
    const [status, response] = await send_request(url, "GET", params, {});
    return [status, response];
}
// Отправка гет запросов к Web Client"у (возвращает response)
async function get_from_webclient(page, params = {}) {
    const target_url = `${web_server_url}/${page}`;
    const [status, response] = await get_from(target_url, params);
    return [status, response];
}