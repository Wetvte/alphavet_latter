// Итемы секций
// Создание итема пользователя
function createUser(fullname, id) {
    const newItem = document.createElement("div");
    newItem.classList = "section-item user";
    newItem.innerHTML =
        `<div class="section-text user-name">${fullname} (${id})</div>
         <button class="button go">Профиль</button>`;
                // Ставит событие кнопке для перехода
                newItem.querySelector(".button.go").addEventListener("click", () => {
                    open_webclient_ref("cabinet?user_id=" + id);
                });
    return newItem;
}
// Создание итема вопроса
function createQuestion(id, version, title, status) {
    const newItem = document.createElement("div");

    let status_string;
    let status_class;
    switch (status) {
        case "Activated":
            status_string = "Активирован";
            status_class = "activated";
            break;
        case "Deactivated":
            status_string = "Активирован";
            status_class = "deactivated";
            break;
        case "Deleted":
            status_string = "Удалён";
            status_class = "deleted";
            break;
        default:
            status_string = "Неизвестный";
            status_class = "unkwonn";
            break;
    }

    newItem.classList = "section-item question";
    newItem.innerHTML =
        `<div class="section-text question-signature">${id}.(${version})</div>
        <div class="section-text question-title">${title}</div>
        <div class="section-text question-status ${status_class}">${status_string}</div>`;
    return newItem;
}
// Создание итема добавления дисциплины
function createDisciplineAdd() {
    const newItem = document.createElement("div");
    newItem.classList = "section-item discipline-add";
    newItem.innerHTML =
    `<span class="section-text">Добавить новую дисциплину</span>
     <span class="button add">Добавить дисциплину</span>`;
    newItem.querySelector(".button.add").addEventListener("click", (req, res) => {
        // Открывает модальное окно в режиме добавления дисциплины
        modalOverlay.style.display = "flex";
        modalContent.className = "modal-content mode-add-discipline";
        modalTitle.textContent = "Добавить дисциплину";
    });
    return newItem;
}
// Создание итема дисциплины
function createDiscipline(id, title) {
    const newItem = document.createElement("div");
    newItem.classList = "section-item discipline";
    newItem.innerHTML =
        `<span class="section-text discipline-title">${title} (${id})</span>
         <button class="button go">Посмотреть</button>`;
                // Ставит событие кнопке для перехода
                newItem.querySelector(".button.go").addEventListener("click", () => {
                    open_webclient_ref("discipline?discipline_id=" + id);
                });
    return newItem;
}

