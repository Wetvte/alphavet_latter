// Информация о дисциплине
let disciplineTitleText;
let disciplineDescText;

function set_title(title) {
  disciplineTitleText.textContent = title;
}
function set_describtion(desc) {
  disciplineDescText.textContent = desc;
}

// Создание итемов
// Преподаватели
function createTeacher(id, fullname) {
  const newItem = document.createElement('div');
  newItem.classList = "section-item teacher";
  newItem.innerHTML =
    `<span class="section-text teacher-name">${fullname} (${id})</span>
  <div class="user-buttons">
   <button class="button go">Профиль</button>
   <button class="button delete">Удалить</button>
  </div>`;
  // Устанавливаем обработчик профиля
  newItem.querySelector(".button.go").addEventListener('click', () => {
    open_webclient_ref("cabinet?user_id=" + id);
  });
  newItem.querySelector(".button.delete").addEventListener('click', async () => {
    // Подтверждаем
    const confirmed = confirm(`Вы действительно хотите удалить ${fullname} из дисциплины?`);
    if (!confirmed) return;
    // Отправляем запрос
    const [status, response] = await post_to_webclient("data/write",
      { source: "disciplines/users/remove" },
      { discipline_id: load_data["discipline_id"], user_id: id });
    // Обрабатывает ошибку
    if (status != 200) {
      send_notification("error", response && response["message"] ? response["message"] : "Ошибка при удалении преподавателя.");
      return;
    }
    // Удаляем на странице
    newItem.style.opacity = '0';
    newItem.style.transform = 'translateX(20px)';
    setTimeout(() => {
      newItem.remove();
    }, 500);
  });
  return newItem;
}
function createTeacherAdd() {
  const newItem = document.createElement('div');
  newItem.classList = "section-item teacher-add";
  newItem.innerHTML =
    `<span class="section-text">Добавить нового преподавателя</span>
   <button id="addButtonTeacher" class="button add">Добавить</button>`;

  // Открытие в режиме добавления преподавателя
  newItem.addEventListener('click', () => {
    modalOverlay.style.display = 'flex';
    modalContent.className = 'modal-content mode-add-teacher';
    modalTitle.textContent = 'Добавление преподавателя';
  });
  return newItem;
}
// Тесты
function createTest(id, title, status) {
  console.log("Creating test ",status);
  let innerHTML = `<span class="section-text title">${title} (${id})</span>\n`;
  switch (status) {
    case "Activated":
      innerHTML += `<div class="test-actions">
                      <span class="status-indicator activated">Активен</span>
                      <button class="button change status deactivate">Деактивировать</button>
                      <button class="button edit">Смотреть</button>
                      <button class="button delete">Удалить</button>
                   </div>`;
      break;
    case "Deactivated":
      innerHTML += `<div class="test-actions">
                      <span class="section-text status-indicator deactivated">Деактивен</span>
                      <button class="button go status activate">Активировать</button>
                      <button class="button edit">Смотреть</button>
                      <button class="button delete">Удалить</button>
                   </div>`;
      break;
    case "Deleted":
      innerHTML += `<span class="section-text status-indicator deleted">Удалён</span>`;
      break;
    default: return;
  }

  const newItem = document.createElement('div');
  newItem.classList = "section-item test";
  newItem.innerHTML = innerHTML;

  if (status !== "Deleted") {
    // Обработчик редактирования
    newItem.querySelector(".button.edit").addEventListener('click', () => {
      open_webclient_ref("test?test_id="+id);
    });
    // Устанавливаем обработчики активации/деактивации
    const status_button = newItem.querySelector(".button.status");
    const status_element = newItem.querySelector(".status-indicator");
    status_button.addEventListener('click', async () => {
      // Выбирает нужный статус
      const target_status = status_button.classList.contains("deactivate") ? "Deactivated" : "Activated";
      // Отправляет запрос
      const [status, response] = await post_to_webclient("data/write", { source: "tests/status" }, { test_id: id, status: target_status });
      // Обрабатывает ошибку
      if (status != 200) {
        send_notification("error", response && response["message"] ? response["message"] : "Ошибка при изменении статуса теста");
        return;
      }
      // Устанавливает нужный статус
      if (target_status == "Activated") {
        status_button.textContent = "Деактивировать";
        status_button.classList = "button change status deactivate";
        status_element.textContent = "Активен";
        status_element.classList = "section-text status activated";
      }
      else {
        status_button.textContent = "Активировать";
        status_button.classList = "button go status activate";
        status_element.textContent = "Деактивен";
        status_element.classList = "section-text status deactivated";
      }
    });
    // Устанавливаем удаление
    newItem.querySelector(".button.delete").addEventListener('click', async () => {
      // Подтверждаем
      const confirmed = confirm(`Вы действительно хотите удалить тест "${title}" из дисциплины?`);
      if (!confirmed) return;
      const [status, response] = await post_to_webclient("data/write", { source: "tests/status" }, { test_id: id, status: "Deleted" });
      // Обрабатывает ошибку
      if (status != 200) {
        send_notification("error", response && response["message"] ? response["message"] : "Ошибка при удалении теста");
        return;
      }
      // Удаляет из списка
      newItem.style.opacity = '0';
      newItem.style.transform = 'translateX(20px)';
      setTimeout(() => {
        newItem.remove();
      }, 500);
    });
  }

  return newItem;
}
function createTestAdd() {
  const newItem = document.createElement('div');
  newItem.classList = "section-item test-add";
  newItem.innerHTML =
    `<span class="section-text">Добавить тест</span>
   <button id="addButtonTest" class="button add">Добавить</button>`;
  // Открытие в режиме добавления теста
  newItem.addEventListener('click', () => {
    modalOverlay.style.display = 'flex';
    modalContent.className = 'modal-content mode-add-test';
    modalTitle.textContent = 'Добавление теста';
  });

  return newItem;
}
// Студенты
function createStudent(id, fullname) {
  const newItem = document.createElement('div');
  newItem.classList = "section-item student";
  newItem.innerHTML =
    `<span class="section-text name">${fullname} (${id})</span>
   <div class="user-buttons">
   <button class="button go">Профиль</button>
   <button class="button delete">Удалить</button>
  </div>`;
  // Устанавливаем обработчик профиля
  newItem.querySelector(".button.go").addEventListener('click', () => {
    open_webclient_ref("cabinet?user_id=" + id);
  });
  // Устанавливаем обработчик удаления
  newItem.querySelector(".button.delete").addEventListener('click', async () => {
    // Подтверждаем
    const confirmed = confirm(`Вы действительно хотите удалить ${fullname} из дисциплины?`);
    if (!confirmed) return;
    // Отправляем запрос
    const [status, response] = await post_to_webclient("data/write",
      { source: "disciplines/users/remove" },
      { discipline_id: load_data["discipline_id"], user_id: id });
    // Обрабатывает ошибку
    if (status != 200) {
      send_notification("error", response && response["message"] ? response["message"] : "Ошибка при удалении студента.");
      return;
    }
    // Удаляет из списка
    newItem.style.opacity = '0';
    newItem.style.transform = 'translateX(20px)';
    setTimeout(() => {
      newItem.remove();
    }, 500);
  });
  return newItem;
}

