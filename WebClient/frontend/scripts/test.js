// Контейнер информации
let dataContainer;

// Информация о дисциплине
let testTitleText;
let testDescText;
let testStatusText;

function set_title(title) {
    testTitleText.textContent = title;
}
function set_describtion(desc) {
    testDescText.textContent = desc;
}
function set_status(status) {
    switch (status) {
        case "Activated":
            testStatusText.textContent = "Активирован";
            testStatusText.classList = "status activated";
            changeStatusButton.style.display = "inline-block";
            changeStatusButton.textContent = "Деактивировать"
            changeStatusButton.classList = "button change status deactivate"
            break;
        case "Deactivated":
            testStatusText.textContent = "Деактивирован";
            testStatusText.classList = "status deactivated";
            changeStatusButton.style.display = "inline-block";
            changeStatusButton.textContent = "Активировать"
            changeStatusButton.classList = "button go status activate"
            break;
        case "Deleted":
            testStatusText.textContent = "Удалён";
            testStatusText.classList = "status deleted";
            changeStatusButton.style.display = "none";
            break;
    }
}

// Создание
// Вопросов
// Редактируемые
function createModifyQuestion(number, id, version, title, options, points, max_options, author_id) {
    const newQuestion = document.createElement("div");
    newQuestion.classList = "question-item modify";
    newQuestion.question_id = id;
    newQuestion.question_version = version;
    newQuestion.author_id = author_id;
    newQuestion.innerHTML =
            `<div class="question-modify-header">
                <div class="question-modify-number">Вопрос ${number}.</div>
                <input type="text" class="question-modify-input title" value="${title}" placeholder="Введите название вопроса">
                <input type="number" class="question-modify-input variants-count" value="${max_options}" min="0" placeholder="Ответов">  
                <div class="question-modify-controls">
                    <button class="button question move-up">↑</button>
                    <button class="button question move-down">↓</button>
                    <button class="button question delete">🗑️</button>
                </div>
            </div>
            <div class="question-modify-options"></div>
            <button class="button question-modify-add-option">+ Добавить ответ</button>`;

    const optionsContainer = newQuestion.querySelector(".question-modify-options");
    options.forEach((option, index) => {
        const optionItem = createModifyOption(option, points[index]);
        optionsContainer.append(optionItem);
    });
    // Обработчик стрелки вверх вопроса
    newQuestion.querySelector(".button.question.move-up").addEventListener("click", () => {
        // Получаем индекс элемента
        const children = Array.from(dataContainer.children);
        // Получаем индекс элемента
        const index = children.indexOf(newQuestion);
        if (index == 0) return;
        // Переставляет
        const upperQuestion = children[index - 1];
        dataContainer.insertBefore(newQuestion, upperQuestion);
        // Обновляет номера вопросов
        upperQuestion.querySelector(".question-modify-number").textContent = `Вопрос ${index + 1}.`;
        newQuestion.querySelector(".question-modify-number").textContent = `Вопрос ${index}.`;
    });
    // Обработчик стрелки вниз вопроса
    newQuestion.querySelector(".button.question.move-down").addEventListener("click", () => {
        // Получаем индекс элемента
        const children = Array.from(dataContainer.children);
        const index = children.indexOf(newQuestion);
        const childrenCount = children.length;
        if (index == childrenCount - 1) return;
        // Переставляет
        const lowerOption = children[index + 1];
        dataContainer.insertBefore(lowerOption, newQuestion);
        // Обновляет номера вопросов
        lowerQuestion.querySelector(".question-modify-number").textContent = `Вопрос ${index + 1}.`;
        newQuestion.querySelector(".question-modify-number").textContent = `Вопрос ${index + 2}.`;
    });
    // Обработчик удаления
    newQuestion.querySelector(".button.question.delete").addEventListener("click", () => {
        // Спрашиваем
        const confirmDelete = confirm("Уверены, то хотите удалить вопрос?");
        if (!confirmDelete) return;
        // Изменяем номера вопросов
        const children = Array.from(dataContainer.children);
        let number = 0;
        children.forEach((question) => {
            if (newQuestion != question) {
                number++;
                question.querySelector(".question-modify-number").textContent = `Вопрос ${number}.`;
            }
        });
        // Удаляем
        newQuestion.remove();
    });
    // Обработчик добавления опции
    newQuestion.querySelector(".button.question-modify-add-option").addEventListener("click", () => {
        const newOption = createModifyOption("", "");
        optionsContainer.append(newOption);
        // Изменяет флаг
        newQuestion.modified = true;
    });
    // Добавляет обработчик любому изменению полей
    newQuestion.querySelectorAll(".question-modify-input").forEach(input => {
        input.addEventListener("input", () => {
            // Изменяет флаг
            newQuestion.modified = true;
        });
    });
    return newQuestion;
}
function createModifyOption(text, points) {
    const newOption = document.createElement("div");
    newOption.classList = "question-option-item modify";
    newOption.innerHTML =
            `<textarea class="question-modify-input option-text" placeholder="Вариант ответа">${text}</textarea>
            <input type="number" class="question-modify-input option-points" value="${points}" min="0" placeholder="Очки">  
            <div class="question-modify-controls">
                <button class="button option move-up">↑</button>
                <button class="button option move-down">↓</button>
                <button class="button option delete">🗑️</button>
            </div>`;

    // Обработчик стрелки вверх опции
    newOption.querySelector(".button.option.move-up").addEventListener("click", () => {
        // Находим контейнер
        const container = newOption.closest(".question-modify-options");
        const children = Array.from(container.children);
        // Получаем индекс элемента
        const index = children.indexOf(newOption);
        if (index == 0) return;
        // Переставляет
        const upperOption = children[index - 1];
        container.insertBefore(newOption, upperOption);
        // Изменяет флаг
        newOption.closest(".question-item.modify").modified = true;
    });
    // Обработчик стрелки вниз опции
    newOption.querySelector(".button.option.move-down").addEventListener("click", () => {
        // Находим контейнер
        const container = newOption.closest(".question-modify-options");
        const children = Array.from(container.children);
        // Получаем индекс элемента
        const index = children.indexOf(newOption);
        const childrenCount = children.length;
        if (index >= childrenCount - 1) return;
        // Переставляет
        const lowerOption = children[index + 1];
        container.insertBefore(lowerOption, newOption);
        // Изменяет флаг
        newOption.closest(".question-item.modify").modified = true;
    });
    // Обработчик удаления опции
    newOption.querySelector(".button.option.delete").addEventListener("click", () => {
        // Изменяет флаг
        newOption.closest(".question-item.modify").modified = true;
        // Удаляем
        newOption.remove();
    });

    return newOption;
}
// Решаемые
function createSolveQuestion(number, answer_id, title, options, max_options) {
    const newQuestion = document.createElement("div");
    newQuestion.classList = "question-item solve";
    newQuestion.answer_id = answer_id;
    newQuestion.innerHTML = generateSolveQuestionHTMLInner(number, title, options);
    // Находит все выделители и кнопки
    const selecters = newQuestion.querySelectorAll(".question-solve-option-selecter");
    const saveButton = newQuestion.querySelector(".button.go");
    const deleteButton = newQuestion.querySelector(".button.delete");
    saveButton.style.display = "none";
    deleteButton.style.display = "none";
    // Обработчик выбора для каждого селектора
    selecters.forEach((selecter) => {
        selecter.addEventListener("click", () => {
            // Находим кол-во чеков
            let checksCount = 0;
            selecters.forEach((item) => {
                if (item.checked) checksCount++;
            });
            console.log(checksCount, "/", max_options);
            // Защита от большего чем нужно кол-ва ответов
            if (checksCount > max_options) selecter.checked = false;
            // Открывает кнопку, если ответ удачно изменён
            else {
                saveButton.style.display = "inline-block";
            }
        });
    });
    // Обработчик сохранения ответа
    saveButton.addEventListener("click", async () => {
        // Находит выбранные опции
        let options = [];
        selecters.forEach((selecter) => {
            if (selecter.checked) {
                const option = selecter.dataset.order;
                options.push(option);
            }
        });
        // Отправляет запрос
        const [status, response] = await post_to_webclient("data/write",
            { source: "tries/answer/change" },
            { answer_id: answer_id, options: options });
        // Обрабатывает ошибку
        if (status != 200) {
            send_notification("error", response && response["message"] ?
                response["message"] : "Не удалось сохранить ответ.");
            return;
        }
        // Сообщаем
        send_notification("success", "Ответ сохранён");
        // Переклюаем кнопки
        saveButton.style.display = "none";
        deleteButton.style.display = "inline-block";
    });
    // Обработчик удаления ответа
    deleteButton.addEventListener("click", async () => {
        // Отправляет запрос
        const [status, response] = await post_to_webclient("data/write",
            { source: "tries/answer/change" },
            { answer_id: answer_id, options: [] });
        // Обрабатывает ошибку
        if (status != 200) {
            send_notification("error", response && response["message"] ?
                response["message"] : "Не удалось удалить ответ.");
            return;
        }
        // Сообщаем
        send_notification("success", "Ответ удалён");
        // Удаляем у себя
        selecters.forEach((selecter) => {
            selecter.checked = false;
        });
        // Скрываем кнопки
        deleteButton.style.display = "none";
        saveButton.style.display = "none";
    });

    return newQuestion;
}
// Ответов
// Обычные
function createAnswer(number, question_title, question_options, selected_options, points, max_points) {
    const newAnswer = document.createElement("div");
    newAnswer.classList = "answer-item";
    newAnswer.innerHTML = generateAnswerHTMLInner(number, question_title, question_options, selected_options, points, max_points);
    return newAnswer;
}
// Формировка
// Вопросов
// Решаемые
function generateSolveQuestionHTMLInner(number, title, options) {  // Вопрос целиком
    // Начинаем формировать HTML
    let html = `<div class="question-solve-number">Вопрос ${number}</div>
                <div class="question-solve-title">${title}</div>
                <div class="question-solve-options">`;
                    // Для каждого ответа создаём блок question-solve-option-item
                    options.forEach((option, index) => {
                        html += `<label class="question-option-item solve">
                                <input 
                                    type="checkbox"
                                    class="question-solve-option-selecter"
                                    data-question="${number}" data-order="${index + 1}">
                                <span class="question-solve-option-text">${option}</span>
                                </label>`;
                    });
        // Закрываем контейнер, добавляя кнопки
        html += `
                <button class="button go">Сохранить ответ</button>
                <button class="button delete">Сбросить ответ</button>
                </div>
            `;
    return html;
}
// Ответов
// Обычных
function generateAnswerHTMLInner(number, question_title, question_options, selected_options, points, max_points) {
    // Определяем цвет индикатора баллов
    let status;
    if (points === max_points) status = "succes"; // Максимум
    else if (points > 0) status = "medium"; // Промежуточное значение
    else status = "fail"; // 0 баллов

    // Создаём заголовок
    let html = `
            <div class="answer-header">
                <div class="answer-number">Вопрос ${number}.</div>
                <div class="answer-title">${question_title}</div>
            </div>
            <div class="answer-options">`;
    // Формируем варианты ответа
    question_options.forEach((option, index) => {
        const selected = selected_options.includes(index + 1);
        html += `
                <div class="answer-option ${selected ? "selected" : ""}">
                    <div class="answer-option-indicator"></div>
                    <span class="answer-option-text">${option}</span>
                </div>`;
    });
    // Добавляем результат
    html += `</div>
            <div class="answer-score-container">
                <div class="answer-score ${status}">Баллов: ${points} из ${max_points}.</div>
            </div>`;

    return html;
}