// При окончании загрузки
// Настройка страницы
// Имитация загрузки данных пользователя (в реальной системе — запрос к API)
document.addEventListener("DOMContentLoaded", async () => {
    // Все переменные - инициализация
    // Дисциплины
    const toggleDisciplinesButton = document.getElementById("toggleDisciplinesButton");
    const disciplinesList = document.getElementById("disciplinesList");
    // Пользователи
    const toggleUsersButton = document.getElementById("toggleUsersButton");
    const usersContainer = document.getElementById("usersContainer");
    // Вопросы
    const toggleQuestionsButton = document.getElementById("toggleQuestionsButton");
    const questionsContainer = document.getElementById("questionsContainer");
    // Модальное окно
    const modalOverlay = document.getElementById("modalOverlay");
    const modalCancelButton = document.getElementById("modalCancelButton");
    const modalAddDisciplineButton = document.getElementById("modalAddDisciplineButton");

    console.log("Главная");
    set_section_toggle(disciplinesList, toggleDisciplinesButton, "все дисциплины", false);
    set_section_toggle(usersContainer, toggleUsersButton, "всех пользователей", false);
    set_section_toggle(questionsContainer, toggleQuestionsButton, "все вопросы", false);

    // Переключение списка пользователей
    // Все пользователи
    toggleUsersButton.addEventListener("click", async () => {
        if (usersContainer.is_open == false) {
            // Получаем данные
            const [status, response] = await get_from_webclient("data/read", { source: "users/list" });
            // Проверяем на ошибку
            if (status != 200 || !response) {
                send_notification("error", response["message"] ? response["message"] : "Ошибка при получении пользователей.");
                return;
            }
            // Возвращается: users массив с user_id, first_name, last_name, patronimyc, fullname

            // Очищаем список 
            usersContainer.innerHTML = "";
            // Наполняем полученными из запроса
            for (const user of response["users"]) {
                // Создаёт
                const item = createUser(user["fullname"], user["user_id"]);
                // Добавляет
                usersContainer.append(item);
            }

            // Открываем список
            set_section_toggle(usersContainer, toggleUsersButton, "всех пользователей", true);
        } else {
            set_section_toggle(usersContainer, toggleUsersButton, "всех пользователей", false);
        }
    });
    // Переключение списка вопросов
    toggleQuestionsButton.addEventListener("click", async () => {
        if (questionsContainer.is_open == false) {
            // Получаем данные
            const [status, response] = await get_from_webclient("data/read", { source: "questions/list" });
            // Проверяем на ошибку
            if (status != 200 || !response) {
                send_notification("error", response["message"] ? response["message"] : "Ошибка при получении вопросов.");
                return;
            }
            // Возвращается: questions массив с question_id, question_version, title, status

            // Очищаем список 
            questionsContainer.innerHTML = "";
            // Наполняем полученными из запроса
            for (const question of response["questions"]) {
                // Создаёт
                const item = createQuestion(question["question_id"], question["question_version"], question["title"], question["status"]);
                // Добавляет
                questionsContainer.append(item);
            }

            // Открываем список
            set_section_toggle(questionsContainer, toggleQuestionsButton, "все вопросы", true);
        } else {
            set_section_toggle(questionsContainer, toggleQuestionsButton, "все вопросы", false);
        }
    });
    // Переключение списка дисциплин
    toggleDisciplinesButton.addEventListener("click", async () => {
        if (disciplinesList.is_open == false) {
            // Получаем данные
            const [status, response] = await get_from_webclient("data/read", { source: "disciplines/list" });
            // Проверяем на ошибку
            if (status != 200 || !response) {
                send_notification("error", response["message"] ? response["message"] : "Ошибка при получении дисциплин.");
                return;
            }
            // Возвращается: disciplines массив с discipline_id, title, status

            // Очищаем список дисциплин
            disciplinesList.innerHTML = "";
            disciplinesList.append(createDisciplineAdd());
            // Наполняем полученными из запроса
            for (const discipline of response["disciplines"]) {
                // Создаёт
                const item = createDiscipline(discipline["discipline_id"], discipline["title"]);
                // Добавляет
                disciplinesList.append(item);
            }

            // Открываем список
            set_section_toggle(disciplinesList, toggleDisciplinesButton, "все дисциплины", true);
        } else {
            set_section_toggle(disciplinesList, toggleDisciplinesButton, "все дисциплины", false);
            set_section_toggle(usersContainer, toggleUsersButton, "всех пользователей", false);
            set_section_toggle(questionsContainer, toggleQuestionsButton, "все вопросы", false);
        }
    });

    // Модальное окно редактирования
    // Добавление дисциплины
    modalAddDisciplineButton.addEventListener("click", async () => {
        // Получает данные и проверяет их
        const title = document.getElementById("addDisciplineTitle").value;
        if (!title) {
            alert("Укажите название!");
            return;
        }
        const teacher = document.getElementById("addDisciplineTeacher").value;
        if (!teacher) {
            alert("Укажите преподавателя!");
            return;
        }

        // Отправляет запрос
        const [status, response] = await post_to_webclient("data/write",
            { source: "disciplines/create" },
            { title: title, teacher_id: teacher });
        // Вернёт discipline_id

        // Если успешно
        if (status == 200) {
            // Сообщаем
            send_notification("success", "Успешно добавлена дисциплина: " + title);
            // Если открыты дисциплины, добавляем туда новую
            if (disciplinesList.style.display !== "none") {
                disciplinesList.append(createDiscipline(response["discipline_id"], title));
            }
            // Закрываем
            modalOverlay.style.display = "none";
        }
        // Если провал
        else {
            send_notification("error", response["message"] ? response["message"] : "Ошибка при добавлении дисциплины.");
        }
    });
    // Закрытие модалки
    modalCancelButton.addEventListener("click", () => {
        modalOverlay.style.display = "none";
    });
});





// Перенести
// Обработчики для кнопок "Удалить дисциплину"
document.querySelectorAll(".button-discipline-remove").forEach(button => {
    button.addEventListener("click", (e) => {
        const disciplineItem = e.target.closest(".discipline-item");
        const disciplineName = disciplineItem.querySelector(".discipline-title").textContent;

        // Подтверждение действия (можно убрать, если не нужно, но не нужно)
        const confirmDismiss = confirm(`Вы действительно хотите удалить дисциплину "${disciplineName}"?`);

        if (confirmDismiss) {
            // Плавное удаление элемента
            disciplineItem.style.opacity = "0";
            disciplineItem.style.transform = "translateX(20px)";

            setTimeout(() => {
                disciplineItem.remove();
                alert(`Дисциплина "${disciplineName}"" успешно удалена.`);
            }, 300);
        }
    });
});