// По загрузке страницы
document.addEventListener('DOMContentLoaded', async () => {
  // Переменные
  // Информация
  disciplineTitleText = document.getElementById("disciplineTitleText");
  disciplineDescText = document.getElementById("disciplineDescText");
  // Преподаватели
  const toggleTeachersButton = document.getElementById('toggleTeachersButton');
  const teachersList = document.getElementById('teachersList');
  // Студенты
  const toggleStudentsButton = document.getElementById('toggleStudentsButton');
  const studentsList = document.getElementById('studentsList');
  // Тесты
  const toggleTestsButton = document.getElementById('toggleTestsButton');
  const testsList = document.getElementById('testsList');
  // Модальное окно редактирования - Основа
  const modalOverlay = document.getElementById('modalOverlay');
  const modalContent = document.getElementById('modalContent');
  const modalTitle = document.getElementById('modalTitle');
  const modalCancelButton = document.getElementById('modalCancelButton');
  // Модальное окно редактирования - Поля ввода
  const disciplineTitleInput = document.getElementById('disciplineTitleInput');
  const disciplineDescInput = document.getElementById('disciplineDescInput');
  const teacherIdInput = document.getElementById('teacherIdInput');
  const testInput = document.getElementById('testInput');
  const studentIdInput = document.getElementById('studentIdInput');
  // Модальное окно редактирования - Применение изменений
  const modalDsiciplineChangesSaveButton = document.getElementById('modalDsiciplineChangesSaveButton');
  const modalAddTeacherButton = document.getElementById('modalAddTeacherButton');
  const modalAddnewTestButton = document.getElementById('modalAddnewTestButton');
  const modalAddTestButton = document.getElementById('modalAddTestButton');
  // Кнопки, открывающие модалку
  const editInfoButton = document.getElementById('editInfoButton');          // Кнопка "Редактировать информацию"
  const addButtonTeacher = document.getElementById('addButtonTeacher'); // Кнопка для добавления преподавателя
  const addButtonTest = document.getElementById('addButtonTest'); // Кнопка для добавления теста
  // Кнопка зачисления на дисциплину
  const subscribeButton = document.getElementById('subscribeButton');

  console.log("Дисциплина");
  set_section_toggle(teachersList, toggleTeachersButton, "преподавателей", false);
  set_section_toggle(studentsList, toggleStudentsButton, "студентов", false);
  set_section_toggle(testsList, toggleTestsButton, "тесты", false);

  // Иницилизация страницы - инициализация через запрос
  const [info_response_status, info_response] = await get_from_webclient("data/read",
    { source: "disciplines/data", discipline_id: load_data["discipline_id"], filter: "text status" });
  if (info_response_status != 200 || !info_response) open_webclient_ref('primary');
  else {
    set_title(info_response["title"] + (info_response["status"] === "Deleted" ? " (Удалена)" : ""));
    set_describtion(info_response["describtion"]);
  }

  // Открытие модалки
  // Открытие в режиме редактирования дисциплины
  editInfoButton?.addEventListener('click', () => {
    modalOverlay.style.display = 'flex';
    modalContent.className = 'modal-content mode-edit';
    modalTitle.textContent = 'Редактирование дисциплины';
    disciplineTitleInput.value = disciplineTitleText.textContent;
    disciplineDescInput.value = disciplineDescText.textContent;
  });
  // Открытие в режиме добавления теста
  addButtonTest?.addEventListener('click', () => {
    modalOverlay.style.display = 'flex';
    modalContent.className = 'modal-content mode-add-test';
    modalTitle.textContent = 'Добавление теста';
  });
  // Открытие в режиме добавления преподавателя
  addButtonTeacher?.addEventListener('click', () => {
    modalOverlay.style.display = 'flex';
    modalContent.className = 'modal-content mode-add-teacher';
    modalTitle.textContent = 'Добавление преподавателя';
  });
  // Закрытие модалки
  modalCancelButton.addEventListener('click', () => {
    modalOverlay.style.display = 'none';
  });

  // Сохранение изменений (редактирование дисциплины)
  modalDsiciplineChangesSaveButton.addEventListener('click', async () => {
    const title = disciplineTitleInput.value;
    const desc = disciplineDescInput.value;
    if (title == "" || desc == "") {
      send_notification("error", "Нельзя оставлять поля пустыми!");
      return;
    }

    // Отправка запроса
    const [status, response] = await post_to_webclient("data/write",
      { source: "disciplines/text" },
      { discipline_id: load_data["discipline_id"], title: title, describtion: desc });
    // Обрабатывает ошибку
    if (status != 200) {
      send_notification("error", response && response["message"] ?
        response["message"] : "Ошибка при изменении информации дисциплины.");
      return;
    }

    // Устанавливаем
    set_title(title);
    set_describtion(desc);
    // Закрываем
    modalOverlay.style.display = 'none';
    send_notification("success", 'Дисциплина обновлена!');
  });
  // Добавление преподавателя
  modalAddTeacherButton.addEventListener('click', async () => {
    const id = teacherIdInput.value;
    if (!id || id == "") {
      send_notification("error", "Нельзя оставлять поле пустым!");
      return;
    }

    // Отправка запроса
    const [status, response] = await post_to_webclient("data/write",
      { source: "disciplines/users/add" },
      { discipline_id: load_data["discipline_id"], user_id: id, user_type: "teacher" });
    // Обрабатывает ошибку
    if (status != 200) {
      send_notification("error", response && response["message"] ? response["message"] : "Ошибка при добавлении преподавателя.");
      return;
    }

    if (teachersList.is_open == true) createTeacher(id, ""); // Впадлу делать запрос на полное имя

    // Сообщает и Закрывает
    modalOverlay.style.display = 'none';
    send_notification("success", 'Преподаватель добавлен!');
  });
  // Добавление теста - Новый
  modalAddnewTestButton.addEventListener('click', async () => {
    const title = testInput.value;
    if (!title || title == "") {
      send_notification("error", "Нельзя оставлять поле пустым!");
      return;
    }

    // Отправка запроса
    const [status, response] = await post_to_webclient("data/write",
      { source: "disciplines/tests/add" },
      { discipline_id: load_data["discipline_id"], title: title });
    // Обрабатывает ошибку
    if (status != 200) {
      send_notification("error", response && response["message"] ? response["message"] : "Ошибка при добавлении теста.");
      return;
    }
    // Вернёт test_id, title, status
  
    // Добавляет
    if (testsList.is_open == true) {
      testsList.append(createTest(response["test_id"], response["title"], response["status"]));
    }
    // Сообщает
    modalOverlay.style.display = 'none';
    send_notification("success", 'Тест добавлен!');
  });
  // Добавление теста - по айди
  modalAddTestButton.addEventListener('click', async () => {
    const id = testInput.value;
    if (!id || id == "") {
      send_notification("error", "Нельзя оставлять поле пустым!");
      return;
    }

    // Отправка запроса
    const [status, response] = await post_to_webclient("data/write",
      { source: "disciplines/tests/add" },
      { discipline_id: load_data["discipline_id"], test_id: id });
    // Обрабатывает ошибку
    if (status != 200) {
      send_notification("error", response && response["message"] ? response["message"] : "Ошибка при добавлении теста.");
      return;
    }
    // Вернёт test_id, title, status

    // Добавляет
    if (testsList.is_open == true) {
      testsList.append(createTest(response["test_id"], response["title"], response["status"]));
    }
    // Сообщает
    modalOverlay.style.display = 'none';
    send_notification("success", 'Тест добавлен!');
  });

  // Добавление студента (зачисление)
  subscribeButton.addEventListener('click', async () => {
    // Отправка запроса
    const [status, response] = await post_to_webclient("data/write",
      { source: "disciplines/users/add" },
      { discipline_id: load_data["discipline_id"], user_type: "student" });
    // Обрабатывает ошибку
    if (status != 200) {
      send_notification("error", response && response["message"] ? response["message"] : "Ошибка при записи.");
      return;
    }

    if (studentsList.is_open == true) createStudent(id, ""); // Впадлу делать запрос на полное имя

    modalOverlay.style.display = 'none';
    send_notification("success", 'Вы зачислены на дисциплину!');
  });

  // Переключение видимости списка студентов
  // Преподаватели
  toggleTeachersButton.addEventListener('click', async () => {
    if (teachersList.is_open == false) {
      // Получаем данные
      const [status, response] = await get_from_webclient("data/read",
        { source: "disciplines/data", discipline_id: load_data["discipline_id"], filter: "teacherslist" });
      // Проверяем на ошибку
      if (status != 200) {
        send_notification("error", response["message"] ? response["message"] : "Ошибка при получении преподавателей.");
        return;
      }
      // Возвращается: teachers массив с user_id, fullname

      // Очищаем список
      teachersList.innerHTML = "";
      teachersList.append(createTeacherAdd());
      // Наполняем полученными из запроса
      for (const teacher of response["teachers"]) {
        // Создаёт и Добавляет
        const item = createTeacher(teacher["user_id"], teacher["fullname"]);
        teachersList.append(item);
      }

      // Открываем список
      set_section_toggle(teachersList, toggleTeachersButton, "преподавателей", true);
    } else {
      set_section_toggle(teachersList, toggleTeachersButton, "преподавателей", false);
    }
  });
  // Тесты
  toggleTestsButton.addEventListener('click', async () => {
    if (testsList.is_open == false) {
      // Получаем данные
      const [status, response] = await get_from_webclient("data/read",
        { source: "disciplines/data", discipline_id: load_data["discipline_id"], filter: "testslist" });
      // Проверяем на ошибку
      if (status != 200) {
        send_notification("error", response["message"] ? response["message"] : "Ошибка при получении тестов.");
        return;
      }
      // Возвращается: tests массив с test_id, title, status

      // Очищаем список
      testsList.innerHTML = "";
      testsList.append(createTestAdd());
      // Наполняем полученными из запроса
      for (const test of response["tests"]) {
        // Создаёт и Добавляет
        console.log(test["status"]);
        const item = createTest(test["test_id"], test["title"], test["status"]);
        testsList.append(item);
      }

      // Открываем список
      set_section_toggle(testsList, toggleTestsButton, "тесты", true);
    } else {
      set_section_toggle(testsList, toggleTestsButton, "тесты", false);
    }
  });
  // Студенты
  toggleStudentsButton.addEventListener('click', async () => {
    if (studentsList.is_open == false) {
      // Получаем данные
      const [status, response] = await get_from_webclient("data/read",
        { source: "disciplines/data", discipline_id: load_data["discipline_id"], filter: "studentslist" });
      // Проверяем на ошибку
      if (status != 200) {
        send_notification("error", response["message"] ? response["message"] : "Ошибка при получении студентов.");
        return;
      }
      // Возвращается: students массив с user_id, fullname

      // Очищаем список
      studentsList.innerHTML = "";
      // Наполняем полученными из запроса
      for (const student of response["students"]) {
        // Создаёт и Добавляет
        const item = createStudent(student["user_id"], student["fullname"]);
        studentsList.append(item);
      }

      // Открываем список
      set_section_toggle(studentsList, toggleStudentsButton, "студентов", true);
    } else {
      set_section_toggle(studentsList, toggleStudentsButton, "студентов", false);
    }
  });
});