document.addEventListener("DOMContentLoaded", async () => {
    // Переменные
    // Получение ключевых элементов DOM
    // Контент теста
    const testContainer = document.getElementById("testContainer");
    // Контейнер информации
    dataContainer = document.getElementById("dataContainer");
    // Изменение информации
    testTitleText = document.getElementById("testTitleText");
    testDescText = document.getElementById("testDescText");
    testStatusText = document.getElementById("testStatusText");
    const testTitleInput = document.getElementById("testTitleInput");
    const testDescInput = document.getElementById("testDescInput");
    
    const editInfoButton = document.getElementById("editInfoButton");
    const changeStatusButton = document.getElementById("changeStatusButton");
    const deleteTestButton = document.getElementById("deleteTestButton");
    // Кнопки действий
    const solveTestButton = document.getElementById("solveTestButton");
    const modifyTestButton = document.getElementById("modifyTestButton");
    // Кнопки редактирования
    const cancelTestChangesButton = document.getElementById("cancelTestChangesButton");
    const addQuestionButton = document.getElementById("addQuestionButton");
    const saveTestButton = document.getElementById("saveTestButton");
    // Кнопки решения
    const finishTestButton = document.getElementById("finishTestButton");
    // Оверлэй и кнопки модального окна
    const modalOverlay = document.getElementById("modalOverlay");
    const modalContent = document.getElementById("modalContent");
    const modalTitle = document.getElementById("modalTitle");
    const modalCloseButton = document.getElementById("modalCloseButton");
    const modalAddQuestionButton = document.getElementById("modalAddQuestionButton");
    const modalAddnewQuestionButton = document.getElementById("modalAddnewQuestionButton");
    const modalSaveInfoButton = document.getElementById("modalSaveInfoButton");
    // Поля ввода модалки
    const questionIdInput = document.getElementById("questionIdInput");
    // Попытки
    const toggleTriesButton = document.getElementById("toggleTriesButton");
    const triesList = document.getElementById("triesList");

    console.log("Тест");
    set_section_toggle(triesList, toggleTriesButton, "попытки прохождения", false);

    // Если передано айди попытки - устанавливает сообветсвующий режим и пытается её показать
    if (load_data["try_id"]) {
        // Ставим режим контейнеру
        testContainer.classList = "test-container mode-observing";

        // Получаем данные
        const [answers_status, answers_response] = await get_from_webclient("data/read",
            { source: "tries/view", try_id: load_data["try_id"] });
        // Возвращается: status, points, max_points, score_percent, answers
        // answers - массив с question_id, question_version, question_title, question_options,
        // selected_options, points, max_points

        // Проверяем на ошибку
        if (answers_status != 200 || !answers_response) {
            send_notification("error", answers_response["message"] ? answers_response["message"] : "Ошибка при получении ответов.");
        }
        // В случае удачи заполняем список ответами
        else {
            // Очищаем список
            dataContainer.innerHTML = "";
            // Наполняем полученными из запроса
            let number = 0;
            for (const answer of answers_response["answers"]) {
                number++;
                // Создаёт и Добавляет
                const item = createAnswer(number,
                    answer["question_title"], answer["question_options"], answer["selected_options"],
                    answer["points"], answer["max_points"]);
                dataContainer.append(item);
            }
            // Создаёт результат
            const try_result = document.createElement("div");
            try_result.classList = "question-item result";
            try_result.innerHTML = 
                `<div class="answer-score-container">
                    <div class="answer-score">Баллов за тест: ${answers_response["points"]} из ${answers_response["max_points"]}.</div>
                </div>`;
            dataContainer.append(try_result);
        }
    }
    // Иначе устанавливает обычный режим
    else {
        // Ставим режим контейнеру
        testContainer.classList = "test-container mode-preview";
    }

    // Иницилизация информации - через запрос
    const [info_response_status, info_response] = await get_from_webclient("data/read",
        { source: "tests/data", test_id: load_data["test_id"], filter: "text, status" });
    if (info_response_status != 200 || !info_response) open_webclient_ref("primary");
    else {
        set_title(info_response["title"]);
        set_describtion(info_response["describtion"]);
        set_status(info_response["status"]);
    }

    // Настройка кнопок
    // Информации
    // Изменение
    editInfoButton.addEventListener("click", async () => {
        modalOverlay.style.display = "flex";
        modalContent.classList = "modal-content mode-edit";
        modalTitle.textContent = "Редактирование информации";
        testTitleInput.value = testTitleText.textContent;
        testDescInput.value = testDescText.textContent;
    });
    changeStatusButton.addEventListener("click", async () => {
        // Выбирает нужный статус
        const target_status = changeStatusButton.classList.contains("activate") ? "Activated" : "Deactivated";
        // Отправляет запрос
        const [status, response] = await post_to_webclient("data/write",
            { source: "tests/status" },
            { test_id: load_data["test_id"], status: target_status });
        // Обрабатывает ошибку
        if (status != 200) {
            send_notification("error", response && response["message"] ? response["message"] : "Ошибка при изменении статуса теста");
            return;
        }
        send_notification("success", "Статус успешно изменён.");
        // Устанавливает нужный статус
        set_status(target_status);
    });
    deleteTestButton.addEventListener("click", async () => {
        // Подтверждаем
        const confirmed = confirm(`Вы действительно хотите удалить тест?`);
        if (!confirmed) return;
        // Отправляем запрос
        const [status, response] = await post_to_webclient("data/write",
            { source: "tests/status" },
            { test_id: load_data["test_id"], status: "Deleted" });
        // Обрабатывает ошибку
        if (status != 200) {
            send_notification("error", response && response["message"] ? response["message"] : "Ошибка при удалении студента.");
            return;
        }
        // Удаляет из списка
        send_notification("success", "Тест удалён!");
        setTimeout(() => {
            open_webclient_ref("primary");
        }, 1000);
    });
    // Выбор и старт режима
    // Редактирование
    modifyTestButton.addEventListener("click", async () => {
        // Получаем данные
        const [status, response] = await get_from_webclient("data/read",
            { source: "tests/data", test_id: load_data["test_id"], filter: "questions has_tries" });
        // Проверяем на ошибку
        if (status != 200) {
            send_notification("error", response["message"] ? response["message"] : "Ошибка при получении вопросов.");
            return;
        }
        // Возвращается: questions массив с question_id, question_version, title, options, points, max_options, author_id
        // А также has_tries

        if (response["has_tries"] === true || response["has_tries"] == 1) {
            send_notification("error", "Нельзя редактировать тест, который уже был пройден однажды.");
        }

        // Очищаем хранилище-список
        dataContainer.innerHTML = "";
        // Наполняем полученными из запроса
        let number = 0;
        for (const question of response["questions"]) {
            number++;
            // Создаёт и Добавляет
            const item = createModifyQuestion(number, question["question_id"], question["question_version"],
                question["title"], question["options"], question["points"], question["max_options"], question["author_id"]);
            dataContainer.append(item);
        }
        // Уведомляет
        testContainer.classList = "test-container mode-modify";
        send_notification("success", "Тест успешно загружен");
    });
    // Решение
    solveTestButton.addEventListener("click", async () => {
        // Отправляем запрос на создание попытки
        const [status, response] = await post_to_webclient("data/write",
            { source: "tries/start" }, { test_id: load_data["test_id"] });
        // Проверяем на ошибку
        if (status != 200) {
            send_notification("error", response["message"] ? response["message"] : "Ошибка при создании попытки.");
            return;
        }
        // Возвращает: try_id и questions: answer_id, question_id, question_version, title, options, points, max_options_in_answer

        // Очищаем хранилище-список
        dataContainer.innerHTML = "";
        // Наполняем полученными из запроса
        let number = 0;
        for (const question of response["questions"]) {
            number++;
            // Создаёт и Добавляет
            const item = createSolveQuestion(number, question["answer_id"],
                question["title"], question["options"], question["max_options"]);
            dataContainer.append(item);
        }
        // Устанавливает айди попытки в контейнер
        dataContainer.try_id = response["try_id"];
        testContainer.classList = "test-container mode-solve";
        // Уведомляет
        send_notification("success", "Тест начат!");
    });
    // Редактирования
    // Отмены
    cancelTestChangesButton.addEventListener("click", () => {
        dataContainer.innerHTML = "";
        testContainer.classList = "test-container mode-preview";
        send_notification("success", "Изменения отменены.");
    });
    // Добавление вопроса
    addQuestionButton.addEventListener("click", async () => {
        modalOverlay.style.display = "flex";
        modalContent.classList = "modal-content mode-question";
        modalTitle.textContent = "Добавление вопроса";
        questionIdInput.value = "";
    });
    // Сохранение вопросов (теста в целом)
    saveTestButton.addEventListener("click", async () => {
        // Узнаём, если у теста попытки прохождения, чтобы не сохранять лишний раз вопросы
        const [check_status, check_response] = await get_from_webclient("data/read",
            { source: "tests/data", test_id: load_data["test_id"], filter: "has_tries" });
        // Проверяем на ошибку
        if (check_status != 200) {
            send_notification("error", check_response["message"] ? check_response["message"] : "Ошибка при получении сведений о тесте.");
            return;
        }
        // Возвращается: has_tries
        if (check_response["has_tries"] === true || check_response["has_tries"] == 1) {
            send_notification("error", "Нельзя редактировать тест, который уже был пройден однажды.");
        }
        
        // Проверяет на отсутствие вопросов
        const questions = dataContainer.children;
        if (questions.childElementCount == 0) {
            send_notification("error", "Список вопросов не может быть пустым!");
            return;
        }
        // Для каждого вопроса Делает запрос на сохранение
        for (const question of questions) {
            // Скип, если не менялся
            if (question.modified) {
                // Проверяет на кол-во опций ответов
                const options = question.querySelectorAll(".question-option-item.modify");
                if (options.length == 0) {
                    send_notification("error", "Вопросы должны содержать минимум один вариант ответа.");
                    return;
                }

                // Проверяет заполнение всех полей вопроса
                let has_empty = false;
                question.querySelectorAll(".question-modify-input").forEach(input => {
                    if (input.value == "") {
                        has_empty = true;
                    }
                });
                if (has_empty) {
                    send_notification("error", "Вопросы не могут содержать пустых полей.");
                    return;
                }

                // Собираем информацию о вопросе
                let data = { title: "", options: [], points: [], max_options: 1 };
                data["title"] = question.querySelector(".question-modify-input.title").value;
                data["max_options"] = question.querySelector(".question-modify-input.variants-count").value;
                for (let i = 0; i < options.length; i++) {
                    data["options"][i] = options[i].querySelector(".question-modify-input.option-text").value;
                    data["points"][i] = options[i].querySelector(".question-modify-input.option-points").value;
                }

                // Если айди, на который ссылается вопрос, указан и автор совпадает
                if (question.question_id && question.question_version && question.author_id == load_data.viewer["id"]) {
                    console.log("Вопрос старый и изменён автором", question.question_id, question.question_version, ", отправляем запрос на обновление", data);
                    // Отправляет запрос на обновление вопроса
                    const [status, response] = await post_to_webclient("data/write",
                        { source: "questions/update" },
                        { question_id: question.question_id, ...data });
                    
                    // Обрабатывает другую ошибку
                    if (status != 200) {
                        send_notification("error", response && response["message"] ?
                            response["message"] : "Не удалось сохранить тест.");
                        return;
                    }
                    // В случае успеха меняет версию вопроса
                    question.question_version = response["question_version"];
                    console.log(response);
                }
                else {
                    console.log("Вопрос новый, либо изменён не своим автором, отправляем запрос на создание", data);
                    // Отправляет запрос на создание вопроса
                    const [status, response] = await post_to_webclient("data/write",
                        { source: "questions/create" },
                        { ...data });
                    // Обрабатывает ошибку
                    if (status != 200) {
                        send_notification("error", response && response["message"] ?
                            response["message"] : "Не удалось сохранить тест.");
                        return;
                    }
                    // В случае успеха меняет сигнатуру вопроса
                    question.question_id = response["question_id"];
                    question.question_version = response["question_version"]; // 1
                    console.log(response);
                }

                // Сбросить флаг
                question.modified = false;
            }
        }

        // Получает новую последовательность вопросов
        let questions_signatures = [];
        for (const question of questions) {
            const sign = [question.question_id, question.question_version];
            if (!question.question_id || !question.question_version) {
                send_notification("error", "Ошибка при изменении вопросов теста.")
                return;
            } 
            questions_signatures.push(sign);
        }

        // Устанавливаем тесту новую последовательность вопросов
        const [change_status, change_response] = await post_to_webclient("data/write",
            { source: "tests/questions/update" },
            { test_id: load_data["test_id"], questions_signatures: questions_signatures });
        // Проверяем на ошибку
        if (change_status != 200) {
            send_notification("error", change_response["message"] ? change_response["message"] : "Ошибка при получении сведений о тесте.");
            return;
        }

        // Очищает
        dataContainer.innerHTML = "";
        testContainer.classList = "test-container mode-preview";
        // После того, как все вопросы сохранены вместе с новой последоватлельностью, уведомляем
        send_notification("success", "Тест успешно сохранён.");
    });
    // Решения  
    // Окончания теста
    finishTestButton.addEventListener("click", async () => {
        // Для каждого вопроса Делает запрос на сохранение
        const questions = dataContainer.children;
        for (const question of questions) {
            // Скип, если не требует сохранения
            if (question.querySelector(".button.go").style.display !== "none") {
                // Находит выбранные опции
                let options = [];
                question.querySelectorAll(".question-solve-option-selecter").forEach((selecter) => {
                    if (selecter.checked) {
                        const option = selecter.dataset.order;
                        options.push(option);
                    }
                });
                // Отправляет запрос
                const [status, response] = await post_to_webclient("data/write",
                    { source: "tries/answer/change" },
                    { answer_id: question.answer_id, options: options });
                // Обрабатывает ошибку
                if (status != 200) {
                    send_notification("error", response && response["message"] ?
                        response["message"] : "Не удалось сохранить тест.");
                    return;
                }
            }
        }

        // Закончить попытку, Отправляет запрос
        const [status, response] = await post_to_webclient("data/write",
            { source: "tries/stop" },
            { try_id: dataContainer.try_id });
        // Обрабатывает ошибку
        if (status != 200) {
            send_notification("error", response && response["message"] ?
                response["message"] : "Не удалось закончить тест.");
            return;
        }

        // Очистить содержимое
        dataContainer.innerHTML = "";
        dataContainer.try_id = undefined;
        testContainer.classList = "test-container mode-preview";
        // Уведомить
        send_notification("success", "Тест успешно завершён");
    });
    // Модали
    // Сохранение изменений информации теста
    modalSaveInfoButton.addEventListener("click", async () => {
        const title = testTitleInput.value;
        const desc = testDescInput.value;
        if (title == "" || desc == "") {
            send_notification("error", "Нельзя оставлять поля пустыми!");
            return;
        }

        // Отправка запроса
        const [status, response] = await post_to_webclient("data/write",
            { source: "test/text" },
            { test_id: load_data["test_id"], title: title, describtion: desc });
        // Обрабатывает ошибку
        if (status != 200) {
            send_notification("error", response && response["message"] ?
                response["message"] : "Ошибка при изменении информации теста.");
            return;
        }

        // Устанавливаем
        set_title(title);
        set_describtion(desc);
        // Закрываем
        modalOverlay.style.display = "none";
        send_notification("success", "Информация теста обновлена!");
    });
    // Создание нового вопроса и его добавление (Создание только на странице. Создание на сервере происходит при сохранении)
    modalAddnewQuestionButton.addEventListener("click", async () => {
        // Добавляет пустой вопрос
        const item = createModifyQuestion(dataContainer.childElementCount + 1, undefined, undefined, "", ["", ""], ["", ""], 1);
        item.modified = true;
        dataContainer.append(item);

        // Закрываем
        modalOverlay.style.display = "none";
        send_notification("success", "Новый вопрос добавлен!");
    });
    // Добавление существующего вопроса
    modalAddQuestionButton.addEventListener("click", async () => {
        // Проверка ввода и создание сигнатуры
        let signature = questionIdInput.value;
        if (signature.contains(".")) {
            const parts = signature.split(".");
            if (parts != 2 || isNaN(Number(parts[0])) || isNaN(Number(parts[1]))) {
                send_notification("error", "Неверный формат ввода!");
                return;
            }
            signature = { question_id: Number(parts[0]), question_version: Number(parts[1]) };
        }
        else {
            if (isNaN(Number(signature))) {
                send_notification("error", "Неверный формат ввода!");
                return;
            }
            signature = { question_id: signature };
        }

        // Получение данных
        const [status, response] = await get_from_webclient("data/read",
            { source: "questions/data", ...signature, filter: "title status options points max_options" });
        // Обрабатывает ошибку
        if (status != 200) {
            send_notification("error", response && response["message"] ?
                response["message"] : "Ошибка при получении информации о вопросе.");
            return;
        }
        // Добавляем вопрос
        dataContainer.append(createModifyQuestion(dataContainer.childElementCount + 1, signature["question_id"], response["version"],
            response["title"], response["options"], response["points"], response["max_options"]));

        // Закрываем
        modalOverlay.style.display = "none";
        send_notification("success", "Вопрос добавлен!");
    });
    // Закрытие модали
    modalCloseButton.addEventListener("click", async () => {
        modalOverlay.style.display = "none";
    });
    // Попыток
    // Открытие/скрытие списка попыток
    toggleTriesButton.addEventListener("click", async () => {
        if (triesList.is_open == false) {
            // Получаем данные
            const [status, response] = await get_from_webclient("data/read",
                { source: "tests/data", test_id: load_data["test_id"], filter: "trieslist" });
            // Проверяем на ошибку
            if (status != 200) {
                send_notification("error", response["message"] ? response["message"] : "Ошибка при получении информации.");
                return;
            }
            // Возвращается: tries массив с try_id, status, points, max_points, score_percent, author_id, author_fullname

            // Очищаем список
            triesList.innerHTML = "";
            // Наполняем полученными из запроса
            for (const tri of response["tries"]) {
                const item = document.createElement("div");
                item.classList = "section-item try";
                item.innerHTML =
                    `<div class="user-info">
                    <span class="section-text name">${tri["author_fullname"]} (${tri["author_id"]})</span>
                    <span class="section-text score">Набрано очков: ${tri["points"]}/${tri["max_points"]} (${tri["score_percent"]}%)</span>
                </div>
                <div class="buttons">
                    ${tri["status"] === "Solved" ? `<button class="button go answers">Ответы</button>` : ""}
                    <button class="button go profile">Профиль</button>
                </div>`;
                // Обработчик профиля
                item.querySelector(".button.go.profile").addEventListener("click", () => {
                    open_webclient_ref("cabinet?user_id=" + tri["author_id"]);
                });
                // Обработчик Ответов
                if (tri["status"] === "Solved") item.querySelector(".button.go.answers").addEventListener("click", () => {
                    open_webclient_ref("test?test_id=" + load_data["test_id"] + "&try_id=" + tri["try_id"]);
                });
                // Добавление
                triesList.append(item);
            }

            // Открываем список
            set_section_toggle(triesList, toggleTriesButton, "попытки прохождения", true);
        } else {
            set_section_toggle(triesList, toggleTriesButton, "попытки прохождения", false);
        }
    });
})