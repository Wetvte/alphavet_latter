// Все переменные
// Данные пользователя
let idText;
let nicknameText;
let fullnameText;
let emailText;
let rolesText;
let statusText;

// Отдельные методы
// Установка айди
function set_id(id, own) {
  idText.textContent = own ? (`Личный кабинет (${id})`) : (`Профиль пользователя (${id})`);
}
// Установка почты
function set_email(email) {
  emailText.textContent = email;
}
// Установка ника
function set_nickname(nickname) {
  nicknameText.textContent = nickname;
}
// Установка ФИО
function set_fullname(fullname) {
  fullnameText.textContent = fullname;
}
// Установка статуса
function set_status(status) {
  switch (status) {
    case "Online":
      statusText.textContent = "Онлайн";
      statusText.classList.classList = "user-info-value status online";
      break;
    case "Offline":
      statusText.textContent = "Оффлайн";
      statusText.classList = "user-info-value status offline";
      break;
    case "Blocked":
      statusText.textContent = "Заблокирован";
      statusText.classList = "user-info-value status blocked";
      break;
  }

  if (status == "Blocked") {
    blockButton.textContent = "Раблокировать";
    blockButton.classList = "button unblock";
  }
  else {
    blockButton.textContent = "Заблокировать";
    blockButton.classList = "button block";
  }
}
// Установка ролей
function set_roles(roles) {
  let roles_string = "";
  for (const role of roles) roles_string += roles_string == "" ? role : (", " + role);
  rolesText.textContent = roles_string;
}

// Создание итемов
// Новости
function createNews(news_id, sender_id, sender_fullname, text) {
  const newItem = document.createElement("div");
  newItem.classList = "news-item";
  newItem.innerHTML =
    `<span class="news-sender">${sender_fullname}<p>(${sender_id})</span>
   <span class="news-content">${text}</span>
   <button class="news-delete">Удалить</button>`;
  // Ставит событие кнопке для удаления
  newItem.querySelector(".news-delete").addEventListener("click", async () => {
    // Отправляет запрос на удаление новости
    const [delete_status, delete_response] = await post_to_webclient("data/write",
      { source: "news/status" },
      { news_id: news_id, status: "Deleted" });
    // Если успешно - выводим
    if (delete_status == 200) {
      send_notification("success", "Сообщение успешно удалено");
    }
    // Удаляем у себя
    newItem.style.display = "none";
    setTimeout(() => {
      newsItem.remove();
    }, 300);
  });
  return newItem;
}
// Создание итема дисциплины
function createDiscipline(id, title) {
  const newItem = document.createElement("div");
  newItem.classList = "section-item discipline";
  newItem.innerHTML =
    `<span class="section-text discipline-title">${title} (${id})</span>
   <button class="button go">Открыть</button>`;
  // Ставит событие кнопке для перехода
  newItem.querySelector(".button.go").addEventListener("click", () => {
    open_webclient_ref("discipline?discipline_id=" + id);
  });
  return newItem;
}
// Тесты
function createTest(test_id, test_title, discipline_id, discipline_title) {
  const newItem = document.createElement("div");
  newItem.classList = "section-item test";
  newItem.innerHTML =
    `<span class="section-text discipline-title">${discipline_title} (${discipline_id})</span>
     <span class="section-text title">${test_title} (${test_id})</span>
     <button class="button go">Открыть</button>`;
  // Ставит событие кнопке для перехода
  newItem.querySelector(".button.go").addEventListener("click", () => {
    open_webclient_ref("test?test_id=" + test_id);
  });
  return newItem;
}
// Оценки (Попытки прохождения)
function createGrade(test_id, try_id, test_title, result) {
  const newItem = document.createElement("div");
  newItem.classList = "section-item grade";
  newItem.innerHTML =
    `<span class="section-text title">${test_title}</span>
     <span class="section-text result">${result}</span>
     <button class="button go">Посмотреть</button>`;
  newItem.querySelector(".button.go").addEventListener("click", () => {
    open_webclient_ref("test?test_id=" + test_id + "&try_id="+try_id);
  });
  return newItem;
}

