/*
    Даёт доступ к пути сайта и функции отправки post запросов
*/

const web_server_url = "http://localhost:3000";
      
// Отправка пост запросов к Web Client"у (возвращает response)
async function send_data_to_webclient(page, data={}, token=undefined, error_callback=undefined) {
    const target_url = `${web_server_url}/${page}`;
    console.log("Отправка данных по URL: " + target_url)
    try {
        const response = await fetch(target_url, {
            method: "POST",
            headers: token ? {
            "Authorization": "Bearer " + token,
            "Content-Type": "application/json"
            } : {
            "Content-Type": "application/json"
            },
            body: JSON.stringify(data),
            credentials: "include" // для передачи куки
        });
        if (!response.ok) {
        if (error_callback) error_callback("Ошибка при приёме данных.");
        return;
        }
        return await response.json(); // Возвращаем json с данными, как ответ
    } catch (error) {
        if (error_callback) error_callback("Ошибка при приёме данных.");
    }
}