// По загрузке страницы
document.addEventListener("DOMContentLoaded", async () => {
  // Все переменные - инициализация
  // Данные пользователя
  idText = document.getElementById("idText");
  nicknameText = document.getElementById("nicknameText");
  fullnameText = document.getElementById("fullnameText");
  statusText = document.getElementById("statusText");
  rolesText = document.getElementById("rolesText");
  emailText = document.getElementById("emailText");
  // Кнопки выхода и удаления
  const logoutButton = document.getElementById("logoutButton");
  const logoutAllButton = document.getElementById("logoutAllButton");
  const deleteAccountButton = document.getElementById("deleteAccountButton");
  // Новостные сообщения
  const newsButton = document.getElementById("newsButton");
  const newsDropdown = document.getElementById("newsDropdown");
  const newsBadge = document.getElementById("newsBadge");
  // Дисциплины
  const toggleDisciplinesButton = document.getElementById("toggleDisciplinesButton");
  const disciplinesList = document.getElementById("disciplinesList");
  // Тесты
  const toggleTestsButton = document.getElementById("toggleTestsButton");
  const testsContainer = document.getElementById("testsContainer");
  // Попытки
  const toggleGradesButton = document.getElementById("toggleGradesButton");
  const gradesContainer = document.getElementById("gradesContainer");
  // Модаль
  const modalOverlay = document.getElementById("modalOverlay");
  const modalTitle = document.getElementById("modalTitle");
  const modalInput = document.getElementById("modalInput");
  const modalCancelButton = document.getElementById("modalCancelButton");
  const modalSaveButton = document.getElementById("modalSaveButton");
  // Кнопки Изменить в профиле
  const editButtons = document.querySelectorAll(".button.edit");
  const blockButton = document.getElementById("blockButton");

  console.log("Личный кабинет");
  // Настроить разделы
  newsDropdown.is_open = false;
  set_section_toggle(disciplinesList, toggleDisciplinesButton, "дисциплины", false);
  set_section_toggle(gradesContainer, toggleGradesButton, "оценки за тесты", false);
  set_section_toggle(testsContainer, toggleTestsButton, "пройденные тесты", false);

  // Информация профиля - инициализация через запрос
  const [profile_response_status, profile_response] = await get_from_webclient("data/read",
    { source: "users/data", user_id: load_data["user_id"], filter: "nickname fullname status email roles" });
    
  if (profile_response_status != 200 || !profile_response) open_webclient_ref("primary");
  else {
    set_id(load_data["user_id"], true);
    set_email(profile_response["email"]);
    set_nickname(profile_response["nickname"]);
    set_fullname(profile_response["fullname"]);
    set_status(profile_response["status"]);
    set_roles(profile_response["roles"]);
  }

  // Выход
  logoutButton.addEventListener("click", () => {
    send_notification("info", "Производится выход из системы");
    setTimeout(() => { open_webclient_ref("logout"); }, 1500);
  });
  // Полный выход
  logoutAllButton.addEventListener("click", () => {
    send_notification("info", "Выход со всех устройств выполняется");
    setTimeout(() => { open_webclient_ref("logout?all=true"); }, 1500);
  });
  // Удаление аккаунта
  deleteAccountButton.addEventListener("click", () => {
    const confirmDelete = confirm("Вы уверены, что хотите удалить аккаунт? Это действие необратимо.");
    if (confirmDelete) {
      send_notification("error", "Аккаунт не удалён.");
      setTimeout(() => {
        location.href = "/";
      }, 2000);
    }
  });

  // Бан
  blockButton.addEventListener("click", async () => {
    // Выбирает статус
    const target_status = blockButton.classList.contains("unblock") ? "Offline" : "Blocked";
    // Делает запрос
    const [status, response] = await post_to_webclient("data/write",
      { source: "users/status" },
      { user_id: load_data["user_id"], status: target_status });
    if (status != 200) {
      send_notification("error", response && response["message"] ? response["message"] : "Ошибка при получении новостей");
      return;
    }
    // Отправка успеха
    send_notification("success", target_status == "Blocked" ? "Пользователь заблокирован." : "Пользователь разблокирован.");
    // Смена статуса и кнопки
    set_status(target_status);
  });

  // Открытие/Закрытие секций
  // Новости
  newsButton.addEventListener("click", async () => {
    if (newsDropdown.is_open == false) {
      // Отправление запроса
      console.log("Открываем новости");
      const [status, response] = await get_from_webclient("data/read",
        { source: "news/list/recipient", user_id: load_data["user_id"]});
      if (status != 200) {
        send_notification("error", response && response["message"] ? response["message"] : "Ошибка при получении новостей");
        return;
      }
      // Возвращает news массив с news_id, status, text, sender_id, sender_fullname

      // Очищаем список новостей
      newsDropdown.innerHTML = "";
      // Наполняем полученными из запроса
      for (const news of response["news"]) {
        // Создаёт
        const item = createNews(news["news_id"], news["sender_id"], news["sender_fullname"], news["text"]);
        // Добавляет
        newsDropdown.append(item);
      }

      newsDropdown.style.display = "block";
      newsDropdown.is_open = true;
    }
    else {
      newsDropdown.style.display = "none";
      newsDropdown.is_open = false;
    }
  });
  // Дисциплины
  toggleDisciplinesButton.addEventListener("click", async () => {
    if (disciplinesList.is_open == false) {
      // Получаем данные
      const [status, response] = await get_from_webclient("data/read",
        { source: "users/data", user_id: load_data["user_id"], filter: "disciplines" });
      // Проверяем на ошибку
      if (status != 200 || !response) {
        send_notification("error", response["message"] ? response["message"] : "Ошибка при получении дисциплин пользователя.");
        return;
      }
      // Возвращается: disciplines массив с discipline_id, title, status, teachers

      // Очищаем список дисциплин
      disciplinesList.innerHTML = "";
      console.log(response["disciplines"]);
      // Наполняем полученными из запроса
      for (const discipline of response["disciplines"]) {
        // Создаёт
        const item = createDiscipline(discipline["discipline_id"], discipline["title"]);
        // Добавляет
        disciplinesList.append(item);
      }

      // Открываем список
      set_section_toggle(disciplinesList, toggleDisciplinesButton, "дисциплины", true);
    } else {
      set_section_toggle(disciplinesList, toggleDisciplinesButton, "дисциплины", false);
    }
  });
  // Тесты
  toggleTestsButton.addEventListener("click", async () => {
    if (testsContainer.is_open == false) {
      // Получаем данные
      const [status, response] = await get_from_webclient("data/read",
        { source: "users/data", user_id: load_data["user_id"], filter: "tests" });
      // Проверяем на ошибку
      if (status != 200 || !response) {
        send_notification("error", response["message"] ? response["message"] : "Ошибка при получении тестов пользователя.");
        return;
      }
      // Возвращается: tests массив с test_id, test_discipline_id, test_title, test_discipline_title,
      // test_status, test_discipline_status

      // Очищаем список дисциплин
      testsContainer.innerHTML = "";
      // Наполняем полученными из запроса
      for (const test of response["tests"]) {
        // Создаёт
        const item = createTest(test["test_id"], test["test_title"], test["test_discipline_id"], test["test_discipline_title"]);
        // Добавляет
        testsContainer.append(item);
      }

      // Открываем список
      set_section_toggle(testsContainer, toggleTestsButton, "пройденные тесты", true);
    } else {
      set_section_toggle(testsContainer, toggleTestsButton, "пройденные тесты", false);
    }
  });
  // Оценки (Попытки прохождения)
  toggleGradesButton.addEventListener("click", async () => {
    if (gradesContainer.is_open == false) {
      // Получаем данные
      const [status, response] = await get_from_webclient("data/read",
        { source: "users/data", user_id: load_data["user_id"], filter: "tries" });
      // Проверяем на ошибку
      if (status != 200 || !response) {
        send_notification("error", response["message"] ? response["message"] : "Ошибка при получении оценок пользователя.");
        return;
      }
      // Возвращается: tries массив с try_id, test_id, test_discipline_id, test_title, test_discipline_title,
      // test_status, test_discipline_status, status,           points, max_points, score_percent

      // Очищаем список дисциплин
      gradesContainer.innerHTML = "";
      // Наполняем полученными из запроса
      for (const tri of response["tries"]) {
        // Создаёт
        const item = createGrade(tri["test_id"], tri["try_id"], 
          tri["test_title"], `${tri["points"]}/${tri["max_points"]} (${tri["score_percent"]}%)`);
        // Добавляет
        gradesContainer.append(item);
      }

      // Открываем список
      set_section_toggle(gradesContainer, toggleGradesButton, "оценки за тесты", true);
    } else {
      set_section_toggle(gradesContainer, toggleGradesButton, "оценки за тесты", false);
    }
  });

  // Модальное окно (для Изменений Никнейма, ФИО, Ролей)
  // Обработчик каждой кнопки «Изменить»
  editButtons.forEach(button => {
    button.addEventListener("click", () => {
      // Какой параметр редактируем
      const field = button.getAttribute("data-field");
      // Ставим заголовок
      switch (field) {
        case "fullname":
          modalTitle.textContent = "Редактирование ФИО (Впишите через пробел)";
          break;
        case "nickname":
          modalTitle.textContent = "Редактирование никнейма";
          break;
        case "roles":
          modalTitle.textContent = "Редактирование ролей (Впишите через запятую)";
          break;
        // Если необрабатываемый тип - прекращаем работу
        default: return;
      }
      // Ставит изначальным значением уже сохранённое
      const elId = `${field}Text`;
      modalInput.value = document.getElementById(elId).textContent;
      // Открытие модального окна
      modalOverlay.style.display = "flex";
      // Ставим метод кнопке сохранения изменений
      modalSaveButton.onclick = async () => {
        // В зависимости от типа параметра получаем тело для запроса
        let body = {};
        if (field == "fullname") {
          // Разбивает значение по пробелам
          let parts = modalInput.value.split(" ");
          // Проверяет, чтобы частей было 3
          if (parts.length != 3) {
            send_notification("error", "Неверный формат ФИО");
            return;
          }
          // Убираем пробелы, Проверяем на пустые
          for (let i = 0; i < parts.length; i++) {
            parts[i] = parts[i].trim();
            if (parts[i].length == 0) {
              send_notification("error", "Неверный формат ФИО");
              return;
            }
          }
          // Получает ФИО
          body = { last_name: parts[0], first_name: parts[1], patronimyc: parts[2] };
        }
        else if (field == "roles") {
          // Разбивает значение по запятым
          let parts = modalInput.value.split(",");
          // Убираем пробелы, Проверяем на пустые
          for (let i = 0; i < parts.length; i++) {
            parts[i] = parts[i].trim();
            if (parts[i].length == 0) {
              send_notification("error", "Неверный формат ролей");
              return;
            }
          }
          body = { roles: parts };
        }
        else if (field == "nickname") {
          // Получаем ник
          const value = modalInput.value.trim();
          if (value.length == 0) {
            send_notification("error", "Неверный формат никнейма");
            return;
          }
          body = { nickname: value };
        }

        // Делаем запрос
        const [status, response] = await post_to_webclient("data/write",
          { source: `users/${field}` },
          { user_id: load_data["user_id"], ...body })
        // Обработка ошибки
        if (status != 200) {
          send_notification("error", response && response["message"] ? response["message"] : "Ошибка при изменении данных");
          return;
        }
        // При успехе ставит успешные значения
        switch (field) {
          case "fullname":
            set_fullname(`${body["last_name"]} ${body["first_name"]} ${body["patronimyc"]}`);
            break;
          case "nickname":
            set_nickname(body["nickname"]);
            break;
          case "roles":
            set_roles(body["roles"]);
            break;
        }
        // Сообщает
        send_notification("success", "Изменения применены.");
        // Закрывает модаль
        modalOverlay.style.display = "none";
      };
    });
  });
  // Закрытие модального окна по кнопке «Отмена»
  modalCancelButton.addEventListener("click", () => {
    modalOverlay.style.display = "none";
  });
});
